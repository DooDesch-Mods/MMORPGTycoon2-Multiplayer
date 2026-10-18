#include "ui_region_runtime.h"
#include "region_ui_readback.h"
#include "region_binding.h"
#include "region_saved_identity.h"
#include "typed_action_runtime.h"
#include "remove_diagnostics.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "object_identity/remove_runtime/copy/scenery_witness.h"
#include "connected_network_reader.h"
#include "ui_path_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include "object_identity/remove_runtime/copy/undo_witness.h"
#include "region_level_reader.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/logger.h"
#include <Windows.h>
#include "mt2mp/hooks/minhook_backend.h"
#include <bcrypt.h>
#include <intrin.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <set>

namespace mt2mp::game::typed_action_runtime {
bool submit_region(const std::string&,const action_session::RegionIntent&,std::string&);
bool submit_region_property(const std::string&,const action_session::RegionPropertyIntent&,std::string&);
}
namespace mt2mp::game::ui_region_runtime {
namespace {
namespace action=action_session;
namespace host=build_actions::private_native::host;
namespace place=build_actions::private_native::place;
using json=nlohmann::json;
using UICommand=bool(__fastcall*)(void*,void*);
using SetPreset=void(__fastcall*)(void*,const void*);
constexpr const char* exact_exe="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
struct NativeString {const char* data;size_t length;unsigned char rest[16]{};};
struct LocalView {
    host::WorldState world;
    uintptr_t region=0,random=0;
    action::RegionState state;
    std::vector<uint64_t> scenery_uids;
    std::vector<uint64_t> building_uids,path_uids,path_node_uids,network_uids;
};
struct Operation {
    action::Binding binding;action::Request request;action::Evidence evidence;
    bool attempted=false,returned=false,confirmed=false,retired=false;
    bool edit_opened=false,edit_ended=false,edit_end_attempted=false;uint32_t style_calls=0,edit_cleanup_exception=0;int32_t edit_depth_before=0,edit_depth_after=0;
    std::vector<uint64_t> removed_scenery_uids;
    std::vector<std::string> retired_keys;
    std::string cause;
};
struct Runtime {
    uint32_t thread=GetCurrentThreadId();uintptr_t image=0;
    bool install_attempted=false,hook_created=false;std::atomic<bool> ready{false};
    std::atomic<UICommand> original_ui{nullptr};
    std::atomic<SetPreset> original{nullptr};host::ProcessMemory memory;host::LookupApi api;
    std::optional<action::RegionIntent> pending;std::string pending_snapshot,last_action,cause;
    action::Binding pending_binding;
    uint64_t captured=0,rejected=0,submitted=0,forwarded=0;
    std::map<std::string,Operation> operations;
    std::set<std::string> ui_attempts;
    json last_ui;
};
std::atomic<Runtime*> published{nullptr};
bool property_pending();
bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class T>bool at(Runtime& r,uintptr_t address,size_t off,T& value,std::string& e){
    if(!address||address>UINTPTR_MAX-off)return fail(e,"region read address overflow");
    return r.memory.copy(address+off,&value,sizeof value,e);
}
bool text(Runtime& r,uintptr_t address,std::string& out,std::string& e) {
    NativeString value{};
    if(!at(r,address,0,value,e))return false;
    if(!value.data||!value.length||value.length>256)return fail(e,"region GNU string requires 1..256 bytes");
    out.resize(value.length);
    if(!r.memory.copy(uintptr_t(value.data),out.data(),out.size(),e))return false;
    if(out.find('\0')!=std::string::npos)return fail(e,"region GNU string contains NUL");return true;
}
bool world(Runtime& r,host::WorldState& out,std::string& e) {
    if(GetCurrentThreadId()!=r.thread)return fail(e,"region requires bound Mainthread");
    host::NativeOrigin origin;host::Reader reader(r.memory,origin,r.image,r.thread,r.api);
    return reader.current(out,true,e);
}
bool identity(Runtime& r,int32_t id,uintptr_t expected,LocalView& v,std::string& e) {
    if(id<0||!world(r,v.world,e))return false;
    uintptr_t map=0;if(!at(r,r.image,0x13df8c0,map,e))return false;
    const auto read_field=[&](uintptr_t address,size_t offset,auto& value,std::string& error){
        return at(r,address,offset,value,error);
    };
    if(!region_saved_identity::resolve(r.image,map,v.world.root,id,expected,v.region,read_field,e))return false;
    v.state.id=id;return true;
}
bool sha256(const std::string& input,std::string& out,std::string& e) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    auto result=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    if(result)return fail(e,"region SHA256 provider NTSTATUS="+std::to_string(result));
    std::array<unsigned char,32> digest{};
    result=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0);
    if(!result)result=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),ULONG(input.size()),0);
    if(!result)result=BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0);
    const auto hash_cleanup=hash?BCryptDestroyHash(hash):0;
    const auto algorithm_cleanup=BCryptCloseAlgorithmProvider(algorithm,0);
    if(result||hash_cleanup||algorithm_cleanup)return fail(e,"region SHA256 failure NTSTATUS="+std::to_string(result)+" hash_cleanup="+std::to_string(hash_cleanup)+" provider_cleanup="+std::to_string(algorithm_cleanup));
    out.clear();out.reserve(64);constexpr char hex[]="0123456789abcdef";
    for(auto byte:digest){out+=hex[byte>>4];out+=hex[byte&15];}return true;
}
uint32_t find(host::FindUid fn,uint64_t uid,uintptr_t& object) {
    __try{object=uintptr_t(fn(uid));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct SceneryPort final:place::SceneryHost {
    Runtime& runtime;const LocalView& view;uintptr_t manager;
    SceneryPort(Runtime& r,const LocalView& v,uintptr_t m):runtime(r),view(v),manager(m){}
    bool current(place::SceneryContext& out,std::string& e)override {
        host::WorldState actual;if(!world(runtime,actual,e))return false;
        if(!host::same_world(actual.origin,view.world.origin)||actual.root!=view.world.root)return fail(e,"region scenery current world changed");
        out={actual.origin.world_epoch,view.region,actual.root,manager,true,true,true};return true;
    }
    bool registry_object(uint64_t uid,uintptr_t& object,std::string& e)override {
        const auto seh=find(runtime.api.find,uid,object);
        return !seh||fail(e,"region scenery UID lookup SEH="+std::to_string(seh));
    }
};
bool read(Runtime& r,int32_t id,uintptr_t expected,LocalView& out,std::string& e) {
    LocalView v;if(!identity(r,id,expected,v,e))return false;
    uintptr_t styles=0,manager=0;int32_t count=0,capacity=0;
    if(!text(r,v.region+0x8b0,v.state.preset,e)||!at(r,v.region,0x860,styles,e)||!at(r,v.region,0x868,count,e)||!at(r,v.region,0x86c,capacity,e))return false;
    if(!styles||count!=4||capacity<count)return fail(e,"region style array requires four entries");
    for(size_t i=0;i<4;++i)if(!text(r,styles+i*32,v.state.styles[i],e))return false;
    if(!at(r,v.region,0x160,v.random,e)||!v.random||!at(r,v.random,0,v.state.random,e)||!at(r,v.region,0x960,manager,e))return false;
    // The grid stores eight-byte cells. Only the scalar style member participates in this action.
    uintptr_t cells=0;int32_t width=0,height=0,cell_count=0,cell_capacity=0;
    if(!at(r,v.region,0xc0,width,e)||!at(r,v.region,0xc4,height,e)||!at(r,v.region,0xd8,cells,e)||!at(r,v.region,0xe0,cell_count,e)||!at(r,v.region,0xe4,cell_capacity,e))return false;
    if(width<1||height<1||int64_t(width)*height>1000000||cell_count!=int64_t(width)*height||cell_capacity<cell_count||!cells)return fail(e,"region grid cell dimensions or array differ");
    json grid={{"width",width},{"height",height},{"styles",json::array()}};
    for(int32_t i=0;i<cell_count;++i){int32_t style=-1;if(!at(r,cells,size_t(i)*8,style,e))return false;grid["styles"].push_back(style);}
    if(!sha256(grid.dump(),v.state.grid_sha256,e))return false;
    SceneryPort port(r,v,manager);place::SceneryWitness scenery;
    if(!place::read_scenery(r.memory,port,r.image,scenery,e))return false;
    if(scenery.members.size()>1000000)return fail(e,"region scenery count exceeds 1000000");
    std::vector<std::string> rows;rows.reserve(scenery.members.size());
    for(const auto& member:scenery.members){int32_t variant=0;
        if(!at(r,member.object,0x26c,variant,e))return false;
        const auto& t=member.transform;
        rows.push_back(json::array({member.category,variant,t.position.x,t.position.y,t.position.z,
            t.rotation[0],t.rotation[1],t.rotation[2],t.rotation[3],t.scale.x,t.scale.y,t.scale.z}).dump());
        v.scenery_uids.push_back(member.uid);
    }
    std::sort(rows.begin(),rows.end());std::sort(v.scenery_uids.begin(),v.scenery_uids.end());
    if(!sha256(json(rows).dump(),v.state.scenery_sha256,e))return false;
    v.state.scenery_count=rows.size();
    host::WorldState after;if(!world(r,after,e))return false;
    if(!host::same_world(v.world.origin,after.origin))return fail(e,"region world changed during readback");
    out=std::move(v);return true;
}
bool bound(const LocalView& v,const action::Binding& binding,const std::string& snapshot,std::string& e) {
    return region_binding::validate(v.world.origin,binding,net::current_snapshot_id(),snapshot,e);
}
bool ledger(action::Ledger& out,std::string& e) {
    native::SimulationState s;if(!native::read_simulation_state(s,e))return false;
    out={s.cash,s.clock.flat_day,int32_t(s.record_present),s.record_day,s.record_cash,s.record_income,s.record_expense,s.line_counts,s.line_income};return true;
}
uint32_t preset_lookup(uintptr_t image,uintptr_t manager,const NativeString* value,uintptr_t& preset) {
    __try{preset=uintptr_t(reinterpret_cast<void*(*)(void*,const void*)>(image+0x2c9d40)(reinterpret_cast<void*>(manager),value));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool expected_styles(Runtime& r,const action::RegionIntent& intent,const LocalView& before,std::array<std::string,4>& styles,std::string& e) {
    styles=before.state.styles;
    if(intent.preset==before.state.preset||intent.preset=="{region_preset_custom}")return true;
    NativeString string{intent.preset.data(),intent.preset.size(),{}};uintptr_t manager=0,preset=0,data=0;int32_t count=0,capacity=0;
    if(!at(r,r.image,0x13df7f0,manager,e)||!manager)return fail(e,"region preset manager unavailable");
    const auto seh=preset_lookup(r.image,manager,&string,preset);
    if(seh||!preset)return fail(e,"region preset lookup failed SEH="+std::to_string(seh));
    if(!at(r,preset,0x60,data,e)||!at(r,preset,0x68,count,e)||!at(r,preset,0x6c,capacity,e))return false;
    if(!data||count!=4||capacity<count)return fail(e,"region preset catalog requires four styles");
    for(size_t i=0;i<4;++i)if(!text(r,data+i*32,styles[i],e))return false;return true;
}
uint32_t invoke(SetPreset fn,uintptr_t region,const NativeString* string,uintptr_t random,const uint64_t* authority_random,bool& returned) {
    __try {
        if(authority_random)std::memcpy(reinterpret_cast<void*>(random),authority_random,32);
        fn(reinterpret_cast<void*>(region),string);returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::string uuid(std::string& e){
    unsigned char b[16];const auto status=BCryptGenRandom(nullptr,b,sizeof b,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(status){e="region UUID NTSTATUS="+std::to_string(status);return {};}
    b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;char value[37];
    std::snprintf(value,sizeof value,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);return value;
}
void rejected(Runtime& r,const std::string& e){++r.rejected;if(r.cause!=e){r.cause=e;LOG_ERROR("[UIRegion] Rejected cause={}",e);}}
void preset_entry(void* region,const void* value) {
    auto* r=published.load(std::memory_order_acquire);const auto caller=uintptr_t(_ReturnAddress());
    if(!r->ready||!net::session_wire::connected()||caller!=r->image+0x797c7e){++r->forwarded;r->original.load(std::memory_order_acquire)(region,value);return;}
    std::string e;const auto controller=typed_action_runtime::status();
    if(GetCurrentThreadId()!=r->thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||r->pending||property_pending()){rejected(*r,"region UI requires idle bound Controller");return;}
    action::RegionIntent intent;LocalView view;
    if(!at(*r,uintptr_t(region),0x834,intent.id,e)||!text(*r,uintptr_t(value),intent.preset,e)||!identity(*r,intent.id,uintptr_t(region),view,e)){rejected(*r,e);return;}
    r->pending=intent;r->pending_snapshot=net::current_snapshot_id();r->pending_binding={view.world.origin.world_epoch,view.world.origin.generation};++r->captured;
}
json state_json(const action::RegionState& s){return {{"id",s.id},{"preset",s.preset},{"styles",s.styles},{"random",s.random},{"scenery_count",s.scenery_count},{"scenery_sha256",s.scenery_sha256},{"grid_sha256",s.grid_sha256}};}
uint32_t select_ui(uintptr_t image,uintptr_t manager,uintptr_t widget) {
    __try {
        reinterpret_cast<void(*)(void*,void*)>(image+0x2d5c60)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(widget));
        reinterpret_cast<void(*)(void*)>(image+0x2d28b0)(reinterpret_cast<void*>(manager));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct UiReceipt {bool constructed=false,called=false,returned=false,accepted=false,cleaned=false;};
uint32_t invoke_ui(uintptr_t image,uintptr_t window,uintptr_t list,const NativeString* preset,void* record,UiReceipt& receipt) {
    __try {
        reinterpret_cast<void(*)(void*,const void*)>(image+0x798630)(reinterpret_cast<void*>(list),preset);
        const auto* selected=reinterpret_cast<const NativeString*(*)(void*)>(image+0x798100)(reinterpret_cast<void*>(list));
        if(!selected||selected->length!=preset->length||std::memcmp(selected->data,preset->data,preset->length))return 0xE0010001;
        reinterpret_cast<void(*)(void*,const char*)>(image+0x967370)(record,"PresetApply");receipt.constructed=true;
        if(!reinterpret_cast<bool(*)(const void*,const char*)>(image+0x1113d80)(record,"PresetApply"))return 0xE0010002;
        receipt.called=true;receipt.accepted=reinterpret_cast<bool(*)(void*,void*)>(image+0x797850)(reinterpret_cast<void*>(window),record);receipt.returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t cleanup_record(uintptr_t image,void* record,UiReceipt& receipt) {
    __try {reinterpret_cast<void(*)(void*)>(image+0x9676a0)(record);receipt.cleaned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t refresh_window(uintptr_t image,uintptr_t window) {
    __try {reinterpret_cast<void(*)(void*)>(image+0x797360)(reinterpret_cast<void*>(window));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool refresh_current_window(Runtime& r,const LocalView& view,std::string& e) {
    uintptr_t handler=0,ap=0,window=0,region=0;uint8_t visible=0;
    if(!at(r,view.world.selection,0xb8,handler,e))return false;
    if(!handler)return true;
    if(!at(r,handler,0,ap,e))return false;
    if(ap!=r.image+0x17317e0)return true;
    if(!at(r,handler,0x10,window,e))return false;
    if(!window)return true;
    if(!at(r,window,0,ap,e)||!at(r,window,0x4e8,region,e)||!at(r,window,0x45f,visible,e))return false;
    if(ap!=r.image+0x171fad0)return fail(e,"region current InfoWindow class differs");
    if(region!=view.region||!visible)return true;
    const auto seh=refresh_window(r.image,window);
    return !seh||fail(e,"region native SetContents SEH="+std::to_string(seh));
}
}
#include "region_styles_runtime.inc"
#include "region_properties_runtime.inc"
bool enable(std::string& e) {
    auto* r=published.load(std::memory_order_acquire);
    if(!r){r=new Runtime;published.store(r,std::memory_order_release);}
    if(r->install_attempted||GetCurrentThreadId()!=r->thread)return fail(e,"region hook installation already attempted or wrong thread");
    r->install_attempted=true;uint32_t thread=0;std::string hash;
    if(!native::read_resolver_receipt(r->image,thread,hash,e)||hash!=exact_exe||thread!=r->thread)return fail(e,"region hook exact EXE or Mainthread differs: "+e);
    if(!host::resolve_lookup(r->memory,r->image,hash,r->api,e))return false;
    constexpr unsigned char start[]{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53};
    constexpr unsigned char call[]{0x48,0x89,0xd9,0x48,0x89,0xc2,0xe8,0x52,0xf9,0xbb,0xff};
    std::array<unsigned char,sizeof start> a{};std::array<unsigned char,sizeof call> b{};
    if(!r->memory.copy(r->image+0x3575d0,a.data(),a.size(),e)||!r->memory.copy(r->image+0x797c73,b.data(),b.size(),e))return false;
    if(std::memcmp(a.data(),start,a.size())||std::memcmp(b.data(),call,b.size()))return fail(e,"region original SetPreset/UI callsite bytes differ");
    SetPreset original=nullptr;const auto created=MH_CreateHook(reinterpret_cast<void*>(r->image+0x3575d0),reinterpret_cast<void*>(&preset_entry),reinterpret_cast<void**>(&original));
    if(created!=MH_OK||!original)return fail(e,"region MinHook create status="+std::to_string(created));
    r->hook_created=true;r->original.store(original,std::memory_order_release);
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(r->image+0x3575d0));
    if(enabled!=MH_OK)return fail(e,"region MinHook enable status="+std::to_string(enabled));
    if(!install_styles(*r,e))return false;
    if(!install_properties(*r,e))return false;
    r->ready.store(true,std::memory_order_release);return true;
}
bool enabled()noexcept{auto* r=published.load(std::memory_order_acquire);return r&&r->ready;}
void tick() {
    property_tick();
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||!r->pending)return;
    std::string e;LocalView before;const auto intent=*r->pending;r->pending.reset();
    if(!identity(*r,intent.id,0,before,e)||!bound(before,r->pending_binding,r->pending_snapshot,e)){rejected(*r,e);return;}
    auto id=uuid(e);if(id.empty()){rejected(*r,e);return;}
    r->last_action=id;
    if(!typed_action_runtime::submit_region(id,intent,e)){rejected(*r,e);return;}++r->submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e) {
    if(request.action==action::Action::EditRegionProperty)return property_perform(binding,request,authority,e);
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||(request.action!=action::Action::SetRegionPreset&&request.action!=action::Action::SetRegionStyles)){e="region native runtime unavailable or action differs";return action::Outcome::RejectedNoEffect;}
    if(r->operations.contains(request.action_uuid)){e="region native action already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="region operation lacks exact Controller request";return action::Outcome::RejectedNoEffect;}
    LocalView before;if(!read(*r,request.region.id,0,before,e)||!bound(before,binding,request.snapshot_uuid,e))return action::Outcome::RejectedNoEffect;
    action::Ledger money;if(!ledger(money,e))return action::Outcome::RejectedNoEffect;
    json known_bindings;if(!remove_diagnostics::bindings(known_bindings,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(authority->before!=money||!action::same_region_content(before.state,authority->region_before))){e="region client content differs from Host before state";return action::Outcome::RejectedNoEffect;}
    std::array<std::string,4> expected;
    const bool styles_action=request.action==action::Action::SetRegionStyles;
    const auto expected_preset=styles_action?before.state.preset:request.region.preset;
    if(styles_action){expected=request.region.styles;if(!validate_styles(*r,expected,e))return action::Outcome::RejectedNoEffect;}
    else if(!expected_styles(*r,request.region,before,expected,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(authority->region_after.preset!=expected_preset||authority->region_after.styles!=expected)){e="region client preset catalog differs from Host";return action::Outcome::RejectedNoEffect;}
    Operation operation;operation.binding=binding;operation.request=request;operation.evidence.before=money;
    operation.evidence.region_before=authority?authority->region_before:before.state;
    auto& saved=r->operations.emplace(request.action_uuid,std::move(operation)).first->second;
    NativeString string{request.region.preset.data(),request.region.preset.size(),{}};
    std::array<NativeString,4> style_values{};
    for(size_t i=0;i<4;++i)style_values[i]={expected[i].data(),expected[i].size(),{}};
    if(styles_action&&!at(*r,r->image,0x13beda0,saved.edit_depth_before,e)){saved.cause=e;return action::Outcome::RejectedNoEffect;}
    saved.attempted=true;
    const auto seh=styles_action?invoke_styles(r->image,before.region,style_values.data(),before.random,
        authority?authority->region_before.random.data():nullptr,saved):
        invoke(r->original.load(std::memory_order_acquire),before.region,&string,before.random,
        authority?authority->region_before.random.data():nullptr,saved.returned);
    if(seh){e=saved.cause="region original execution SEH="+std::to_string(seh)+" edit_cleanup_exception="+std::to_string(saved.edit_cleanup_exception);return action::Outcome::Unknown;}
    if(styles_action&&(!at(*r,r->image,0x13beda0,saved.edit_depth_after,e)||saved.edit_depth_after!=saved.edit_depth_before||saved.style_calls!=4||saved.edit_opened!=saved.edit_ended)){
        e=saved.cause="region original TerrainApply scope differs depth_before="+std::to_string(saved.edit_depth_before)+" depth_after="+std::to_string(saved.edit_depth_after)+" style_calls="+std::to_string(saved.style_calls)+" cause="+e;return action::Outcome::Unknown;
    }
    if(!refresh_current_window(*r,before,e)){saved.cause=e;return action::Outcome::Unknown;}
    LocalView after;
    if(!read(*r,request.region.id,before.region,after,e)||!bound(after,binding,request.snapshot_uuid,e)||!ledger(saved.evidence.after,e)){saved.cause=e;return action::Outcome::Unknown;}
    saved.evidence.region_after=after.state;
    std::set_difference(before.scenery_uids.begin(),before.scenery_uids.end(),after.scenery_uids.begin(),after.scenery_uids.end(),std::back_inserter(saved.removed_scenery_uids));
    if(!remove_diagnostics::retire_region_scenery(request.snapshot_uuid,request.action_uuid,saved.removed_scenery_uids,saved.retired_keys,e)||
       !sha256(json(saved.retired_keys).dump(),saved.evidence.region_retired_sha256,e)){saved.cause=e;return action::Outcome::Unknown;}
    if(saved.evidence.after!=money||after.state.preset!=expected_preset||after.state.styles!=expected||after.state.grid_sha256!=before.state.grid_sha256||
       (authority&&(after.state!=authority->region_after||saved.evidence.region_retired_sha256!=authority->region_retired_sha256))) {
        e=saved.cause="region original result/ledger/scenery differs from expected authority";return action::Outcome::Unknown;
    }
    saved.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& b,const action::Request& request,action::Evidence& out,std::string& e) {
    if(request.action==action::Action::EditRegionProperty)return property_observe(b,request,out,e);
    auto* r=published.load(std::memory_order_acquire);if(!r)return fail(e,"region observation runtime absent");
    const auto found=r->operations.find(request.action_uuid);
    if(found==r->operations.end()||found->second.binding!=b||found->second.request!=request||!found->second.confirmed)return fail(e,"region observation lacks exact confirmed operation");
    const auto& operation=found->second;LocalView actual;action::Ledger money;
    if(!read(*r,request.region.id,0,actual,e)||!bound(actual,b,request.snapshot_uuid,e)||!ledger(money,e))return false;
    if(actual.state!=operation.evidence.region_after||money!=operation.evidence.after)return fail(e,"region observation changed after native result");
    out=operation.evidence;out.retirement_confirmed=operation.retired;return true;
}
bool retire(const action::Binding& b,const action::Request& request,std::string& e) {
    if(request.action==action::Action::EditRegionProperty)return property_retire(b,request,e);
    action::Evidence actual;if(!observe(b,request,actual,e))return false;
    auto* r=published.load(std::memory_order_acquire);auto& operation=r->operations.at(request.action_uuid);
    if(operation.retired)return fail(e,"region receipt already retired");
    // SetPreset owns no temporary cursor or DoGraph. The original return and readback retire this scope.
    operation.retired=true;return true;
}
bool retired_keys(const action::Binding& b,const action::Request& request,std::vector<std::string>& keys,std::string& e) {
    if(request.action==action::Action::EditRegionProperty)return property_retired_keys(b,request,keys,e);
    action::Evidence actual;if(!observe(b,request,actual,e))return false;
    auto* r=published.load(std::memory_order_acquire);keys=r->operations.at(request.action_uuid).retired_keys;return true;
}
json status() {
    auto* r=published.load(std::memory_order_acquire);if(!r)return {{"initialized",false}};
    if(GetCurrentThreadId()!=r->thread)return {{"initialized",true},{"error","region status requires Mainthread"}};
    json operations=json::array();for(const auto& [id,operation]:r->operations)operations.push_back({{"action_uuid",id},{"attempted",operation.attempted},{"returned",operation.returned},{"action",uint32_t(operation.request.action)},{"style_calls",operation.style_calls},{"edit_opened",operation.edit_opened},{"edit_ended",operation.edit_ended},{"edit_end_attempted",operation.edit_end_attempted},{"edit_cleanup_exception",operation.edit_cleanup_exception},{"edit_depth_before",operation.edit_depth_before},{"edit_depth_after",operation.edit_depth_after},{"confirmed",operation.confirmed},{"retired",operation.retired},{"removed_scenery_uids",operation.removed_scenery_uids},{"retired_shared_keys",operation.retired_keys},{"retired_keys_sha256",operation.evidence.region_retired_sha256},{"before",state_json(operation.evidence.region_before)},{"after",state_json(operation.evidence.region_after)},{"cause",operation.cause}});
    json property_operations=json::array();
    for(const auto& [id,op]:properties.operations)property_operations.push_back({{"action_uuid",id},{"kind",uint8_t(op.request.region_property.kind)},
        {"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},
        {"before_name",op.evidence.property_before.name},{"after_name",op.evidence.property_after.name},
        {"before_type",op.evidence.property_before.type},{"after_type",op.evidence.property_after.type},
        {"before_random",op.evidence.property_before.content.random},{"after_random",op.evidence.property_after.content.random},
        {"before_type_random",op.evidence.property_before.type_random},{"after_type_random",op.evidence.property_after.type_random},
        {"before_levels",json::array({op.evidence.property_before.level,op.evidence.property_before.max_level})},
        {"after_levels",json::array({op.evidence.property_after.level,op.evidence.property_after.max_level})},
        {"level_brush",op.level_brush},{"level_graph",op.level_graph},{"level_constructed",op.level_constructed},{"level_destroyed",op.level_destroyed},
        {"undo_confirmed",op.evidence.undo_confirmed},{"level_before",op.evidence.property_before.actors?level_json(*op.evidence.property_before.actors):json(nullptr)},
        {"level_after",op.evidence.property_after.actors?level_json(*op.evidence.property_after.actors):json(nullptr)},
        {"before_owned",op.evidence.property_before.owned},{"after_owned",op.evidence.property_after.owned},
        {"building_count_before",op.evidence.property_before.building_count},{"building_count_after",op.evidence.property_after.building_count},
        {"building_uids_sha256_before",op.building_uids_sha256_before},{"building_uids_sha256_after",op.building_uids_sha256_after},
        {"scenery_count_before",op.evidence.property_before.content.scenery_count},{"scenery_count_after",op.evidence.property_after.content.scenery_count},
        {"path_count_before",op.evidence.property_before.path_count},{"path_count_after",op.evidence.property_after.path_count},
        {"path_node_count_before",op.evidence.property_before.path_node_count},{"path_node_count_after",op.evidence.property_after.path_node_count},
        {"network_node_count_before",op.evidence.property_before.network_node_count},{"network_node_count_after",op.evidence.property_after.network_node_count},
        {"network_pipe_count_before",op.evidence.property_before.network_pipe_count},{"network_pipe_count_after",op.evidence.property_after.network_pipe_count},
        {"removed_uids",op.removed_uids},{"removed_lookup_absent",op.removed_lookup_absent},
        {"retired_shared_keys",op.retired_keys},{"retired_keys_sha256",op.evidence.region_retired_sha256},
        {"before_cash",op.evidence.before.cash},{"after_cash",op.evidence.after.cash},{"cause",op.cause}});
    return {{"initialized",true},{"enabled",bool(r->ready)},{"pending_ui",r->pending.has_value()||properties.pending.has_value()},
        {"captured",r->captured},{"submitted",r->submitted},{"rejected",r->rejected},{"forwarded",r->forwarded},
        {"last_action_uuid",r->last_action},{"cause",r->cause},{"operations",operations},{"last_ui",r->last_ui},
        {"property_captured",properties.captured},{"property_submitted",properties.submitted},{"property_rejected",properties.rejected},
        {"property_last_action_uuid",properties.last_action},{"property_cause",properties.cause},{"property_operations",property_operations}};
}
bool query(int32_t region_id,json& out,std::string& e) {
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready)return fail(e,"region query runtime unavailable");
    LocalView view;action::RegionPropertyState property;
    if(!property_read(*r,region_id,0,property,view,e))return false;
    out=state_json(view.state);
    out["name"]=property.name;out["type"]=property.type;out["owned"]=property.owned;
    out["status"]=property.status;out["level"]=property.level;out["max_level"]=property.max_level;
    out["type_random"]=property.type_random;
    out["actors"]=nullptr;if(property.owned){action::RegionLevelState actors;uintptr_t global=0;
        if(!level_state(*r,view,{property.level,property.max_level},actors,global,e))return false;out["actors"]=level_json(actors);}
    std::array<float,3> position{};uintptr_t map=0,at_position=0;
    if(!at(*r,view.region,0x38,position,e)||!at(*r,r->image,0x13df8c0,map,e))return false;
    const auto hit_seh=property_hit_region(r->image,map,position.data(),at_position);
    out["region_position"]=position;out["position_resolves_region"]=!hit_seh&&at_position==view.region;
    if(hit_seh)return fail(e,"region native center GetRegionAt SEH="+std::to_string(hit_seh));
    uintptr_t manager=0,data=0;int32_t count=0;
    if(!at(*r,r->image,0x13df7f0,manager,e)||!at(*r,manager,8,data,e)||!at(*r,manager,0x10,count,e))return false;
    if(count<1||count>4096||!data)return fail(e,"region native preset catalog span invalid");
    json presets=json::array();std::set<std::string> names;
    for(int32_t i=0;i<count;++i) {
        uintptr_t preset=0;std::string name;
        if(!at(*r,data,size_t(i)*8,preset,e)||!text(*r,preset+0x18,name,e))return false;
        if(!names.insert(name).second)return fail(e,"region native preset names duplicate");
        presets.push_back(name);
    }
    out["available_presets"]=std::move(presets);out["preset_ui_available"]=false;out["preset_ui_reason"]="shipped info_region.win comments out Style/Preset tab";
    std::vector<std::string> styles;if(!style_catalog(*r,styles,e))return false;out["available_styles"]=styles;
    return ui_readback::read(r->memory,r->image,view.world.selection,out["ui_state"],e);
}
bool preset_ui(const std::string& token,int32_t region_id,const std::string& preset,json& out,std::string& e) {
    out=json::object();auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||GetCurrentThreadId()!=r->thread||!net::session_wire::connected())return fail(e,"region UI diagnostic requires enabled connected Mainthread");
    if(token.empty()||token.size()>64||preset.empty()||preset.size()>256||preset.find('\0')!=std::string::npos)return fail(e,"region UI diagnostic input invalid");
    const auto controller=typed_action_runtime::status();
    if(!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||r->pending)return fail(e,"region UI diagnostic requires idle Controller");
    if(r->ui_attempts.contains(token))return fail(e,"region UI diagnostic token already attempted; inspect receipt before retry");
    LocalView before;if(!read(*r,region_id,0,before,e))return false;
    std::array<std::string,4> styles;if(!expected_styles(*r,{region_id,preset},before,styles,e))return false;
    uintptr_t widget=0,ap=0,owner=0;
    if(!at(*r,before.region,0x370,widget,e)||!at(*r,widget,0,ap,e)||ap!=r->image+0x17024b0||!at(*r,widget,0x178,owner,e)||owner!=before.region)return fail(e,"region UI actual RegionWidget differs: "+e);
    r->ui_attempts.insert(token);out["selection_attempted"]=true;
    const auto selection=select_ui(r->image,before.world.selection,widget);
    out["selection_exception"]=selection;if(selection)return fail(e,"region native SelectProp/SetInfoWindow SEH="+std::to_string(selection));
    json ui_state;const bool ui_read=ui_readback::read(r->memory,r->image,before.world.selection,ui_state,e);
    out["ui_state"]=ui_state;r->last_ui={{"token",token},{"ui_state",ui_state},{"read_complete",ui_read}};
    if(!ui_read)return false;
    if(!ui_readback::validate(ui_state,r->image,widget,before.region,e)){r->last_ui["cause"]=e;return false;}
    const auto window=ui_state.at("window").get<uintptr_t>(),list=ui_state.at("preset_list").get<uintptr_t>();
    out["window_confirmed"]=true;const auto captured=r->captured;
    alignas(8) unsigned char record[0x78]{};NativeString string{preset.data(),preset.size(),{}};UiReceipt receipt;
    const auto native=invoke_ui(r->image,window,list,&string,record,receipt);
    const auto cleanup=receipt.constructed?cleanup_record(r->image,record,receipt):0;
    out["command_attempted"]=receipt.called;out["command_returned"]=receipt.returned;out["command_accepted"]=receipt.accepted;
    out["record_cleaned"]=receipt.cleaned;out["native_exception"]=native;out["cleanup_exception"]=cleanup;
    if(native||cleanup||!receipt.returned||!receipt.accepted||!receipt.cleaned)return fail(e,"region actual PresetApply failed SEH="+std::to_string(native)+" cleanup="+std::to_string(cleanup));
    const bool captured_once=r->captured==captured+1&&r->pending&&r->pending->id==region_id&&r->pending->preset==preset;
    out["captured_once"]=captured_once;
    if(!captured_once)return fail(e,"region PresetApply did not reach genuine lower UI capture: "+r->cause);
    LocalView after;if(!read(*r,region_id,before.region,after,e))return false;
    if(after.state!=before.state)return fail(e,"region client UI mutated before Host authority");
    out["local_state_unchanged"]=true;out["pending_intent"]=true;return true;
}
}

#include "ui_terrain_runtime.inc"
#include "ui_path_runtime.inc"
#include "ui_network_runtime.inc"

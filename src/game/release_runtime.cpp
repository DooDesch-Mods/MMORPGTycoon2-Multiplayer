#include "release_runtime.h"
#include "control_input/control_runtime.h"
#include "release_edit_native.h"
#include "release_edit_reader.h"
#include "release_ui_owner.h"
#include "release_graph_native.h"
#include "release_cycle_native.h"
#include "release_live_port.h"
#include "release_boundary_owner.h"
#include "release_boundary_frames.h"
#include "mt2mp/core/framework_runtime.h"
#include "release_reference_reader.h"
#include "general_project_runtime.h"
#include "remove_diagnostics.h"
#include "npc_probe.h"
#include "direct_monster_identity.h"
#include "region_saved_identity.h"
#include "network_identity.h"
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "native_open_runtime.h"
#include "live_host_state/native_port.h"
#include "live_host_state/apply_timing.h"
#include "live_host_state/memory_query_timing.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/hooks/minhook_backend.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include <Windows.h>
#include <bcrypt.h>
#include <atomic>
#include <map>
#include <memory>
#include <cstdio>
namespace mt2mp::game::release_runtime {
namespace {
namespace a=action_session;namespace p=release_edit;namespace live=live_host_state;namespace n=release_edit_native;
using UI=bool(*)(void*,void*);
struct Pending {a::Context context;a::Binding binding;p::Intent intent;std::string id;uintptr_t window=0;};
struct Operation {a::Context context;a::Binding binding;a::Request request;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,ui_attempted=false,ui_returned=false;uint32_t calls=0,default_steps=0;std::string cause;};
struct Route {uintptr_t rva;size_t bytes,stolen;const char* sha;uintptr_t ap;UI original=nullptr;release_ui_owner::Owner owner;};
struct Runtime {DWORD thread=GetCurrentThreadId();uintptr_t image=0,presentation_window=0;size_t image_bytes=0;
    release_ui_owner::Owner postresolve_owner;release_graph_native::PostResolve postresolve=nullptr;bool install_attempted=false,ready=false,unknown=false;placement_ui_probe::ProcessMemory memory;
    std::optional<Pending> pending;std::map<std::string,Pending> presentations;std::map<std::string,Operation> operations;uint64_t captures=0,submitted=0,rejected=0;std::string cause;
    std::array<Route,3> routes{{{0x79baa0,1840,6,"bbc3f16988ec42c6fd77f3dc59f929b67f6d4c01f0c025afa0aec3c66ac8f542",0x171fe80},{0x79d8b0,784,11,"105796f231015274d19186f55d8713a89d681fb849de3b8a12f5d485d0c7b4f3",0x17338a0},{0x69ef10,528,8,"445d5464b3260595c7358d14d7049dd138361682bdc0b51487b2421a23745fbc",0x1721d20}}};
};
std::atomic<Runtime*> published{nullptr};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void rejected(Runtime& r,const std::string& cause){++r.rejected;if(r.cause!=cause){r.cause=cause;LOG_ERROR("[Release] cause={}",cause);}}
template<class T>bool at(Runtime& r,uintptr_t base,size_t offset,T& value,std::string& e){if(!base||base>UINTPTR_MAX-offset)return fail(e,"Release native address overflow");return r.memory.copy(base+offset,&value,sizeof value,e);}
bool string(Runtime& r,uintptr_t address,std::string& value,std::string& e){uintptr_t data=0;uint64_t count=0;if(!at(r,address,0,data,e)||!at(r,address,8,count,e))return false;
    if(!data||count>50000||data>UINTPTR_MAX-count)return fail(e,"Release GNU string extent invalid");value.resize(size_t(count));if(count&&!r.memory.copy(data,value.data(),size_t(count),e))return false;char end=1;
    if(!at(r,data,size_t(count),end,e)||end||value.find('\0')!=std::string::npos)return fail(e,e.empty()?"Release GNU string termination differs":e);return true;
}
std::string hash(const std::string& value){return mt2mp::sha256(std::vector<unsigned char>(value.begin(),value.end()));}
bool context(Runtime& r,const a::Context& c,const a::Binding& b,live::Pin& pin,std::string& e){
    a::Context actual;uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(actual,e)||actual!=c||!native::read_resolver_receipt(image,thread,sha,e)||image!=r.image||thread!=r.thread||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,e.empty()?"Release actual Session/Mainthread/EXE differs":e);
    live::NativePort port(c,b,nullptr);if(!port.read(pin,e))return false;
    if(c.role==a::Role::Client&&pin.selected!=0)return fail(e,"Release Client actual Clock requires0");return true;
}
std::string uuid(std::string& e){unsigned char b[16]{};const auto code=BCryptGenRandom(nullptr,b,sizeof b,BCRYPT_USE_SYSTEM_PREFERRED_RNG);if(code){e="Release UUID NTSTATUS="+std::to_string(code);return {};}
    b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;char text[37]{};std::snprintf(text,sizeof text,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);return text;
}
uint32_t command_label(uintptr_t image,void* record,uintptr_t& result)noexcept{
    __try{result=uintptr_t(reinterpret_cast<const void*(*)(const void*)>(image+0x96d880)(record));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

bool code(Runtime& r,std::string& e){
    for(const auto& route:r.routes){if(!route.owner.generation)continue;
        MT2FW_MT2_RouteReceipt receipt{};receipt.size=sizeof receipt;
        if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(route.owner.entry),receipt))return fail(e,"Release named route read failed: "+hooks::framework::process_backend().error());
        if(receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.generation!=route.owner.generation||receipt.entry!=route.owner.entry||receipt.trampoline!=route.owner.trampoline||receipt.detour!=route.owner.detour||std::strcmp(receipt.owner,"doodesch.multiplayer"))return fail(e,"Release named Framework owner/generation changed");
        if(!release_ui_owner::verify(route.owner,[&](auto address,auto out,auto count,auto& cause){return r.memory.copy(address,out,count,cause);},[](const auto& bytes){return mt2mp::sha256(bytes);},route.sha,e)||!hooks::framework::process_backend().verify_code(route.owner.entry,route.owner.pristine.data(),route.owner.pristine.size(),e))return false;
    }
    if(r.postresolve_owner.generation){auto& owner=r.postresolve_owner;MT2FW_MT2_RouteReceipt receipt{};receipt.size=sizeof receipt;
        if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),receipt)||receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.generation!=owner.generation||receipt.entry!=owner.entry||receipt.trampoline!=owner.trampoline||receipt.detour!=owner.detour||std::strcmp(receipt.owner,"doodesch.multiplayer"))return fail(e,"Release PostResolve actual named owner/generation differs");
        if(!release_ui_owner::verify(owner,[&](auto address,auto out,auto count,auto& cause){return r.memory.copy(address,out,count,cause);},[](const auto& bytes){return mt2mp::sha256(bytes);},"a1edc1837751699deb1d9ebcd01f53442da8b9ad83733e848720cc0695a58ddd",e)||!hooks::framework::process_backend().verify_code(owner.entry,owner.pristine.data(),owner.pristine.size(),e))return false;}
    #include "release_edit_pins.inc"
    #include "release_cycle_pins.inc"
    for(const auto& pin:cycle_pins){
        if(pin.rva==0x259d70){bool owned=false;
            if(!control_input::runtime::verify_clock_time_scale(r.image,pin.bytes,pin.sha,owned,e))return false;
            if(owned)continue;
        }
        std::vector<unsigned char> bytes(pin.bytes);if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),e))return false;if(mt2mp::sha256(bytes)!=pin.sha)return fail(e,"Release cycle original helper body differs RVA="+std::to_string(pin.rva));}
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.bytes);if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),e))return false;if(mt2mp::sha256(bytes)!=pin.sha)return fail(e,"Release original helper body differs RVA="+std::to_string(pin.rva));}return true;
}
#include "release_reference_runtime.inc"
bool sample(Runtime& r,const a::Context& c,const a::Binding& b,p::Local& out,p::State& state,a::Ledger& money,std::string& e){
    live::Pin pin;if(!context(r,c,b,pin,e))return false;
    auto read=[&](uintptr_t address,size_t offset,auto& value,std::string& cause){return at(r,address,offset,value,cause);};auto text=[&](uintptr_t address,std::string& value,std::string& cause){return string(r,address,value,cause);};
    p::Local before;p::State first;if(!p::sample(read,text,r.image,before,first,e)||!p::sample(read,text,r.image,out,state,e))return false;
    if(before!=out||first!=state)return fail(e,"Release actual owners/text/state changed during double read");
    building_mixed_identity::opened_file::CurrentWorld world;uintptr_t software=0;if(!native_open_runtime::read_current_world(world,e)||world.world_epoch!=b.native_epoch||world.generation!=b.native_generation||!at(r,uintptr_t(world.state),0x1c8,software,e)||software!=out.software)return fail(e,e.empty()?"Release actual World/Software reciprocal identity differs":e);
    money=live::ledger(pin.values);return true;
}
bool defaults(Runtime& r,uintptr_t release,p::Defaults& out,Operation& op,std::string& e){
    for(unsigned which=0;which<2;++which){n::Returned result;uint32_t completed=0,exception=0;
        for(unsigned step=0;step<2;++step){exception=n::default_step(r.image,release,which==0,result,step);if(exception)break;completed|=1u<<step;op.default_steps|=1u<<(which*5+step);}
        std::string first;
        if(exception)first="Release original default getter/format SEH="+std::to_string(exception);
        else if(!string(r,uintptr_t(result.text.data()),which==0?out.announcement:out.changelog,first)&&first.empty())first="Release original default text read failed";
        // A throwing constructor has uncertain partial ownership. Only returned values are destroyed.
        for(unsigned step=2;step<5;++step){if((step==2&&!(completed&2))||(step>=3&&!(completed&1)))continue;
            const auto cleanup=n::default_step(r.image,release,which==0,result,step);if(cleanup){if(first.empty())first="Release original default cleanup SEH="+std::to_string(cleanup);else first+="; cleanup SEH="+std::to_string(cleanup);break;}op.default_steps|=1u<<(which*5+step);}
        if(!first.empty())return fail(e,first);
    }return true;
}
bool collection(Runtime& r,uintptr_t owner,size_t offset,std::vector<uintptr_t>& out,std::string& e){uintptr_t data=0;int32_t count=0,capacity=0;
    if(!at(r,owner,offset+8,data,e)||!at(r,owner,offset+16,count,e)||!at(r,owner,offset+20,capacity,e))return false;
    if(count<0||count>4096||capacity<count||capacity>65536||(!data&&capacity))return fail(e,"Release UI owned collection extent invalid");out.resize(count);return !count||r.memory.copy(data,out.data(),out.size()*8,e);
}
bool window(Runtime& r,uintptr_t receiver,size_t route_index,std::string& e){building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,e))return false;std::vector<uintptr_t> windows;if(!collection(r,uintptr_t(world.mode),0x98,windows,e))return false;
    uintptr_t ap=0,logic=1;uint8_t visible=0;std::string name;
    if(std::count(windows.begin(),windows.end(),receiver)!=1||!at(r,receiver,0,ap,e)||ap!=r.image+r.routes[route_index].ap||!at(r,receiver,0x318,logic,e)||logic||!at(r,receiver,0x45f,visible,e)||visible!=1||!string(r,receiver+0x1b0,name,e))return fail(e,e.empty()?"Release original UI class/current Mode membership/visibility differs":e);
    if(route_index==0&&name!="release_hud")return fail(e,"Release HUD original window name differs");
    if(route_index==1&&name!="release_planner"&&name!="release_planner_nomoney")return fail(e,"Release planner original window name differs");return true;
}
bool child(Runtime& r,uintptr_t owner,uintptr_t object,std::string& e){std::set<uintptr_t> seen;uintptr_t current=object;
    for(size_t depth=0;depth<32;++depth){if(!current||!seen.insert(current).second)return fail(e,"Release UI child parent chain absent/cyclic");if(current==owner)return true;
        uintptr_t parent=0,window=0;if(!at(r,current,0x278,parent,e)||!at(r,current,0x280,window,e))return false;if(window&&window!=owner)return fail(e,"Release UI child belongs to another Window");
        unsigned matches=0;for(size_t offset:{size_t(0x2c8),size_t(0x2f8)}){std::vector<uintptr_t> rows;if(!collection(r,parent,offset,rows,e))return false;matches+=unsigned(std::count(rows.begin(),rows.end(),current));}
        if(matches!=1)return fail(e,"Release UI child lacks exact primary/secondary membership");current=parent;
    }return fail(e,"Release UI child hierarchy exceeds32");
}
uint32_t token_text(uintptr_t image,void* record,int index,uintptr_t& result)noexcept{__try{void* token=reinterpret_cast<void*(*)(void*,int)>(image+0x967740)(record,index);result=uintptr_t(reinterpret_cast<const void*(*)(const void*)>(image+0x96d880)(token));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t pane_text(uintptr_t image,uintptr_t pane,uintptr_t& result)noexcept{__try{result=uintptr_t(reinterpret_cast<const void*(*)(const void*)>(image+0x7ebd60)(reinterpret_cast<const void*>(pane)));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
#include "release_cycle_runtime.inc"
bool capture_edit(Runtime& r,size_t index,void* receiver,void* record,bool& handled,bool& accepted,std::string& e){
    handled=false;accepted=false;uintptr_t label=0;std::string command;const auto exception=command_label(r.image,record,label);
    if(exception||!string(r,label,command,e)){handled=true;return fail(e,exception?"Release command label SEH="+std::to_string(exception):e);}
    if(!cycle_capture(r,index,receiver,record,command,handled,accepted,e)||handled)return e.empty();
    Pending pending;pending.window=uintptr_t(receiver);
    if(index==1){if(command=="SetHotfix"||command=="SetMinor"||command=="SetMajor"){pending.intent.command=p::Command::SetType;pending.intent.type=command=="SetHotfix"?1:command=="SetMinor"?2:3;}else if(command=="SetCustomTitle")pending.intent.command=p::Command::PlannerTitle;else return true;}
    else if(index==0){if(command=="ConfirmResetCustomText")pending.intent.command=p::Command::ResetCustom;else if(command=="EditedChangelog")pending.intent.command=p::Command::Changelog;else if(command=="EditedAnnounceTitle")pending.intent.command=p::Command::Announcement;else if(command=="ToggleCustomText")pending.intent.command=p::Command::UseCustom;else return true;}
    else return true;
    handled=true;const auto controller=typed_action_runtime::controller_status();int32_t count=-1;
    if(r.unknown||r.pending||r.presentations.size()>=a::history_limit||controller.value("pending",true)||controller.value("locked",true)||!window(r,pending.window,index,e)||!code(r,e)||!at(r,uintptr_t(record),0x40,count,e)||count<0||count>1||!a::selected_session().current(pending.context,e)||!typed_action_runtime::network_binding(pending.context.snapshot_uuid,pending.binding,e))return fail(e,e.empty()?"Release edit requires exact idle owned UI/record capacity":e);
    const auto type=pending.intent.command;const bool has_text=type==p::Command::PlannerTitle||type==p::Command::Changelog||type==p::Command::Announcement;
    if(has_text){uintptr_t value=0;
        if(type==p::Command::PlannerTitle){if(count!=1)return fail(e,"Release planner title needs one original token");const auto seh=token_text(r.image,record,0,value);if(seh)return fail(e,"Release planner original token SEH="+std::to_string(seh));}
        else{uintptr_t pane=0,ap=0;if(!at(r,pending.window,type==p::Command::Changelog?0x540:0x538,pane,e)||!at(r,pane,0,ap,e)||ap!=r.image+0x1703290||!child(r,pending.window,pane,e))return fail(e,e.empty()?"Release original edit pane differs":e);
            const auto seh=pane_text(r.image,pane,value);if(seh)return fail(e,"Release original pane text SEH="+std::to_string(seh));
            if(!string(r,value,pending.intent.text,e))return false;
            if(type==p::Command::Announcement&&!pending.intent.text.empty()){if(count!=1)return fail(e,"Release nonempty announcement needs its original token");const auto seh=token_text(r.image,record,0,value);if(seh)return fail(e,"Release announcement token SEH="+std::to_string(seh));}
        }
        if(!string(r,value,pending.intent.text,e))return false;pending.intent.text_sha256=hash(pending.intent.text);
    }else if(count)return fail(e,"Release original scalar command has unexpected tokens");
    if(type==p::Command::UseCustom){uintptr_t checkbox=0,table=0,getter=0;uint8_t value=2;if(!at(r,pending.window,0x560,checkbox,e)||!child(r,pending.window,checkbox,e)||!at(r,checkbox,0,table,e)||!at(r,table,0x1c0,getter,e)||getter!=r.image+0xd388c0||!at(r,checkbox,0x585,value,e)||value>1)return fail(e,e.empty()?"Release original checkbox getter/owner/value differs":e);pending.intent.custom=value;}
    p::Local local;p::State state;a::Ledger money;if(!p::valid(pending.intent)||!sample(r,pending.context,pending.binding,local,state,money,e)||(pending.id=uuid(e)).empty())return false;
    r.presentations.emplace(pending.id,pending);r.pending=std::move(pending);++r.captures;accepted=true;return true;
}
bool ui_entry(size_t index,void* receiver,void* record){auto* r=published.load(std::memory_order_acquire);if(!r||!r->routes[index].original)throw std::runtime_error("Release original UI was not published");
    if(GetCurrentThreadId()!=r->thread||!typed_action_runtime::controller_status().value("started",false))return r->routes[index].original(receiver,record);
    if(r->presentation_window){uintptr_t label=0;std::string command,e;const auto exception=command_label(r->image,record,label);
        if(index!=0||r->presentation_window!=uintptr_t(receiver)||exception||!string(*r,label,command,e)||(command!="EditedAnnounceTitle"&&command!="EditedChangelog")){r->unknown=true;rejected(*r,e.empty()?"Release source text refresh emitted unexpected command":e);return false;}
        // Native SetText emits the same edit notification. Its world value was already ordered.
        return true;
    }
    bool handled=false,accepted=false;std::string e;if(!capture_edit(*r,index,receiver,record,handled,accepted,e)){rejected(*r,e);return false;}if(handled)return accepted;return r->routes[index].original(receiver,record);
}
bool hud_entry(void* receiver,void* record){return ui_entry(0,receiver,record);}bool planner_entry(void* receiver,void* record){return ui_entry(1,receiver,record);}bool celebration_entry(void* receiver,void* record){return ui_entry(2,receiver,record);}
bool presentation(Runtime& r,Operation& op,std::string& e){const auto found=r.presentations.find(op.request.action_uuid);if(found==r.presentations.end())return true;if(op.ui_attempted)return op.ui_returned||fail(e,"Release source presentation already attempted; no replay");
    const auto& source=found->second;const auto index=source.intent.command==p::Command::SetType||source.intent.command==p::Command::PlannerTitle?1u:0u;
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e))return false;std::vector<uintptr_t> windows;if(!collection(r,uintptr_t(world.mode),0x98,windows,e))return false;
    if(std::find(windows.begin(),windows.end(),source.window)==windows.end()){r.presentations.erase(found);LOG_INFO("[Release] Source window closed action={}",op.request.action_uuid);return true;}
    if(!window(r,source.window,index,e)||!code(r,e))return false;p::Local local,after_local;p::State before,after;a::Ledger money,after_money;if(!sample(r,op.context,op.binding,local,before,money,e))return false;op.ui_attempted=true;
    if(op.evidence.release_edit_after->confirmation_required){uintptr_t game=0,game_ap=0;if(!at(r,r.image,0x13df8f0,game,e)||!at(r,game,0,game_ap,e)||game_ap!=r.image+0x1743740)return fail(e,e.empty()?"Release original confirmation Game owner differs":e);
        n::String key(std::string("release_reset_custom_changelog")),extra(std::string{}),command(std::string("MessageTo WindowName \"release_hud\" ConfirmResetCustomText"));const auto seh=n::query_reset(r.image,game,key,extra,command);if(seh)return fail(e,"Release original confirmation SEH="+std::to_string(seh)+"; no replay");
        uintptr_t query=0,ap=0;uint8_t visible=0;std::string actual_extra,actual_command;
        if(!at(r,game,0x1d8,query,e)||!at(r,query,0,ap,e)||ap!=r.image+0x16fd340||!at(r,query,0x45f,visible,e)||visible!=1||!string(r,query+0x508,actual_extra,e)||!string(r,query+0x528,actual_command,e)||!actual_extra.empty()||actual_command!="MessageTo WindowName \"release_hud\" ConfirmResetCustomText")return fail(e,e.empty()?"Release original confirmation registration/readback differs":e);
    }
    // World mutation is already ordered; only the original HUD setup is called here.
    if(index==0){const auto seh=n::refresh(r.image,source.window);if(seh)return fail(e,"Release original custom widgets refresh SEH="+std::to_string(seh));
        const auto type=source.intent.command;
        for(size_t field=2;field<4;++field){const bool reset=type==p::Command::ResetCustom||((type==p::Command::Changelog&&field==3||type==p::Command::Announcement&&field==2)&&source.intent.text.empty());if(!reset)continue;
            uintptr_t pane=0,ap=0;if(!at(r,source.window,field==2?0x538:0x540,pane,e)||!at(r,pane,0,ap,e)||ap!=r.image+0x1703290||!child(r,source.window,pane,e))return fail(e,e.empty()?"Release original refreshed pane owner differs":e);
            n::String text(before.text[field]);r.presentation_window=source.window;const auto exception=n::pane_set(r.image,pane,text);r.presentation_window=0;
            if(exception||r.unknown)return fail(e,exception?"Release original text presentation SEH="+std::to_string(exception):r.cause);
            uintptr_t actual=0;std::string value;const auto read_error=pane_text(r.image,pane,actual);if(read_error||!string(r,actual,value,e)||value!=before.text[field])return fail(e,e.empty()?"Release source original text readback differs":e);
        }
    }
    if(!sample(r,op.context,op.binding,after_local,after,after_money,e)||after_local!=local||after!=before||after_money!=money)return fail(e,e.empty()?"Release source presentation changed gameplay/RNG/owners/Ledger":e);op.ui_returned=true;r.presentations.erase(found);return true;
}
}
bool enable(std::string& e){auto* r=published.load(std::memory_order_acquire);if(!r){r=new Runtime;published.store(r,std::memory_order_release);}if(r->ready){e.clear();return true;}if(r->install_attempted)return fail(e,"Release hook installation already attempted; no replay: "+r->cause);r->install_attempted=true;
    uint32_t thread=0;std::string sha;if(!native::read_resolver_receipt(r->image,thread,sha,e)||thread!=r->thread||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"Release exact EXE/Mainthread differs: "+e);
    const std::array<UI,3> detours{&hud_entry,&planner_entry,&celebration_entry};
    for(size_t i=0;i<r->routes.size();++i){auto& route=r->routes[i];auto& owner=route.owner;owner.entry=r->image+route.rva;owner.detour=uintptr_t(detours[i]);owner.stolen=route.stolen;owner.pristine.resize(route.bytes);
        if(!r->memory.copy(owner.entry,owner.pristine.data(),owner.pristine.size(),e)||mt2mp::sha256(owner.pristine)!=route.sha)return fail(e,e.empty()?"Release original full UI body differs":e);
        const auto created=MH_CreateHook(reinterpret_cast<void*>(owner.entry),reinterpret_cast<void*>(owner.detour),reinterpret_cast<void**>(&route.original));if(created!=MH_OK||!route.original)return fail(e,"Release named prepare failed status="+std::to_string(created)+" original="+hooks::framework::process_backend().error());owner.trampoline=uintptr_t(route.original);
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(owner.entry));if(enabled!=MH_OK)return fail(e,"Release named enable failed status="+std::to_string(enabled)+" original="+hooks::framework::process_backend().error());
        MT2FW_MT2_RouteReceipt receipt{};receipt.size=sizeof receipt;if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),receipt))return fail(e,"Release enabled route receipt failed: "+hooks::framework::process_backend().error());owner.generation=receipt.generation;
    }
    {auto& owner=r->postresolve_owner;owner.entry=r->image+0x4dfed0;owner.detour=uintptr_t(&release_graph_native::postresolve_entry);owner.stolen=6;owner.pristine.resize(1840);
        if(!r->memory.copy(owner.entry,owner.pristine.data(),owner.pristine.size(),e)||mt2mp::sha256(owner.pristine)!="a1edc1837751699deb1d9ebcd01f53442da8b9ad83733e848720cc0695a58ddd")return fail(e,e.empty()?"Release original PostResolve full body differs":e);
        const auto created=MH_CreateHook(reinterpret_cast<void*>(owner.entry),reinterpret_cast<void*>(owner.detour),reinterpret_cast<void**>(&r->postresolve));
        if(created!=MH_OK||!r->postresolve)return fail(e,"Release PostResolve named prepare failed status="+std::to_string(created)+" original="+hooks::framework::process_backend().error());
        owner.trampoline=uintptr_t(r->postresolve);if(!release_graph_native::set_postresolve_original(r->postresolve,e))return false;
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(owner.entry));if(enabled!=MH_OK)return fail(e,"Release PostResolve named enable failed status="+std::to_string(enabled)+" original="+hooks::framework::process_backend().error());
        MT2FW_MT2_RouteReceipt receipt{};receipt.size=sizeof receipt;if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),receipt))return fail(e,"Release PostResolve named receipt failed: "+hooks::framework::process_backend().error());owner.generation=receipt.generation;
    }
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    if(!r->memory.copy(r->image,&dos,sizeof dos,e)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0||dos.e_lfanew>4096||!r->memory.copy(r->image+dos.e_lfanew,&nt,sizeof nt,e)||nt.Signature!=IMAGE_NT_SIGNATURE||!nt.OptionalHeader.SizeOfImage)return fail(e,e.empty()?"Release actual PE image extent differs":e);r->image_bytes=nt.OptionalHeader.SizeOfImage;
    if(!code(*r,e)||!cycle_runtime.boundary.bind(framework_runtime::release(),r->thread,[r](const auto& call,uint32_t& disposition,std::string& cause){return cycle_boundary(*r,call,disposition,cause);},e)){r->cause=e;return false;}r->ready=true;e.clear();return true;
}
bool pending_ui(){auto* r=published.load(std::memory_order_acquire);return r&&(r->unknown||r->pending.has_value()||cycle_runtime.pending.has_value()||!cycle_runtime.finalize_id.empty());}
void tick(){auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||r->unknown)return;cycle_tick(*r);if(!r->pending)return;const auto pending=*r->pending;r->pending.reset();a::Context actual;a::Binding b;std::string e;
    if(!a::selected_session().current(actual,e)||actual!=pending.context||!typed_action_runtime::network_binding(actual.snapshot_uuid,b,e)||b!=pending.binding||!typed_action_runtime::submit_release_edit(pending.id,pending.intent,e)){r->presentations.erase(pending.id);rejected(*r,e.empty()?"Release retained Context/Binding changed before ordering":e);return;}++r->submitted;
}
bool retire_lobby(std::string& e){auto* r=published.load(std::memory_order_acquire);if(!r){e.clear();return true;}native::WorldStatus world;if(GetCurrentThreadId()!=r->thread||!native::read_world_status(world,e))return false;if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Release retirement requires actual idle Title");r->pending.reset();r->presentations.clear();r->operations.clear();cycle_runtime.pending.reset();cycle_runtime.operations.clear();cycle_runtime.context.reset();cycle_runtime.binding.reset();cycle_runtime.cycle.clear();cycle_runtime.last_terminal.clear();cycle_runtime.finalize_id.clear();cycle_runtime.phase=cycle::Phase::Idle;cycle_runtime.local_finished=false;r->unknown=false;r->cause.clear();e.clear();return true;}
a::Outcome perform(const a::Binding& b,const a::Request& q,const a::Evidence* host,std::string& e){auto* r=published.load(std::memory_order_acquire);
    if(r&&r->ready&&q.action==a::Action::ReleaseCycle)return cycle_perform(*r,b,q,host,e);
    if(!r||!r->ready||r->unknown||q.action!=a::Action::ReleaseEdit||!p::valid(q.release_edit)){e="Release edit action/runtime unavailable";return a::Outcome::RejectedNoEffect;}if(r->operations.contains(q.action_uuid)){e="Release edit token already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();a::Context c;if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=q.action_uuid||!a::selected_session().current(c,e)||c.snapshot_uuid!=q.snapshot_uuid||(c.role==a::Role::Client)!=(host!=nullptr)||!code(*r,e)){if(e.empty())e="Release exact pending Controller/Context/role differs";return a::Outcome::RejectedNoEffect;}
    if(!q.release_edit.text_sha256.empty()&&hash(q.release_edit.text)!=q.release_edit.text_sha256){e="Release exact transported text SHA differs";return a::Outcome::RejectedNoEffect;}
    p::Local local;p::State before;a::Ledger money;if(!sample(*r,c,b,local,before,money,e))return a::Outcome::RejectedNoEffect;
    if(r->operations.size()>=a::history_limit){auto oldest=r->operations.end();for(auto i=r->operations.begin();i!=r->operations.end();++i)if(i->second.retired&&(oldest==r->operations.end()||i->second.request.host_revision<oldest->second.request.host_revision))oldest=i;if(oldest==r->operations.end()){e="Release retained unretired operation limit reached";return a::Outcome::RejectedNoEffect;}r->operations.erase(oldest);}
    auto& op=r->operations.try_emplace(q.action_uuid).first->second;op.context=c;op.binding=b;op.request=q;
    const auto unknown=[&](const std::string& cause){e=op.cause=cause.empty()?"Release original result Unknown":cause;r->unknown=true;rejected(*r,e);return a::Outcome::Unknown;};
    p::Defaults initial,final;op.attempted=true;if(!defaults(*r,local.release,initial,op,e))return unknown(e);
    p::Local checked;p::State actual;a::Ledger checked_money;if(!sample(*r,c,b,checked,actual,checked_money,e)||checked!=local||actual!=before||checked_money!=money)return unknown(e.empty()?"Release default formatting changed actual world/RNG/Ledger":e);
    p::Proof proof;if(!p::proof(before,initial,false,hash,proof,e))return unknown(e);op.evidence.before=money;op.evidence.release_edit_before=proof;
    if(host&&(!host->release_edit_before||!host->release_edit_after||*host->release_edit_before!=proof||host->before!=money||host->after!=money)){op.cause=e="Release Client actual complete before/defaults/Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    const auto& i=q.release_edit;uint32_t exception=0;bool confirmation=false;
    {live::apply_timing::PageReadScope original(live::apply_timing::PageBoundary::OriginalCall);
        if(i.command==p::Command::SetType){std::array<int32_t,3> next;if(!p::expected_type(before,i.type,next)){op.cause=e="Release next version overflow before native effect";return a::Outcome::RejectedNoEffect;}exception=n::set_type(r->image,local.release,next,i.type,op.calls);}
        else if(i.command==p::Command::PlannerTitle||(i.command==p::Command::Changelog&&!i.text.empty())||(i.command==p::Command::Announcement&&!i.text.empty())){n::String value(i.text);exception=n::assign(r->image,local.release+(i.command==p::Command::PlannerTitle?0x80:i.command==p::Command::Changelog?0xe0:0xc0),value);if(!exception){op.calls|=8;if(i.command==p::Command::Changelog)exception=n::custom(local.release,before.custom,true);}}
        else if(i.command==p::Command::Changelog||i.command==p::Command::Announcement){exception=n::reset(r->image,local.release,i.command==p::Command::Announcement);if(!exception)op.calls|=16;}
        else if(i.command==p::Command::UseCustom){confirmation=!i.custom&&(before.text[2]!=initial.announcement||before.text[3]!=initial.changelog);exception=n::custom(local.release,before.custom,i.custom||confirmation);if(!exception)op.calls|=32;}
        else{exception=n::custom(local.release,before.custom,false);if(!exception){exception=n::reset(r->image,local.release,false);if(!exception){op.calls|=64;exception=n::reset(r->image,local.release,true);if(!exception)op.calls|=128;}}}
    }
    if(exception)return unknown("Release original edit/owning cleanup SEH="+std::to_string(exception)+" calls="+std::to_string(op.calls)+"; no replay");op.returned=true;
    p::Local after_local;p::State after;if(!sample(*r,c,b,after_local,after,op.evidence.after,e)||after_local!=local||!p::transition(i,before,after,initial,confirmation,e)||op.evidence.after!=money||!defaults(*r,local.release,final,op,e))return unknown(e.empty()?"Release original owner/Ledger differs":e);
    p::Local final_local;p::State final_state;a::Ledger final_money;if(!sample(*r,c,b,final_local,final_state,final_money,e)||final_local!=after_local||final_state!=after||final_money!=money)return unknown(e.empty()?"Release final defaults changed actual gameplay/RNG/Ledger":e);
    p::Proof after_proof;if(!p::proof(after,final,confirmation,hash,after_proof,e)||!p::effect(i,proof,after_proof,e))return unknown(e);op.evidence.release_edit_after=after_proof;
    if(host&&(after_proof!=*host->release_edit_after||op.evidence.after!=host->after))return unknown("Release Client actual original result/defaults differs from Host");op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}
bool observe(const a::Binding& b,const a::Request& q,a::Evidence& out,std::string& e){auto* r=published.load(std::memory_order_acquire);if(!r)return fail(e,"Release observation owner absent");if(q.action==a::Action::ReleaseCycle)return cycle_observe(*r,b,q,out,e);const auto found=r->operations.find(q.action_uuid);
    if(found==r->operations.end()||found->second.binding!=b||found->second.request!=q||!found->second.confirmed)return fail(e,"Release exact confirmed original receipt absent");const auto& op=found->second;p::Local local;p::State state;a::Ledger money;
    if(!sample(*r,op.context,b,local,state,money,e))return false;const auto& proof=*op.evidence.release_edit_after;
    if(state.version!=proof.version||state.planned!=proof.planned||state.type!=proof.type||state.custom!=proof.custom||money!=op.evidence.after)return fail(e,"Release current adopted version/custom/Ledger changed before retirement");for(size_t n=0;n<4;++n)if(hash(state.text[n])!=proof.text_sha256[n])return fail(e,"Release current owned text changed before retirement");out=op.evidence;out.retirement_confirmed=op.retired;e.clear();return true;
}
bool retire(const a::Binding& b,const a::Request& q,std::string& e){auto out=std::make_unique<a::Evidence>();if(!observe(b,q,*out,e))return false;auto* r=published.load(std::memory_order_acquire);if(q.action==a::Action::ReleaseCycle){cycle_runtime.operations.at(q.action_uuid).retired=true;return true;}auto& op=r->operations.at(q.action_uuid);if(!presentation(*r,op,e)){op.cause=e;r->unknown=true;return false;}op.retired=true;return true;}
nlohmann::json status(){auto* r=published.load(std::memory_order_acquire);if(!r)return {{"ready",false}};auto operations=nlohmann::json::object();for(const auto& [id,op]:r->operations)operations[id]={{"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},{"calls",op.calls},{"default_steps",op.default_steps},{"ui_attempted",op.ui_attempted},{"ui_returned",op.ui_returned},{"cause",op.cause},{"before",op.evidence.release_edit_before?p::encode(*op.evidence.release_edit_before):nlohmann::json(nullptr)},{"after",op.evidence.release_edit_after?p::encode(*op.evidence.release_edit_after):nlohmann::json(nullptr)}};
    return {{"cycle",cycle_status()},{"ready",r->ready},{"unknown",r->unknown},{"captures",r->captures},{"submitted",r->submitted},{"rejected",r->rejected},{"pending",r->pending.has_value()},{"cause",r->cause},{"operations",operations}};
}
bool framework_retire(std::string& e){auto* r=published.load(std::memory_order_acquire);if(!r){e.clear();return true;}return cycle_runtime.boundary.unbind(e);}
#include "release_live_port.inc"
}

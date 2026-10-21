#include "zone_edit_runtime.h"
#include "district_stroke_runtime.h"
#include "npc_target_key.h"
#include "snapshot_building_key.h"
#include "ui_remove_runtime.h"
#include "network_remove_runtime.h"
#include "ui_path_runtime.h"
#include "group_object_runtime.h"
#include "ui_remove/native_entry_windows.h"
#include "ui_remove/native_contract_bytes.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include "typed_action/action_session.h"
#include "mt2mp/game/native_engine.h"
#include "typed_action_runtime.h"
#include "remove_diagnostics.h"
#include "npc_probe.h"
#include "monster_action_runtime.h"
#include "dungeon_input_runtime.h"
#include "object_identity/native_free_runtime/native_free_runtime.h"
#include "object_identity/remove_runtime/remove_runtime.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/overlay/notification_overlay.h"
#include <Windows.h>
#include <bcrypt.h>
#include "mt2mp/hooks/minhook_backend.h"
#include <atomic>
#include <cstdio>
#include <limits>
#include <regex>
#include <charconv>
#include <algorithm>
#include <stdexcept>
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/core/framework_runtime.h"
#endif

namespace mt2mp::game::ui_remove_runtime {
namespace {
namespace ui=action_session::ui_capture;
using json=nlohmann::json;
class ProcessMemory final:public ui::Memory {
public:
    bool copy(uintptr_t address,void* out,size_t bytes,std::string& error)override {
        if(!address||!out||!bytes||address>UINTPTR_MAX-bytes){error="UI Remove native read address/size invalid";return false;}
        SIZE_T actual=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),out,bytes,&actual)||actual!=bytes) {
            error="UI Remove ReadProcessMemory address="+std::to_string(address)+" bytes="+std::to_string(bytes)+
                " actual="+std::to_string(actual)+" Win32="+std::to_string(GetLastError());return false;
        }
        return true;
    }
    bool region(uintptr_t address,ui::Region& out,std::string& error)override {
        out={};MEMORY_BASIC_INFORMATION actual{};
        if(VirtualQuery(reinterpret_cast<const void*>(address),&actual,sizeof actual)!=sizeof actual) {
            error="UI Remove VirtualQuery address="+std::to_string(address)+" Win32="+std::to_string(GetLastError());return false;
        }
        const auto protection=actual.Protect&0xff;
        out={uintptr_t(actual.AllocationBase),actual.State==MEM_COMMIT,
            !(actual.Protect&PAGE_GUARD)&&(protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ||
                protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY),actual.Type==MEM_IMAGE,
            protection==PAGE_READWRITE||protection==PAGE_WRITECOPY||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY};
        return true;
    }
};
void entry_error(const std::string& error) {LOG_ERROR("[UIRemove] Native entry failed cause={}",error);}
ui::LocalBinding local_binding(const remove_diagnostics::LocalTarget& value) {
    return {value.manager,value.object,value.uid,value.world_epoch,value.world_generation,value.snapshot_id,value.record_key,
        value.normal_building,value.normal_scenery};
}
#ifdef MT2MP_FRAMEWORK_PLUGIN
struct FrameworkRemoveInvocation {
    MT2FW_MT2_RemoveCall call{};
    MT2FW_MT2_RemoveNext next=nullptr;
    void* invocation=nullptr;
    FrameworkRemoveInvocation* previous=nullptr;
    bool attempted=false,returned=false;
};
#endif
struct Runtime;
class Services final:public ui::Services {
public:
    uint32_t thread=0;uintptr_t image=0;ProcessMemory memory;
    ui::Intent last_intent;
#ifdef MT2MP_FRAMEWORK_PLUGIN
    FrameworkRemoveInvocation* framework_invocation=nullptr;
    uintptr_t framework_trampoline=0;
#endif
    bool on_game_thread()noexcept override{return thread&&GetCurrentThreadId()==thread;}
    ui::Policy policy(std::string& error)override {
        if(!on_game_thread()){error="UI Remove policy requires bound Mainthread";return ui::Policy::Unavailable;}
        const auto current=typed_action_runtime::status();
        if(!Config::instance().local_test&&!current.value("initialized",false))return ui::Policy::Standalone;
        if(!net::session_wire::connected()||!current.value("started",false)||current.value("locked",true)||current.value("pending",true)) {
            error="UI Remove cooperative action authority is disconnected, inactive, locked or pending";return ui::Policy::Unavailable;
        }
        return ui::Policy::AuthorityRequired;
    }
    bool array(ui::NativePropArray* actual,ui::ArrayObservation& out,std::string& error)override {
        return ui::read_native_array(memory,image,actual,out,error);
    }
    bool scene_collection(uintptr_t manager,ui::NativePropArray* array,bool& handled,std::string& error)override {
        return npc_probe::monster_action_runtime::dungeon_input::capture_scene_remove(manager,uintptr_t(array),handled,error);
    }
    bool resolve(uintptr_t manager,uintptr_t object,ui::LocalBinding& out,std::string& error)override {
        uintptr_t ap=0;
        if(!memory.copy(object,&ap,sizeof ap,error))return false;
        if(ap==image+0x1745ce0)return npc_probe::zone_edit_runtime::remove_local_binding(manager,object,out,error);
        if(ap==image+0x16da000)return ui_move_runtime::district_stroke::remove_local_binding(manager,object,out,error);
        if(ap==image+0x1743f10)return ui_path_runtime::remove_local_binding(manager,object,out,error);
        if(ap==image+0x1749de0)return ui_move_runtime::group_object::remove_local_binding(manager,object,out,error);
        if(ap==image+0x16fc700||ap==image+0x16fca60)return network_remove_runtime::local_binding(manager,object,out,error);
        if(ap==image+0x1742980)
            return npc_probe::action_runtime::remove_local_binding(manager,object,out,error);
        if(ap==image+0x16c0770)
            return npc_probe::monster_action_runtime::remove_local_binding(manager,object,out,error);
        remove_diagnostics::LocalTarget actual;
        if(!remove_diagnostics::resolve_local_target(manager,object,actual,error))return false;
        std::string scene;bool dungeon=false;
        if(!remove_diagnostics::dungeon_building_scene(actual,scene,dungeon,error))return false;
        if(!actual.normal_building&&!actual.normal_scenery&&!dungeon){error=actual.kind_cause.empty()?"UI Remove target is not a supported current Building":actual.kind_cause;return false;}
        out=local_binding(actual);out.supported_dungeon_building=dungeon;return true;
    }
    bool fresh_action_uuid(std::string& out,std::string& error)override {
        unsigned char bytes[16];const auto result=BCryptGenRandom(nullptr,bytes,sizeof bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if(result!=0){error="UI Remove BCryptGenRandom NTSTATUS="+std::to_string(result);return false;}
        bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;char text[37];
        std::snprintf(text,sizeof text,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
        out=text;return true;
    }
    bool submit(const ui::Intent& intent,std::string& error)override {
        json result;last_intent=intent;
        return typed_action_runtime::request_remove({{"action_uuid",intent.action_uuid},{"record_key",intent.canonical_record_key}},result,error);
    }
    bool submit_npc(const ui::Intent& intent,std::string& error)override {
        if(!npc_target_key::valid(intent.canonical_record_key)){
            error="NPC UI target has no Place action key";return false;}
        const auto target=npc_target_key::argument(intent.canonical_record_key);
        if(!typed_action_runtime::submit_remove_npc(intent.action_uuid,target,error))return false;
        last_intent=intent;
        npc_probe::action_runtime::remove_submitted(intent.action_uuid,target);
        return true;
    }
    bool submit_monster(const ui::Intent& intent,std::string& error)override {
        if(!npc_probe::monster_action_runtime::submit_remove(intent,error))return false;
        last_intent=intent;return true;
    }
    bool submit_network(const ui::Intent& intent,std::string& error)override {if(!network_remove_runtime::submit(intent,error))return false;last_intent=intent;return true;}
    bool submit_group(const ui::Intent& intent,std::string& error)override {if(!ui_move_runtime::group_object::submit_remove(intent,error))return false;last_intent=intent;return true;}
    bool submit_path(const ui::Intent& intent,std::string& error)override {
        if(!ui_path_runtime::submit_remove(intent,error))return false;
        last_intent=intent;return true;
    }
    bool submit_district(const ui::Intent& intent,std::string& error)override {
        if(!ui_move_runtime::district_stroke::submit_remove(intent,error))return false;last_intent=intent;return true;
    }
    bool submit_zone(const ui::Intent& intent,std::string& error)override {
        if(!npc_probe::zone_edit_runtime::submit_remove(intent,error))return false;last_intent=intent;return true;
    }
    bool authorize_replay(const ui::Replay& replay,std::string& error)override {
        if(replay.binding.supported_zone)return npc_probe::zone_edit_runtime::authorize_remove_replay(replay,error);
        if(replay.binding.supported_district)return ui_move_runtime::district_stroke::authorize_remove_replay(replay,error);
        if(replay.binding.supported_group)return ui_move_runtime::group_object::authorize_remove_replay(replay,error);
        if(replay.binding.supported_network)return network_remove_runtime::authorize(replay,error);
        if(replay.binding.supported_monster)return npc_probe::monster_action_runtime::authorize_remove_replay(replay,error);
        if(replay.binding.supported_npc)return npc_probe::action_runtime::authorize_remove_replay(replay,error);
        return typed_action_runtime::authorize_ui_replay(replay,error);
    }
    void original(ui::DestroyProps function,void* manager,ui::NativePropArray* array)override {
#ifdef MT2MP_FRAMEWORK_PLUGIN
        auto* current=framework_invocation;
        if(!on_game_thread()||!current||!current->next||!current->invocation||current->attempted||
           current->call.manager!=manager||current->call.array!=array||uintptr_t(function)!=framework_trampoline)
            throw std::runtime_error("UI Remove OriginalOnce lacks its exact active Framework continuation");
        current->attempted=true;
        const auto result=current->next(current->invocation);
        if(result!=MT2FW_MT2_OK)throw std::runtime_error("UI Remove Framework original did not return result="+std::to_string(result)+"; no replay");
        current->returned=true;
#else
        function(manager,array);
#endif
    }
    void log(const char* level,const char* operation,const std::string& error)noexcept override {
        if(std::string_view(level)=="error")LOG_ERROR("[UIRemove] {}: {}",operation,error);
        else if(std::string_view(level)=="warn")LOG_WARN("[UIRemove] {}: {}",operation,error);
        else LOG_INFO("[UIRemove] {}: {}",operation,error);
    }
    void show_rejection(const std::string& error)noexcept override {
        overlay::NotificationOverlay::instance().show("Entfernen abgelehnt: "+error,overlay::NotificationType::Error,10.0f);
    }
};
struct Runtime {
    Services services;ui::Capture capture{services};ui::Publication publication;
    bool attempted=false,created=false,enabled=false;std::string cause;
#ifdef MT2MP_FRAMEWORK_PLUGIN
    uint64_t framework_remove_token=0,framework_remove_generation=0,framework_remove_invocation=0;
    uintptr_t framework_remove_detour=0;
#endif
    std::map<std::string,json> diagnostics;
    Runtime(){services.thread=GetCurrentThreadId();publication.capture=&capture;publication.memory=&services.memory;
        publication.game_thread=services.thread;publication.entry_error=&entry_error;}
};
std::atomic<Runtime*> published{nullptr};
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "framework_remove_authority.inc"
#endif
bool ledger(action_session::Ledger& out,std::string& error) {
    native::SimulationState value;if(!native::read_simulation_state(value,error))return false;
    out={value.cash,value.clock.flat_day,int32_t(value.record_present),value.record_day,value.record_cash,value.record_income,value.record_expense,
        value.line_counts,value.line_income};return true;
}
class DiagnosticServices final:public action_session::ui_diagnostic::RootServices {
    Runtime& runtime_;remove_diagnostics::LocalTarget target_;action_session::Ledger before_;
    uint64_t captured_=0,attempted_=0;bool client_=false;
public:
    explicit DiagnosticServices(Runtime& runtime):runtime_(runtime){}
    bool preflight(const action_session::ui_diagnostic::Request& request,action_session::ui_diagnostic::Context& out,std::string& error)override {
        if(!runtime_.services.on_game_thread()||!runtime_.enabled||runtime_.capture.status().replay_active||runtime_.capture.status().unknown) {
            error="native UI diagnostic requires enabled idle nonUnknown Mainthread capture";return false;
        }
        if(runtime_.diagnostics.contains(request.diagnostic_uuid)){error="native UI diagnostic UUID already admitted; no replay";return false;}
        if(runtime_.diagnostics.size()>=128){error="native UI diagnostic journal capacity actual=128 required<128";return false;}
        runtime_.diagnostics.emplace(request.diagnostic_uuid,json{{"attempted",true},{"confirmed",false}});
        if(runtime_.services.policy(error)!=ui::Policy::AuthorityRequired)return false;
        ui::Replay actual;
        if(!make_replay(request.snapshot_uuid,request.canonical_record_key,request.diagnostic_uuid,ui::ReplayPurpose::HostExecution,actual,error))return false;
        if(!actual.binding.supported_normal_building&&!actual.binding.supported_normal_scenery){error="native UI diagnostic target is not snapshot-bound normal Building or Scenery";return false;}
        if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target_,error)||!ledger(before_,error))return false;
        if(actual.binding!=local_binding(target_)){error="native UI diagnostic complete target binding changed";return false;}
        native_free_runtime::Receipt heap;if(!native_free_runtime::Runtime::instance().read_actual_receipt(heap,error))return false;
        captured_=runtime_.capture.status().captured;attempted_=typed_action_runtime::status().at("attempted").get<uint64_t>();
        client_=!Config::instance().is_host;
        out={heap.image,target_.manager,target_.object,target_.uid,target_.world_epoch,target_.world_generation,heap.executable.sha256.data(),target_.normal_scenery?action_session::ui_diagnostic::Family::Scenery:action_session::ui_diagnostic::Family::Building};
        return true;
    }
    bool preparation_unchanged(const action_session::ui_diagnostic::Context& context,const action_session::ui_diagnostic::Window& window,std::string& error)override {
        remove_diagnostics::LocalTarget fresh;action_session::Ledger after;
        if(!remove_diagnostics::resolve_record_target(target_.snapshot_id,target_.record_key,fresh,error)||!ledger(after,error))return false;
        if(local_binding(fresh)!=local_binding(target_)||after!=before_||window.target!=context.target||context.target!=target_.object||
           context.manager!=target_.manager||context.epoch!=target_.world_epoch||context.generation!=target_.world_generation) {
            error="native UI selection preparation changed complete binding or full typed ledger";return false;
        }
        return true;
    }
    bool observed_intent(const action_session::ui_diagnostic::Request& request,const action_session::ui_diagnostic::Context&,std::string& error)override {
        const auto capture=runtime_.capture.status();const auto controller=typed_action_runtime::status();
        const auto& intent=runtime_.services.last_intent;
        if(captured_==UINT64_MAX||capture.captured!=captured_+1||capture.unknown||
           runtime_.publication.last_origin.load()!=uint8_t(target_.normal_scenery?ui::Origin::OtherWindow:ui::Origin::BuildingWindow)||
           runtime_.publication.last_disposition.load()!=uint8_t(ui::Disposition::IntentCaptured)||
           intent.snapshot_uuid!=request.snapshot_uuid||intent.canonical_record_key!=request.canonical_record_key||intent.action_uuid.empty()||
           controller.value("action_uuid",std::string{})!=intent.action_uuid||!controller.value("started",false)||controller.value("locked",true)) {
            error="native UI BuildingWindow capture and actual typed intent acknowledgment differ";return false;
        }
        if(client_) {
            action_session::Ledger after;if(!ledger(after,error))return false;
            if(after!=before_||controller.at("attempted").get<uint64_t>()!=attempted_||!controller.value("pending",false)) {
                error="native Client UI changed ledger or executed native removal before Host authority";return false;
            }
        }
        return true;
    }
    void log(const char* operation,const std::string& error)noexcept override {LOG_ERROR("[UIRemove] Diagnostic stage={} cause={}",operation,error);}
};
json diagnostic_receipt(const action_session::ui_diagnostic::Receipt& value) {
    return {{"selection_attempted",value.selection_attempted},{"selection_returned",value.selection_returned},{"window_confirmed",value.window_confirmed},
        {"record_constructed",value.record_constructed},{"record_label_confirmed",value.record_label_confirmed},
        {"ui_command_attempted",value.ui_command_attempted},{"ui_command_returned",value.ui_command_returned},{"ui_command_accepted",value.ui_command_accepted},
        {"record_cleanup_attempted",value.record_cleanup_attempted},{"record_cleanup_confirmed",value.record_cleanup_confirmed},
        {"intent_observed",value.intent_observed},{"native_exception",value.native_exception},{"cleanup_exception",value.cleanup_exception},
        {"failed_stage",value.failed_stage},{"cause",value.cause}};
}
}
bool enable(std::string& error)try {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime){runtime=new Runtime;published.store(runtime,std::memory_order_release);}
    if(!runtime->services.on_game_thread()){error="UI Remove installation requires bound Mainthread";return false;}
    if(runtime->attempted){error="UI Remove installation already attempted; no reinstall";return false;}
    runtime->attempted=true;
    auto fail=[&](){runtime->cause=error;LOG_ERROR("[UIRemove] Installation failed cause={}",error);return false;};
    const auto action=typed_action_runtime::status();
    if(!action.value("started",false)||action.value("locked",true)||action.value("pending",true)) {
        error="UI Remove installation requires started idle typed Controller";return fail();
    }
    auto& free=native_free_runtime::Runtime::instance();
    if(free.status().phase==native_free_runtime::Phase::Idle&&!free.prepare_and_enable(error))return fail();
    native_free_runtime::Receipt actual;if(!free.read_actual_receipt(actual,error))return fail();
    auto& removal=remove_runtime::Runtime::instance();
    if(removal.status().phase==remove_runtime::RuntimePhase::Idle&&!removal.prepare_and_enable(error))return fail();
    const auto installed=removal.status();uint32_t mask=0;
    if(installed.phase!=remove_runtime::RuntimePhase::Enabled) {
        error="UI Remove installation phase actual="+std::to_string(int(installed.phase))+" required="+
            std::to_string(int(remove_runtime::RuntimePhase::Enabled))+" cause="+installed.cause;return fail();
    }
    if(!installed.originals_published){error="UI Remove original publication actual=false required=true";return fail();}
    if(installed.main_thread!=GetCurrentThreadId()) {
        error="UI Remove removal Mainthread actual="+std::to_string(installed.main_thread)+" required="+std::to_string(GetCurrentThreadId());return fail();
    }
    if(installed.image!=actual.image){error="UI Remove current executable image match actual=false required=true";return fail();}
    if(installed.inflight!=0){error="UI Remove removal entries in flight actual="+std::to_string(installed.inflight)+" required=0";return fail();}
    if(!removal.read_actual_hook_mask(mask,error))return fail();
    if(mask!=15){error="UI Remove actual removal hook mask="+std::to_string(mask)+" required=15";return fail();}
    runtime->services.image=actual.image;runtime->publication.image=actual.image;
#ifdef MT2MP_FRAMEWORK_PLUGIN
    if(std::string_view(actual.executable.sha256.data())!=ui::contract::exe_sha256){error="UI Remove exact Framework executable identity differs";return fail();}
    if(!framework_remove_route(*runtime,error))return fail();
    runtime->services.framework_trampoline=runtime->publication.trampoline;
    if(!runtime->capture.publish_disabled(reinterpret_cast<ui::DestroyProps>(runtime->publication.trampoline),error)||!runtime->capture.set_enabled(true,error))return fail();
    const auto& routes=framework_runtime::routes();
    const auto result=routes.remove_bind(routes.context,&framework_remove,runtime,&runtime->framework_remove_token);
    if(result!=MT2FW_MT2_OK||!runtime->framework_remove_token){error="UI Remove Framework authority binding failed result="+std::to_string(result);return fail();}
    runtime->created=true;
#else
    if(!ui::validate_unpatched(runtime->services.memory,actual.image,actual.executable.sha256.data(),error))return fail();
    void* original=nullptr;
    if(!hooks::create_hook(reinterpret_cast<void*>(actual.image+ui::contract::destroy_props_rva),
        reinterpret_cast<void*>(ui::entry_address()),&original)){error="UI Remove shared MinHook CreateHook failed; see original HookEngine cause";return fail();}
    runtime->created=true;runtime->publication.trampoline=uintptr_t(original);
    if(!ui::publish_disabled(runtime->publication,actual.executable.sha256.data(),error))return fail();
    if(!runtime->capture.set_enabled(true,error))return fail();
    const auto result=MH_EnableHook(reinterpret_cast<void*>(actual.image+ui::contract::destroy_props_rva));
    if(result!=MH_OK){error="UI Remove MH_EnableHook: "+std::string(MH_StatusToString(result));return fail();}
    if(!ui::enabled_route(runtime->publication,error))return fail();
#endif
    runtime->enabled=true;runtime->cause.clear();
    LOG_INFO("[UIRemove] Normal DestroyProps capture enabled Mainthread={} targetRVA={}",actual.main_thread,ui::contract::destroy_props_rva);
    error.clear();return true;
}catch(const std::exception& failure){error=failure.what();LOG_ERROR("[UIRemove] Installation exception cause={}",error);return false;}
catch(...){error="UI Remove installation nonstandard exception";LOG_ERROR("[UIRemove] Installation exception cause={}",error);return false;}
#ifdef MT2MP_FRAMEWORK_PLUGIN
bool framework_retire(std::string& error)try {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||(!runtime->framework_remove_token&&!runtime->enabled)){error.clear();return true;}
    if(!runtime->services.on_game_thread()||runtime->publication.inflight.load(std::memory_order_acquire)||runtime->services.framework_invocation||runtime->capture.status().replay_active){error="Framework Remove authority retirement waits for idle bound-thread scopes";return false;}
    if(runtime->framework_remove_token){
        const auto& routes=framework_runtime::routes();
        const auto result=routes.remove_unbind(routes.context,runtime->framework_remove_token);
        if(result!=MT2FW_MT2_OK){error="Framework Remove authority retirement failed result="+std::to_string(result);return false;}
        runtime->framework_remove_token=0;
    }
    if(!runtime->capture.set_enabled(false,error))return false;
    runtime->enabled=false;error.clear();return true;
}catch(const std::exception& problem){error=std::string("Framework Remove authority retirement exception: ")+problem.what();return false;}
catch(...){error="Framework Remove authority retirement nonstandard exception";return false;}
#endif
bool enabled()noexcept {
    const auto* runtime=published.load(std::memory_order_acquire);return runtime&&runtime->services.thread==GetCurrentThreadId()&&runtime->enabled;
}
bool requested()noexcept {
    const auto* runtime=published.load(std::memory_order_acquire);return runtime&&runtime->attempted;
}
bool make_replay(const std::string& snapshot,const std::string& key,const std::string& action,
    ui::ReplayPurpose purpose,ui::Replay& out,std::string& error) {
    out={};auto* runtime=published.load(std::memory_order_acquire);
    if(!enabled()){error="UI Remove actual capture is not enabled on Mainthread";return false;}
#ifdef MT2MP_FRAMEWORK_PLUGIN
    if(!runtime->framework_remove_token||!framework_remove_route(*runtime,error))return false;
#else
    if(!ui::validate_own_enabled_route(runtime->services.memory,runtime->services.image,runtime->publication.trampoline,uintptr_t(ui::entry_address()),error))return false;
#endif
    remove_diagnostics::LocalTarget target;
    if(!remove_diagnostics::resolve_record_target(snapshot,key,target,error))return false;
    std::string scene;bool dungeon=false;
    if(!remove_diagnostics::dungeon_building_scene(target,scene,dungeon,error))return false;
    if(!target.normal_building&&!target.normal_scenery&&!dungeon){error=target.kind_cause.empty()?"UI Remove replay target is not a supported current Building":target.kind_cause;return false;}
    out={purpose,{action,snapshot,key},local_binding(target)};out.binding.supported_dungeon_building=dungeon;return true;
}
bool with_replay(const ui::Replay& replay,const std::function<bool(std::string&)>& callback,std::string& error) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!enabled()){error="UI Remove own replay requires enabled Mainthread capture";return false;}
    return runtime->capture.with_replay(replay,callback,error);
}
bool can_reclaim_global_minhook(std::string& error) {
    const auto* runtime=published.load(std::memory_order_acquire);
    if(runtime&&runtime->created){error="UI Remove process-lifetime native route/original publication retained";return false;}
    return true;
}
json status() {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime)return {{"initialized",false}};
    if(!runtime->services.on_game_thread())return {{"initialized",false},{"error","UI Remove status requires bound Mainthread"}};
    const auto current=runtime->capture.status();
    return {{"initialized",true},{"attempted",runtime->attempted},{"created",runtime->created},{"enabled",runtime->enabled},
        {"captured",current.captured},{"rejected",current.rejected},{"foreign_originals",current.foreign_originals},
        {"replay_originals",current.replay_originals},{"unknown",current.unknown},{"replay_active",current.replay_active},
        {"inflight",runtime->publication.inflight.load()},{"cause",current.cause.empty()?runtime->cause:current.cause},
        {"last_disposition",runtime->publication.last_disposition.load()},{"last_original_attempted",runtime->publication.last_original_attempted.load()},
        {"last_original_returned",runtime->publication.last_original_returned.load()},{"last_origin",runtime->publication.last_origin.load()},
        {"last_intent",{{"action_uuid",runtime->services.last_intent.action_uuid},{"snapshot_uuid",runtime->services.last_intent.snapshot_uuid},
            {"record_key",runtime->services.last_intent.canonical_record_key}}},{"diagnostic_receipts",runtime->diagnostics}};
}
bool probe(const json& args,json& out,std::string& error) {
    out=json::object();static const std::regex uuid("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    static const std::regex key("^action:[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    if(!args.is_object()||args.size()!=3||!args.contains("diagnostic_uuid")||!args.contains("snapshot_uuid")||!args.contains("record_key")||
       !args.at("diagnostic_uuid").is_string()||!args.at("snapshot_uuid").is_string()||!args.at("record_key").is_string()) {
        error="native UI diagnostic requires exact scalar diagnostic_uuid, snapshot_uuid and record_key";return false;
    }
    const action_session::ui_diagnostic::Request request{args.at("diagnostic_uuid").get<std::string>(),args.at("snapshot_uuid").get<std::string>(),args.at("record_key").get<std::string>()};
    if(!std::regex_match(request.diagnostic_uuid,uuid)||!std::regex_match(request.snapshot_uuid,uuid)||!(std::regex_match(request.canonical_record_key,key)||snapshot_building_key::valid(request.canonical_record_key))) {
        error="native UI diagnostic scalar UUID or record key invalid";return false;
    }
    if(request.canonical_record_key.rfind("action:",0)!=0) {
        int64_t link=0;const auto* begin=request.canonical_record_key.data()+80;const auto* end=request.canonical_record_key.data()+request.canonical_record_key.size();
        const auto parsed=std::from_chars(begin,end,link);
        if(parsed.ec!=std::errc{}||parsed.ptr!=end||link<=0) {
            error="native UI diagnostic building linkID requires positive int64";return false;
        }
    }
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->services.on_game_thread()){error="native UI diagnostic actual Mainthread runtime unavailable";return false;}
    if(const auto previous=runtime->diagnostics.find(request.diagnostic_uuid);previous!=runtime->diagnostics.end()) {
        out=previous->second;error="native UI diagnostic UUID already attempted; no replay";return false;
    }
    DiagnosticServices services(*runtime);action_session::ui_diagnostic::Receipt receipt;bool accepted=false;
    try {accepted=action_session::ui_diagnostic::invoke(request,services,runtime->services.memory,action_session::ui_diagnostic::process_calls(),receipt,error);}
    catch(const std::exception& failure){error=failure.what();receipt.cause=error;receipt.failed_stage="root_exception";}
    catch(...){error="native UI diagnostic nonstandard exception";receipt.cause=error;receipt.failed_stage="root_exception";}
    if(!accepted&&error.empty())error="native UI diagnostic failed without original cause";
    out={{"diagnostic_uuid",request.diagnostic_uuid},{"snapshot_uuid",request.snapshot_uuid},{"record_key",request.canonical_record_key},
        {"confirmed",accepted},{"receipt",diagnostic_receipt(receipt)},{"action_uuid",accepted?runtime->services.last_intent.action_uuid:std::string{}},
        {"ui_captured",runtime->capture.status().captured},{"ui_replay_originals",runtime->capture.status().replay_originals},
        {"ui_origin",runtime->publication.last_origin.load()},{"ui_disposition",runtime->publication.last_disposition.load()},
        {"action_at_return",typed_action_runtime::status()},{"error",error}};
    if(runtime->diagnostics.contains(request.diagnostic_uuid))runtime->diagnostics.at(request.diagnostic_uuid)=out;
    if(!accepted) {
        LOG_ERROR("[UIRemove] Diagnostic failed UUID={} stage={} cause={}",request.diagnostic_uuid,receipt.failed_stage,error);
        if(receipt.selection_attempted||receipt.ui_command_attempted||receipt.record_constructed)runtime->capture.record_entry_failure(error);
    } else LOG_INFO("[UIRemove] Actual BuildingWindow intent admitted UUID={} action={}",request.diagnostic_uuid,runtime->services.last_intent.action_uuid);
    return accepted;
}
}

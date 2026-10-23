#include "ui_build_runtime.h"
#include "dungeon_build_cursor.h"
#include "dungeon_build_local_ui.h"
#include "object_identity/remove_runtime/copy/selection_witness.h"
#include "dungeon_build_cursor_guard.h"
#include "dungeon_build_calls.h"
#include "live_host_state/apply_timing.h"
#include "live_host_state/memory_query_timing.h"
#include "mt2mp/hooks/original_body.h"
#include "dungeon_scene_identity_runtime.h"
#include "scenery_stroke_calls.h"
#include "scenery_kiss/native_scenery.h"
#include "scenery_kiss/native_catalog.h"
#include "scenery_kiss/ui_contract.h"
#include "scenery_kiss/ui_native_gate.h"
#include "placement_kiss/native_build.h"
#include "typed_action_runtime.h"
#include "scenery_stroke_runtime.h"
#include "building_history_slice/native_bridge.h"
#include "placement_ui_diagnostics/placement_ui_diagnostics.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "object_identity/remove_runtime/copy/undo_witness.h"
#include "object_identity/native_free_runtime/native_free_runtime.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/overlay/notification_overlay.h"
#include <Windows.h>
#include "mt2mp/hooks/minhook_backend.h"
#include <bcrypt.h>
#include <atomic>
#include <cstdio>
#include <cmath>
#include <intrin.h>
#include <optional>
#include <exception>
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/core/framework_runtime.h"
#endif

namespace mt2mp::game::native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld&,std::string&);
}

namespace mt2mp::game::ui_build_runtime {
namespace {
using json=nlohmann::json;
namespace action=action_session;
namespace host=build_actions::private_native::host;
using Purchase=bool(__fastcall*)(const void*,const void*);
using Add=void*(__fastcall*)(void*,const void*,const void*,const void*,void*,void*,void*,bool);
using Undo=void(__fastcall*)(void*,const void*,void*);
using ScenePurchase=bool(__fastcall*)(int32_t,const void*);
using SceneAdd=void*(__fastcall*)(void*,int32_t,int32_t,const void*,bool);
using Press=void(__fastcall*)(void*,void*,const void*);
struct Scope {
    dungeon_build_cursor::Scope cursor;
    bool scenery=false;int32_t category=-1,variant=0;void* manager=nullptr;
    std::array<float,3> position{};
    uint32_t purchase_attempted=0,purchase_returned=0,add_attempted=0,add_returned=0,undo_attempted=0,undo_returned=0;
    int32_t cost=-1;
    void* building=nullptr;
    uintptr_t graph=0;
    std::string cause;
};
struct DeferredBuilding {
    placement_kiss::Request request;
    uintptr_t receiver=0,ghost=0,blueprint=0;
    host::opened::CurrentWorld world;
    void* shard=nullptr;
    std::string action_uuid,error,scene;
};
struct Runtime {
    uint32_t thread=GetCurrentThreadId();uintptr_t image=0;
    bool attempted=false,ready=false,created=false,replaying=false;
    std::string cause;
    placement_kiss::NativeBuild build;
    scenery_kiss::NativeScenery scene;
    scenery_kiss::Update scene_update=nullptr;ScenePurchase scene_purchase=nullptr;SceneAdd scene_add=nullptr;
    Press scene_press=nullptr,erase_press=nullptr,scene_release=nullptr;
    uintptr_t ui_scene_receiver=0;
    std::array<uintptr_t,6> scene_originals{};
    std::map<std::string,scenery_kiss::PrivateScenery> held_scenery;
    placement_kiss::Update original=nullptr;
    Purchase purchase=nullptr;Add add=nullptr;Undo undo=nullptr;
    using CursorSet=void(__fastcall*)(void*,void*);
    CursorSet cursor_behaviour=nullptr,cursor_selection=nullptr;
    Scope* scope=nullptr;
#ifdef MT2MP_FRAMEWORK_PLUGIN
    struct CreatePair {MT2FW_MT2_CreateCall call{};Scope* scope=nullptr;};
    std::array<CreatePair,64> framework_creates{};
    size_t framework_create_depth=0,framework_create_overflow=0;
    uint64_t framework_create_token=0;
    bool framework_create_failed=false;
#endif
    uint64_t captured=0,forwarded=0,rejected=0;
    std::string last_action;
    uintptr_t ui_build_receiver=0;
    std::optional<DeferredBuilding> deferred_build;
    uint64_t purchase_intents=0;
    std::map<std::string,json> receipts;
    struct LocalBuild {uintptr_t receiver=0;dungeon_build_local_ui::Receipt completion;};
    std::map<std::string,LocalBuild> local_builds;
    std::map<std::string,placement_kiss::PrivateBuild> held_builds;
};
std::atomic<Runtime*> published{nullptr};
void cursor_route(bool selected,uintptr_t caller,void* receiver,void* value){
    auto* runtime=published.load(std::memory_order_acquire);
    const auto original=selected?runtime->cursor_selection:runtime->cursor_behaviour;
    if(runtime->thread==GetCurrentThreadId()&&runtime->replaying&&runtime->scope){
        auto& scope=*runtime->scope;scope.cursor.building=uintptr_t(scope.building);
        const auto result=dungeon_build_cursor::route(scope.cursor,selected,caller-runtime->image,uintptr_t(receiver),uintptr_t(value));
        if(result!=dungeon_build_cursor::Route::Forward)return;
    }
    original(receiver,value);
}
void cursor_behaviour_entry(void* receiver,void* value){cursor_route(false,uintptr_t(_ReturnAddress()),receiver,value);}
void cursor_selection_entry(void* receiver,void* value){cursor_route(true,uintptr_t(_ReturnAddress()),receiver,value);}
bool cursor_guards(Runtime& runtime,bool enabled,std::string& error){
    host::ProcessMemory memory;
    return dungeon_build_cursor::guard(memory,runtime.image,dungeon_build_cursor::codes[0],
        uintptr_t(runtime.cursor_behaviour),uintptr_t(&cursor_behaviour_entry),enabled,error)&&
        dungeon_build_cursor::guard(memory,runtime.image,dungeon_build_cursor::codes[1],
        uintptr_t(runtime.cursor_selection),uintptr_t(&cursor_selection_entry),enabled,error);
}
bool read_world(host::opened::CurrentWorld& opened,void*& shard,std::string& error) {
    shard=nullptr;
    if(!native_open_runtime::read_current_world(opened,error))return false;
    auto& free=native_free_runtime::Runtime::instance();
    if(free.status().phase==native_free_runtime::Phase::Idle&&!free.prepare_and_enable(error))return false;
    native_free_runtime::Receipt heap;if(!free.read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState actual;
    if(!reader.current(actual,true,error)||!host::same_world(opened,actual.origin)) {
        if(error.empty())error="building action current world differs";return false;
    }
    shard=reinterpret_cast<void*>(actual.root);
    if(!shard){error="building action root shard unavailable";return false;}
    return true;
}
#include "dungeon_build_view.inc"
#include "dungeon_build_capture.inc"
#include "dungeon_build_tasks.inc"
bool read_cost(uintptr_t image,const void* type,int32_t& out,uint32_t& exception) {
    __try {out=reinterpret_cast<int32_t(__fastcall*)(const void*)>(image+0x40a650)(type);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();return false;}
}
bool building_purchase_gate(Runtime&,uintptr_t,const void*,const void*);
bool purchase_entry(const void* type,const void* position) {
    auto* runtime=published.load(std::memory_order_acquire);
    auto* scope=runtime->thread==GetCurrentThreadId()?runtime->scope:nullptr;
    if(building_purchase_gate(*runtime,uintptr_t(_ReturnAddress()),type,position))return false;
    if(scope&&!scope->scenery) {
        ++scope->purchase_attempted;uint32_t exception=0;
        if(!read_cost(runtime->image,type,scope->cost,exception))
            scope->cause="building native price read SEH="+std::to_string(exception);
    }
    const bool result=runtime->purchase(type,position);
    if(scope&&!scope->scenery){++scope->purchase_returned;if(!result&&scope->cause.empty())scope->cause="native building purchase rejected";}
    return result;
}
#ifndef MT2MP_FRAMEWORK_PLUGIN
void* add_entry(void* manager,const void* type,const void* variant,const void* transform,
                void* layout,void* theme,void* plan,bool purchase) {
    auto* runtime=published.load(std::memory_order_acquire);
    auto* scope=runtime->thread==GetCurrentThreadId()?runtime->scope:nullptr;
    if(scope&&!scope->scenery)++scope->add_attempted;
    void* result=runtime->add(manager,type,variant,transform,layout,theme,plan,purchase);
    if(scope&&!scope->scenery){++scope->add_returned;scope->building=result;}
    return result;
}
#else
#include "framework_create_observer.inc"
#endif
void undo_entry(void* manager,const void* label,void* graph) {
    auto* runtime=published.load(std::memory_order_acquire);
    auto* scope=runtime->thread==GetCurrentThreadId()?runtime->scope:nullptr;
    if(scope){++scope->undo_attempted;scope->graph=uintptr_t(graph);}
    runtime->undo(manager,label,graph);
    if(scope)++scope->undo_returned;
}
std::string action_uuid(std::string& error) {
    unsigned char bytes[16];const auto status=BCryptGenRandom(nullptr,bytes,sizeof bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(status!=0){error="building action BCryptGenRandom NTSTATUS="+std::to_string(status);return {};}
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;char value[37];
    std::snprintf(value,sizeof value,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return value;
}
void reject(Runtime& runtime,const std::string& cause) {
    ++runtime.rejected;
    if(runtime.cause!=cause){runtime.cause=cause;LOG_ERROR("[UIBuild] Capture rejected cause={}",cause);
        overlay::NotificationOverlay::instance().show("Bauen abgelehnt: "+cause,overlay::NotificationType::Error,10.0f);}
}
bool submit_scene_intent(Runtime&,uintptr_t,const void*,std::string&);
bool scene_purchase_entry(int32_t category,const void* position) {
    auto* r=published.load(std::memory_order_acquire);auto* scope=r->thread==GetCurrentThreadId()?r->scope:nullptr;
    const auto caller=uintptr_t(_ReturnAddress());
    if(!scope&&r->thread==GetCurrentThreadId()&&caller==r->image+0x5d48e3&&r->ui_scene_receiver){
        try{std::string e;if(!submit_scene_intent(*r,r->ui_scene_receiver,position,e))reject(*r,e);}
        catch(const std::exception& x){reject(*r,std::string("scenery Purchase capture exception: ")+x.what());}
        catch(...){reject(*r,"scenery Purchase capture nonstandard exception");}
        return false;
    }
    const bool own=scope&&scope->scenery&&caller==r->image+0x5d48e3;
    if(own){host::ProcessMemory memory;if(!scenery_kiss::purchase_input(memory,category,scope->category,uintptr_t(position),scope->position,scope->cause))return false;++scope->purchase_attempted;}
    const bool result=r->scene_purchase(category,position);
    if(own){++scope->purchase_returned;if(!result&&scope->cause.empty())scope->cause="native scenery purchase rejected";}
    return result;
}
void* scene_add_entry(void* manager,int32_t category,int32_t variant,const void* transform,bool purchase) {
    auto* r=published.load(std::memory_order_acquire);auto* scope=r->thread==GetCurrentThreadId()?r->scope:nullptr;
    const bool own=scope&&scope->scenery&&uintptr_t(_ReturnAddress())==r->image+0x5d49b4;
    if(own){++scope->add_attempted;if(manager!=scope->manager||category!=scope->category||variant!=scope->variant||!purchase)scope->cause="scenery native Add binding differs";}
    void* result=r->scene_add(manager,category,variant,transform,purchase);
    if(own){++scope->add_returned;scope->building=result;}return result;
}
bool scene_authority(Runtime&,std::string& e) {
    if(!net::session_wire::connected()){e="scenery cooperative session disconnected";return false;}
    const auto current=typed_action_runtime::status();
    if(!current.value("started",false)||current.value("locked",true)||current.value("pending",true)){e="scenery Controller unavailable, locked or pending";return false;}return true;
}
bool submit_scene_intent(Runtime& r,uintptr_t receiver,const void* purchase_position,std::string& e) {
    host::ProcessMemory memory;bool own=false;
    if(!r.ready||!scene_authority(r,e)||!scenery_kiss::live_cursor(memory,r.image,receiver,own,e)||!own){if(e.empty())e="scenery UI authority or live cursor changed";return false;}
    if(!scenery_kiss::single_press(memory,r.image,receiver,e))return false;
    host::opened::CurrentWorld world;void* shard=nullptr;if(!read_world(world,shard,e))return false;
    scenery_kiss::NativeCatalog catalog(memory,r.image,r.thread);action::BuildingState state;
    if(scenery_kiss::read_single_intent(memory,catalog,r.image,uintptr_t(shard),receiver,state,e)!=scenery_kiss::Capture::Single){if(e.empty())e="scenery Purchase has no complete single intent";return false;}
    if(purchase_position){std::array<float,3> position{};if(!memory.copy(uintptr_t(purchase_position),position.data(),sizeof position,e))return false;
        if(position!=std::array<float,3>{state.x,state.y,state.z}){e="scenery UI Purchase copied pose differs";return false;}}
    uintptr_t ghost=0,blueprint=0,after_ghost=0,after_blueprint=0;
    if(!memory.copy(receiver+0xb58,&ghost,8,e)||!memory.copy(receiver+0xb50,&blueprint,8,e))return false;
    uint32_t exception=0;if(!r.scene.consume_captured_intent(memory,catalog,uintptr_t(shard),receiver,state,exception,e))return false;
    if(!memory.copy(receiver+0xb58,&after_ghost,8,e)||!memory.copy(receiver+0xb50,&after_blueprint,8,e))return false;
    if(ghost!=after_ghost||blueprint!=after_blueprint){e="scenery UI children changed during Clear; no replay";return false;}
    const auto id=action_uuid(e);if(id.empty())return false;
    if(!placement_ui_diagnostics::capture_bound(reinterpret_cast<void*>(receiver),id,state,e)||!typed_action_runtime::submit_place(id,state,e)){placement_ui_diagnostics::capture_failed(reinterpret_cast<void*>(receiver),e);return false;}
    ++r.captured;r.last_action=id;r.cause.clear();LOG_INFO("[UIBuild] Normal scenery intent captured action={} type={} variant={}",id,state.type,state.variant);return true;
}
void scene_original_ui(Runtime& r,void* receiver,float dt,bool capture) {
    const auto prior=r.ui_scene_receiver;r.ui_scene_receiver=capture?uintptr_t(receiver):0;
    struct Restore {Runtime& r;uintptr_t prior;~Restore(){r.ui_scene_receiver=prior;}} restore{r,prior};r.scene_update(receiver,dt);
}
void scene_press_route(void* receiver,void* cursor,const void* point,bool eraser)noexcept {
    auto* r=published.load(std::memory_order_acquire);const auto original=eraser?r->erase_press:r->scene_press;
    if(r->thread!=GetCurrentThreadId()){original(receiver,cursor,point);return;}
    try{
        host::ProcessMemory memory;bool own=false;std::string e;
        if(!scenery_kiss::live_cursor(memory,r->image,uintptr_t(receiver),own,e)){reject(*r,e);return;}
        if(!own){original(receiver,cursor,point);return;}
        if(!r->ready||!scene_authority(*r,e)){if(e.empty())e="scenery capture not ready";reject(*r,e);return;}
        bool bulk=false;
        if(!ui_move_runtime::scenery_stroke::bulk(uintptr_t(receiver),eraser,bulk,e)){reject(*r,e);return;}
        if(bulk){original(receiver,cursor,point);return;}
        if(eraser){reject(*r,"scenery eraser requires an ordered bulk action");return;}
        if(!scenery_kiss::single_press(memory,r->image,uintptr_t(receiver),e)){reject(*r,e);return;}
        original(receiver,cursor,point);
    }catch(const std::exception& x){reject(*r,std::string("scenery press guard exception: ")+x.what());}
    catch(...){reject(*r,"scenery press guard nonstandard exception");}
}
void scene_press_entry(void* receiver,void* cursor,const void* point){scene_press_route(receiver,cursor,point,false);}
void erase_press_entry(void* receiver,void* cursor,const void* point){scene_press_route(receiver,cursor,point,true);}
void scene_release_entry(void* receiver,void* cursor,const void* point) {
    auto* r=published.load(std::memory_order_acquire);
    if(r->thread!=GetCurrentThreadId()||r->replaying){r->scene_release(receiver,cursor,point);return;}
    try{
        host::ProcessMemory memory;std::string e;bool own=false;
        if(!scenery_kiss::live_cursor(memory,r->image,uintptr_t(receiver),own,e)){reject(*r,e);return;}
        if(!own){r->scene_release(receiver,cursor,point);return;}
        if(!r->ready||!scene_authority(*r,e)){reject(*r,e.empty()?"scenery Release capture not ready":e);return;}
        bool bulk=false;
        if(!ui_move_runtime::scenery_stroke::bulk(uintptr_t(receiver),false,bulk,e)){reject(*r,e);return;}
        if(bulk){r->scene_release(receiver,cursor,point);return;}
        if(!scenery_kiss::single_press(memory,r->image,uintptr_t(receiver),e)){reject(*r,e);return;}
        r->scene_release(receiver,cursor,point);
        host::opened::CurrentWorld world;void* shard=nullptr;
        if(!read_world(world,shard,e)){placement_ui_diagnostics::capture_failed(receiver,e);reject(*r,e);return;}
        scenery_kiss::NativeCatalog catalog(memory,r->image,r->thread);action::BuildingState state;
        const auto route=scenery_kiss::read_single_intent(memory,catalog,r->image,uintptr_t(shard),uintptr_t(receiver),state,e);
        if(route==scenery_kiss::Capture::Forward)return;
        if(route!=scenery_kiss::Capture::Single||!placement_ui_diagnostics::release_ready(receiver,e)||!submit_scene_intent(*r,uintptr_t(receiver),nullptr,e)){
            if(e.empty())e="scenery original Release has no complete single intent";
            placement_ui_diagnostics::capture_failed(receiver,e);reject(*r,e);
        }
    }catch(const std::exception& x){const auto e=std::string("scenery Release capture exception: ")+x.what();placement_ui_diagnostics::capture_failed(receiver,e);reject(*r,e);}
    catch(...){const std::string e="scenery Release capture nonstandard exception";placement_ui_diagnostics::capture_failed(receiver,e);reject(*r,e);}
}
void scene_update_entry(void* receiver,float dt) {
    if(published.load(std::memory_order_acquire)->thread==GetCurrentThreadId())placement_ui_diagnostics::build_update_entered(receiver,dt);
    auto* r=published.load(std::memory_order_acquire);
    if(r->thread!=GetCurrentThreadId()){r->scene_update(receiver,dt);return;}
    if(r->replaying){auto* prior=r->scope;r->scope=nullptr;struct Restore {Runtime& r;Scope* prior;~Restore(){r.scope=prior;}} restore{*r,prior};scene_original_ui(*r,receiver,dt,false);return;}
    try{
        host::ProcessMemory memory;std::string e;bool own=false;
        if(!scenery_kiss::live_cursor(memory,r->image,uintptr_t(receiver),own,e)){reject(*r,e);return;}
        if(!own){scene_original_ui(*r,receiver,dt,false);return;}
        if(!r->ready||!scene_authority(*r,e)){if(e.empty())e="scenery capture not ready";reject(*r,e);return;}
        bool bulk=false;
        if(!ui_move_runtime::scenery_stroke::bulk(uintptr_t(receiver),false,bulk,e)){reject(*r,e);return;}
        if(bulk){scene_original_ui(*r,receiver,dt,true);return;}
        bool mode_transition=false;
        if(!scenery_kiss::idle_single_update(memory,r->image,uintptr_t(receiver),mode_transition,e)){reject(*r,e);return;}
        if(mode_transition){
            host::opened::CurrentWorld transition_world;void* transition_shard=nullptr;
            if(!read_world(transition_world,transition_shard,e)){reject(*r,e);return;}
            scene_original_ui(*r,receiver,dt,true);
            if(!scenery_kiss::single_press(memory,r->image,uintptr_t(receiver),e))reject(*r,e);
            return;
        }
        if(!scenery_kiss::single_press(memory,r->image,uintptr_t(receiver),e)){reject(*r,e);return;}
        host::opened::CurrentWorld world;void* shard=nullptr;if(!read_world(world,shard,e)){reject(*r,e);return;}
        scenery_kiss::NativeCatalog catalog(memory,r->image,r->thread);action::BuildingState state;
        const auto route=scenery_kiss::read_single_intent(memory,catalog,r->image,uintptr_t(shard),uintptr_t(receiver),state,e);
        if(route==scenery_kiss::Capture::Forward){++r->forwarded;scene_original_ui(*r,receiver,dt,true);return;}
        if(route!=scenery_kiss::Capture::Single){reject(*r,e);return;}
        if(!submit_scene_intent(*r,uintptr_t(receiver),nullptr,e))reject(*r,e);
    }catch(const std::exception& x){reject(*r,std::string("scenery update guard exception: ")+x.what());}
    catch(...){reject(*r,"scenery update guard nonstandard exception");}
}
bool queue_building_purchase(Runtime& r,const void* type,const void* position,std::string& error) {
    if(r.deferred_build){error="building UI captured request has unconfirmed cleanup; no replay";return false;}
    if(!net::session_wire::connected()){error="building cooperative session disconnected";return false;}
    const auto controller=typed_action_runtime::status();
    if(!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)){
        error="building Controller unavailable, locked or pending";return false;
    }
    host::ProcessMemory memory;uintptr_t cursor=0,ap=0,active=0;
    if(!memory.copy(r.image+0x13df9d0,&cursor,8,error)||!cursor||!memory.copy(cursor,&ap,8,error)||
       ap!=r.image+0x1786dd0||!memory.copy(cursor+0x190,&active,8,error)||active!=r.ui_build_receiver){
        if(error.empty())error="building UI Purchase actual cursor owner differs";return false;
    }
    DeferredBuilding fresh;fresh.receiver=r.ui_build_receiver;
    if(!read_world(fresh.world,fresh.shard,error))return false;
    action::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!typed_action_runtime::network_binding(snapshot,binding,error))return false;
    if(fresh.world.uuid.empty()||fresh.world.world_epoch!=binding.native_epoch||fresh.world.generation!=binding.native_generation){
        error="building UI Purchase current native binding differs: session_snapshot="+snapshot+" native_snapshot="+fresh.world.uuid+
            " native_epoch="+std::to_string(fresh.world.world_epoch)+" binding_epoch="+std::to_string(binding.native_epoch)+
            " native_generation="+std::to_string(fresh.world.generation)+" binding_generation="+std::to_string(binding.native_generation);return false;}
    if(r.build.capture_at_update_entry(reinterpret_cast<void*>(fresh.receiver),fresh.request,error,true)!=placement_kiss::Capture::Captured){
        if(error.empty())error="building UI Purchase lacks final native pending intent";return false;
    }
    if(!captured_build_scene(r,fresh.request,fresh.scene,error))return false;
    std::array<float,3> copied{};
    if(!position||!memory.copy(uintptr_t(position),copied.data(),sizeof copied,error)){
        if(error.empty())error="building UI Purchase copied position unavailable";return false;
    }
    if(copied!=fresh.request.pose.position){error="building UI Purchase copied final pose differs";return false;}
    uintptr_t data=0;uint64_t bytes=0;
    if(!type||!memory.copy(uintptr_t(type),&data,8,error)||!memory.copy(uintptr_t(type)+8,&bytes,8,error)){
        if(error.empty())error="building UI Purchase type span unavailable";return false;
    }
    if(!data||bytes!=fresh.request.type.size()||bytes>256){error="building UI Purchase type span differs";return false;}
    char name[256]{};if(!memory.copy(data,name,size_t(bytes),error))return false;
    if(std::string(name,size_t(bytes))!=fresh.request.type){error="building UI Purchase actual type differs";return false;}
    if(!memory.copy(fresh.receiver+0x6b8,&fresh.ghost,8,error)||!memory.copy(fresh.receiver+0x6c0,&fresh.blueprint,8,error))return false;
    r.deferred_build=std::move(fresh);++r.purchase_intents;return true;
}
bool building_purchase_gate(Runtime& r,uintptr_t caller,const void* type,const void* position) {
    if(r.thread!=GetCurrentThreadId()||r.scope||r.replaying||!r.ui_build_receiver||caller!=r.image+0x5940d3)return false;
    try{std::string error;if(!queue_building_purchase(r,type,position,error)){
        placement_ui_diagnostics::capture_failed(reinterpret_cast<void*>(r.ui_build_receiver),error);reject(r,error);
    }}catch(const std::exception& cause){const auto error=std::string("building UI Purchase capture exception: ")+cause.what();placement_ui_diagnostics::capture_failed(reinterpret_cast<void*>(r.ui_build_receiver),error);reject(r,error);}
    catch(...){const std::string error="building UI Purchase capture nonstandard exception";placement_ui_diagnostics::capture_failed(reinterpret_cast<void*>(r.ui_build_receiver),error);reject(r,error);}
    return true;
}
bool submit_deferred_building(Runtime& r,std::string& error) {
    auto& pending=*r.deferred_build;
    if(!pending.error.empty()){error=pending.error;return false;}
    host::ProcessMemory memory;uintptr_t ap=0,ghost=0,blueprint=0;std::array<uint8_t,3> flags{};
    if(!memory.copy(pending.receiver,&ap,8,error)||!memory.copy(pending.receiver+0x660,flags.data(),flags.size(),error)||
       !memory.copy(pending.receiver+0x6b8,&ghost,8,error)||!memory.copy(pending.receiver+0x6c0,&blueprint,8,error))return false;
    if(ap!=r.image+0x17327d0||flags!=std::array<uint8_t,3>{0,0,0}||ghost!=pending.ghost||blueprint!=pending.blueprint){
        error="building original false-branch cleanup or children differs; no replay";return false;
    }
    host::opened::CurrentWorld world;void* shard=nullptr;
    if(!read_world(world,shard,error))return false;
    if(!host::same_world(world,pending.world)||shard!=pending.shard){error="building world changed before original cleanup ACK; no replay";return false;}
    action::BuildingState state;state.type=pending.request.type;state.variant=pending.request.variant;
    state.x=pending.request.pose.position[0];state.y=pending.request.pose.position[1];state.z=pending.request.pose.position[2];
    state.rotation=pending.request.pose.quaternion;state.scale=pending.request.pose.scale;state.alive=true;
    pending.action_uuid=action_uuid(error);if(pending.action_uuid.empty())return false;
    std::string scene;
    if(!captured_build_scene(r,pending.request,scene,error)||scene!=pending.scene){
        if(error.empty())error="Building captured template scene changed before submission";return false;
    }
    if(!placement_ui_diagnostics::capture_bound(reinterpret_cast<void*>(pending.receiver),pending.action_uuid,state,error)||
       !submit_captured_build(r,pending.request,pending.action_uuid,state,error))return false;
    if(pending.request.type=="DungeonExterior")r.local_builds.emplace(pending.action_uuid,Runtime::LocalBuild{pending.receiver,{}});
    ++r.captured;r.last_action=pending.action_uuid;r.cause.clear();r.deferred_build.reset();return true;
}
void building_original_ui(Runtime& r,void* receiver,float dt) {
    const auto prior=r.ui_build_receiver;r.ui_build_receiver=uintptr_t(receiver);
    struct Restore {Runtime& r;uintptr_t prior;~Restore(){r.ui_build_receiver=prior;}} restore{r,prior};
    try{r.original(receiver,dt);}catch(const std::exception& cause){
        const auto error=std::string("building original UI Update exception: ")+cause.what();
        if(r.deferred_build)r.deferred_build->error=error;
        placement_ui_diagnostics::capture_failed(receiver,error);reject(r,error);throw;
    }catch(...){const std::string error="building original UI Update nonstandard exception";
        if(r.deferred_build)r.deferred_build->error=error;
        placement_ui_diagnostics::capture_failed(receiver,error);reject(r,error);throw;
    }
    if(!r.deferred_build||r.deferred_build->receiver!=uintptr_t(receiver))return;
    try{std::string error;if(!submit_deferred_building(r,error)){
        r.deferred_build->error=error;placement_ui_diagnostics::capture_failed(receiver,error);reject(r,error);
    }}catch(const std::exception& cause){const auto error=std::string("building deferred capture exception: ")+cause.what();
        if(r.deferred_build)r.deferred_build->error=error;
        placement_ui_diagnostics::capture_failed(receiver,error);reject(r,error);
    }catch(...){const std::string error="building deferred capture nonstandard exception";
        if(r.deferred_build)r.deferred_build->error=error;
        placement_ui_diagnostics::capture_failed(receiver,error);reject(r,error);
    }
}
void update_entry(void* receiver,float dt) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(runtime->thread==GetCurrentThreadId())placement_ui_diagnostics::build_update_entered(receiver,dt);
    if(runtime->thread==GetCurrentThreadId()&&runtime->replaying&&runtime->scope&&runtime->scope->scenery){auto* prior=runtime->scope;runtime->scope=nullptr;struct Restore {Runtime& r;Scope* prior;~Restore(){r.scope=prior;}} restore{*runtime,prior};runtime->original(receiver,dt);return;}
    if(!runtime->ready||runtime->replaying||runtime->thread!=GetCurrentThreadId()) {
        runtime->original(receiver,dt);return;
    }
    if(runtime->deferred_build){reject(*runtime,runtime->deferred_build->error.empty()?"building captured request awaiting original cleanup; no replay":runtime->deferred_build->error);return;}
    placement_kiss::Request captured;std::string error;
    const auto result=runtime->build.capture_at_update_entry(receiver,captured,error,true);
    if(result==placement_kiss::Capture::PassOriginal){++runtime->forwarded;building_original_ui(*runtime,receiver,dt);return;}
    if(result!=placement_kiss::Capture::Captured){placement_ui_diagnostics::capture_failed(receiver,error);reject(*runtime,error);return;}
    std::string captured_scene;
    if(!captured_build_scene(*runtime,captured,captured_scene,error)){placement_ui_diagnostics::capture_failed(receiver,error);reject(*runtime,error);return;}
    uint32_t cleanup_exception=0;
    if(!runtime->build.consume_captured_intent(receiver,captured,cleanup_exception,error)){placement_ui_diagnostics::capture_failed(receiver,error);reject(*runtime,error);return;}
    const auto id=action_uuid(error);
    if(id.empty()){placement_ui_diagnostics::capture_failed(receiver,error);reject(*runtime,error);return;}
    action::BuildingState state;
    state.type=captured.type;state.variant=captured.variant;
    state.x=captured.pose.position[0];state.y=captured.pose.position[1];state.z=captured.pose.position[2];
    state.rotation=captured.pose.quaternion;state.scale=captured.pose.scale;state.alive=true;
    if(!placement_ui_diagnostics::capture_bound(receiver,id,state,error)){reject(*runtime,error);return;}
    std::string final_scene;
    if(!captured_build_scene(*runtime,captured,final_scene,error)||final_scene!=captured_scene){
        if(error.empty())error="Building captured template scene changed during original cleanup";
        placement_ui_diagnostics::capture_failed(receiver,error);reject(*runtime,error);return;
    }
    if(!submit_captured_build(*runtime,captured,id,state,error)){placement_ui_diagnostics::capture_failed(receiver,error);reject(*runtime,error);return;}
    if(captured.type=="DungeonExterior")runtime->local_builds.emplace(id,Runtime::LocalBuild{uintptr_t(receiver),{}});
    ++runtime->captured;runtime->last_action=id;runtime->cause.clear();
    LOG_INFO("[UIBuild] Normal building intent captured action={} type={} variant={}",id,state.type,state.variant);
}
}
bool enable(std::string& error) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime){runtime=new Runtime;}
    if(runtime->thread!=GetCurrentThreadId()||runtime->attempted){error="building capture requires one installation on the game thread";return false;}
    runtime->attempted=true;
    auto fail=[&](){published.store(runtime,std::memory_order_release);runtime->cause=error;LOG_ERROR("[UIBuild] Installation failed cause={}",error);return false;};
    if(!net::session_wire::connected()){error="building capture requires connected session";return fail();}
    if(!typed_action_runtime::status().value("started",false)){error="building capture requires started action controller";return fail();}
    std::string measured;uint32_t thread=0;
    if(!native::read_resolver_receipt(runtime->image,thread,measured,error)||
       !runtime->build.bind_before_hooks(runtime->image,thread,error)||
       !runtime->scene.bind_before_hooks(runtime->image,thread,error)||
       !scenery_kiss::validate_ui_originals(runtime->image,error))return fail();
    host::opened::CurrentWorld install_world;void* install_shard=nullptr;
    if(!read_world(install_world,install_shard,error)||!cursor_guards(*runtime,false,error))return fail();
    const auto install=[&](uintptr_t rva,void* detour,void** original) {
        if(!hooks::create_hook(reinterpret_cast<void*>(runtime->image+rva),detour,original)) {
            error="building capture CreateHook failed RVA="+std::to_string(rva);return false;
        }
        runtime->created=true;return true;
    };
    if(!install(0x593cc0,reinterpret_cast<void*>(&update_entry),reinterpret_cast<void**>(&runtime->original))||
       !install(0x40bb10,reinterpret_cast<void*>(&purchase_entry),reinterpret_cast<void**>(&runtime->purchase))||
#ifndef MT2MP_FRAMEWORK_PLUGIN
       !install(0x24ed30,reinterpret_cast<void*>(&add_entry),reinterpret_cast<void**>(&runtime->add))||
#endif
       !install(0x53b260,reinterpret_cast<void*>(&undo_entry),reinterpret_cast<void**>(&runtime->undo))||
       !install(0x5854e0,reinterpret_cast<void*>(&cursor_behaviour_entry),reinterpret_cast<void**>(&runtime->cursor_behaviour))||
       !install(0x585270,reinterpret_cast<void*>(&cursor_selection_entry),reinterpret_cast<void**>(&runtime->cursor_selection))||
       !runtime->build.publish_typed_original(runtime->original,error)||
       !install(0x5d4620,reinterpret_cast<void*>(&scene_update_entry),reinterpret_cast<void**>(&runtime->scene_update))||
       !install(0x448330,reinterpret_cast<void*>(&scene_purchase_entry),reinterpret_cast<void**>(&runtime->scene_purchase))||
       !install(0x2cb340,reinterpret_cast<void*>(&scene_add_entry),reinterpret_cast<void**>(&runtime->scene_add))||
       !install(0x5cf440,reinterpret_cast<void*>(&scene_press_entry),reinterpret_cast<void**>(&runtime->scene_press))||
       !install(0x5d64e0,reinterpret_cast<void*>(&erase_press_entry),reinterpret_cast<void**>(&runtime->erase_press))||
       !install(0x5d0600,reinterpret_cast<void*>(&scene_release_entry),reinterpret_cast<void**>(&runtime->scene_release))||
       !runtime->scene.publish_typed_original(runtime->scene_update,error))return fail();
    runtime->scene_originals={uintptr_t(runtime->scene_update),uintptr_t(runtime->scene_purchase),uintptr_t(runtime->scene_add),uintptr_t(runtime->scene_press),uintptr_t(runtime->erase_press),uintptr_t(runtime->scene_release)};
    if(!scenery_kiss::validate_ui_trampolines(runtime->image,runtime->scene_originals,error)||!cursor_guards(*runtime,false,error))return fail();
    published.store(runtime,std::memory_order_release);
#ifdef MT2MP_FRAMEWORK_PLUGIN
    if(!framework_subscribe_create(*runtime,error))return fail();
#endif
    std::vector<uintptr_t> prepared_targets;
    for(const auto rva:{0x593cc0,0x40bb10,0x24ed30,0x53b260,0x5854e0,0x585270,0x5d4620,0x448330,0x2cb340,0x5cf440,0x5d64e0,0x5d0600}) {
#ifdef MT2MP_FRAMEWORK_PLUGIN
        if(rva==0x24ed30)continue;
#endif
        prepared_targets.push_back(runtime->image+rva);
    }
    if(!hooks::enable_prepared_targets(prepared_targets,"building capture",error))return fail();
    const std::array<uintptr_t,6> scene_entries{uintptr_t(&scene_update_entry),uintptr_t(&scene_purchase_entry),uintptr_t(&scene_add_entry),uintptr_t(&scene_press_entry),uintptr_t(&erase_press_entry),uintptr_t(&scene_release_entry)};
    if(!scenery_kiss::validate_ui_routes(runtime->image,runtime->scene_originals,scene_entries,error))return fail();
    if(!building_history_slice::enable(runtime->image,thread,error))return fail();
    if(!cursor_guards(*runtime,true,error))return fail();
    runtime->ready=true;runtime->cause.clear();error.clear();
    LOG_INFO("[UIBuild] Original building Update capture enabled");return true;
}
#ifdef MT2MP_FRAMEWORK_PLUGIN
bool framework_retire(std::string& error)try {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->framework_create_token){error.clear();return true;}
    if(runtime->thread!=GetCurrentThreadId()||runtime->scope||runtime->replaying||runtime->framework_create_depth||runtime->framework_create_overflow){error="Framework CreateBuilding retirement waits for idle bound-thread scopes";return false;}
    const auto& routes=framework_runtime::routes();
    const auto result=routes.create_unsubscribe(routes.context,runtime->framework_create_token);
    if(result!=MT2FW_MT2_OK){error="Framework CreateBuilding retirement failed result="+std::to_string(result);return false;}
    runtime->framework_create_token=0;runtime->ready=false;error.clear();return true;
}catch(const std::exception& problem){error=std::string("Framework CreateBuilding retirement exception: ")+problem.what();return false;}
catch(...){error="Framework CreateBuilding retirement nonstandard exception";return false;}
#endif
bool enabled()noexcept {const auto* runtime=published.load(std::memory_order_acquire);return runtime&&runtime->ready&&runtime->thread==GetCurrentThreadId();}
bool place_scenery(Runtime&,const action::Request&,action::BuildingState&,json&,std::string&);
bool place_bound(const action::Request& request,action::BuildingState& state,json& receipt,std::string& error) {
    auto* runtime=published.load(std::memory_order_acquire);
    receipt=json::object();state={};
    if(!enabled()||runtime->replaying||runtime->receipts.contains(request.action_uuid)
#ifdef MT2MP_FRAMEWORK_PLUGIN
       ||runtime->framework_create_failed
#endif
    ) {
        error="native building action unavailable, reentrant or already attempted";return false;
    }
    bool scenery=false;
    host::ProcessMemory family_memory;scenery_kiss::NativeCatalog catalog(family_memory,runtime->image,runtime->thread);
    if(!catalog.classify_directory(request.building.type,scenery,error))return false;
    if(scenery)return place_scenery(*runtime,request,state,receipt,error);
    host::opened::CurrentWorld world;void* shard=nullptr;
    if(!read_build_target(request,world,shard,error))return false;
    placement_kiss::Request native_request;
    native_request.dungeon_family=request.dungeon_building;
    native_request.action_uuid=request.action_uuid;native_request.type=request.building.type;native_request.variant=request.building.variant;
    native_request.pose={{request.building.x,request.building.y,request.building.z},request.building.rotation,request.building.scale};
    native_request.world=uintptr_t(world.world);native_request.epoch=world.world_epoch;native_request.generation=world.generation;native_request.sequence=request.sequence;
    if(!placement_kiss::valid_request(native_request,error))return false;
    native::SimulationState before,after;
    if(!native::read_simulation_state(before,error))return false;
    host::ProcessMemory undo_memory;
    build_actions::private_native::UndoWitness undo_before,undo_after;
    build_actions::private_native::GraphWitness graph;
    if(!build_actions::private_native::read_undo(undo_memory,runtime->image,undo_before,error))return false;
    if(undo_before.active_graph){error="native building action requires inactive Undo graph";return false;}
    native::PropState grounded;
    if(!request.dungeon_building&&!native::probe_building_placement(request.building.type,request.building.variant,request.building.x,request.building.z,grounded,error))return false;
    runtime->receipts.emplace(request.action_uuid,json{{"attempted",true},{"confirmed",false}});
    if(!cursor_guards(*runtime,true,error))return false;
    Scope scope;scope.cursor.exterior=request.dungeon_building&&request.building.type=="DungeonExterior";
    if(scope.cursor.exterior){
        uintptr_t ap=0;
        if(!family_memory.copy(runtime->image+0x13df9d0,&scope.cursor.cursor,8,error)||!scope.cursor.cursor||
           !family_memory.copy(scope.cursor.cursor,&ap,8,error)||ap!=runtime->image+0x1786dd0){
            if(error.empty())error="Private Dungeon Build current global Cursor owner differs";return false;
        }
    }
    runtime->scope=&scope;runtime->replaying=true;
    struct Reset {Runtime& r;~Reset(){r.scope=nullptr;r.replaying=false;}} reset{*runtime};
    auto& private_build=runtime->held_builds.emplace(request.action_uuid,placement_kiss::PrivateBuild{}).first->second;
    LOG_INFO("[UIBuild] Private prepare attempted action={}",request.action_uuid);
    bool success=runtime->build.prepare(native_request,shard,private_build,error);
    LOG_INFO("[UIBuild] Private prepare returned action={} success={} stage={} cause={}",request.action_uuid,success,private_build.stage,error);
    uint32_t exception=private_build.exception;
    if(success){
        LOG_INFO("[UIBuild] Original Update attempted action={}",request.action_uuid);
        success=runtime->build.update_once(private_build,exception);
        LOG_INFO("[UIBuild] Original Update returned action={} success={} stage={} exception={}",request.action_uuid,success,private_build.stage,exception);
    }
    json maker=json::object();
    if(success&&!exception&&scope.cursor.exterior)success=finish_dungeon_maker(*runtime,maker,error);
    native::BuildingState actual;
    if(success&&!exception&&scope.building) {
        uint64_t uid=0;SIZE_T count=0;
        if(!ReadProcessMemory(GetCurrentProcess(),static_cast<unsigned char*>(scope.building)+0x18,&uid,sizeof uid,&count)||count!=sizeof uid) {
            success=false;error="native created building UID read failed Win32="+std::to_string(GetLastError());
        }else success=native::read_building(uid,actual,error)&&native::read_simulation_state(after,error);
    }else {success=false;if(error.empty())error="native building action incomplete SEH="+std::to_string(exception);}
    placement_kiss::Receipt effects;
    effects.world=uintptr_t(world.world);effects.epoch=world.world_epoch;effects.generation=world.generation;effects.sequence=request.sequence;
    effects.action_uuid=request.action_uuid;effects.type=actual.type;effects.variant=actual.variant;effects.native_uid=actual.prop.uid;
    effects.pose={{actual.prop.x,actual.prop.y,actual.prop.z},actual.rotation,actual.scale};
    effects.purchase_calls=scope.purchase_returned;effects.add_calls=scope.add_returned;effects.undo_calls=scope.undo_returned;
    effects.cash_before=before.cash;effects.cash_after=after.cash;effects.native_cost=scope.cost;effects.exception=exception;
    if(success&&!scope.cause.empty()){success=false;error=scope.cause;}
    if(success&&!dungeon_build_cursor::complete(scope.cursor,error))success=false;
    host::opened::CurrentWorld after_world;
    if(success&&(!native_open_runtime::read_current_world(after_world,error)||!host::same_world(world,after_world))) {
        success=false;if(error.empty())error="native building world changed during execution";
    }
    if(success)success=placement_kiss::valid_receipt(native_request,effects,error);
    if(success)success=build_actions::private_native::read_undo(undo_memory,runtime->image,undo_after,error)&&
        build_actions::private_native::read_graph(undo_memory,runtime->image,scope.graph,graph,error)&&
        build_actions::private_native::validate_undo_commit(undo_before,undo_after,graph,error);
    void* cleanup_shard=nullptr;
    if(success&&(!read_build_target(request,after_world,cleanup_shard,error)||!host::same_world(world,after_world)||cleanup_shard!=shard)) {
        success=false;if(error.empty())error="native building cleanup world or shard changed";
    }
    if(success){
        LOG_INFO("[UIBuild] Private D0 attempted action={}",request.action_uuid);
        success=runtime->build.destroy_once(private_build,exception);
        LOG_INFO("[UIBuild] Private D0 returned action={} success={} stage={} exception={}",request.action_uuid,success,private_build.stage,exception);
    }
    if(!success&&error.empty())error=(private_build.preview_effect.cause.empty()?std::string("native private building cleanup failed"):private_build.preview_effect.cause)+" SEH="+std::to_string(exception);
    receipt={{"attempted",true},{"confirmed",success},{"native_uid",actual.prop.uid},{"purchase",scope.purchase_returned},
        {"add",scope.add_returned},{"undo",scope.undo_returned},{"cost",scope.cost},{"cash_before",before.cash},{"cash_after",after.cash},
        {"exception",exception},{"private_stage",private_build.stage},{"error",error},
        {"requested_position",{request.building.x,request.building.y,request.building.z}},
        {"actual_position",{actual.prop.x,actual.prop.y,actual.prop.z}},
        {"actual_rotation",actual.rotation},{"actual_scale",actual.scale}};
    const auto& preview=private_build.preview_effect;
    receipt["preview_effect"]={{"checked",preview.checked},{"present",preview.present},{"destroy_attempted",preview.attempted},{"destroy_returned",preview.returned},{"removed",preview.removed},{"slot_cleared",preview.slot_cleared},{"manager_count_before",preview.count_before},{"manager_count_after",preview.count_after},{"error",preview.cause}};
    receipt["undo_confirmed"]=success&&graph.locked&&graph.pending_delete==0;
    if(success)receipt["local_undo_graph"]=uint64_t(scope.graph);
    receipt["dungeon_maker"]=maker;
    receipt["cursor_behaviour_preserved"]=scope.cursor.behaviour;receipt["cursor_selection_preserved"]=scope.cursor.selection;
    receipt["purchase_attempted"]=scope.purchase_attempted;receipt["add_attempted"]=scope.add_attempted;
    receipt["undo_attempted"]=scope.undo_attempted;
    runtime->receipts.at(request.action_uuid)=receipt;
    if(!success){LOG_ERROR("[UIBuild] Native building action failed action={} cause={}",request.action_uuid,error);return false;}
    state={actual.type,actual.variant,actual.prop.x,actual.prop.y,actual.prop.z,actual.rotation,actual.scale,true};
    LOG_INFO("[UIBuild] Original building action confirmed action={} local_uid={}",request.action_uuid,actual.prop.uid);return true;
}
uint32_t cursor_complete_pod(Runtime::CursorSet original,uintptr_t cursor,uintptr_t value){
    __try{original(reinterpret_cast<void*>(cursor),reinterpret_cast<void*>(value));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool complete_dungeon_local_ui(const action::Request& request,uintptr_t building,uint64_t uid,std::string& error){
    if(!request.dungeon_building||request.action!=action::Action::PlaceBuilding||request.building.type!="DungeonExterior")return true;
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId()||runtime->scope||runtime->replaying){
        error="Dungeon Building local UI completion requires its idle current native owner";return false;
    }
    const auto local=runtime->local_builds.find(request.action_uuid);
    if(local==runtime->local_builds.end())return true;
    const auto native=runtime->receipts.find(request.action_uuid);
    if(native==runtime->receipts.end()||!native->second.value("confirmed",false)||
       native->second.value("native_uid",uint64_t{0})!=uid||!building||!uid||!cursor_guards(*runtime,true,error)){
        if(error.empty())error="Dungeon Building local UI has no exact confirmed original Building receipt";
        return false;
    }
    host::ProcessMemory memory;
    auto read=[&](uintptr_t pointer,size_t offset,uintptr_t& value,std::string& cause){
        return memory.copy(pointer+offset,&value,sizeof value,cause);
    };
    auto call=[&](bool selected,uintptr_t cursor,uintptr_t value,std::string& cause){
        const auto seh=cursor_complete_pod(selected?runtime->cursor_selection:runtime->cursor_behaviour,cursor,value);
        if(seh){cause="Dungeon Building local UI original Cursor completion SEH="+std::to_string(seh);return false;}
        return true;
    };
    auto selected=[&](uintptr_t owner,uintptr_t target,std::string& cause){
        build_actions::private_native::SelectionWitness witness;
        build_actions::private_native::SelectionCounts counts;
        if(!build_actions::private_native::read_selection(memory,runtime->image,owner,witness,counts,cause))return false;
        if(witness.lists[1]!=std::vector<uintptr_t>{target}){
            cause="Dungeon Building original selection did not contain its exact new Exterior";return false;
        }
        return true;
    };
    auto& receipt=local->second.completion;
    const bool result=dungeon_build_local_ui::complete(runtime->image,local->second.receiver,building,receipt,read,call,selected,error);
    native->second["local_ui"]={{"attempted",receipt.attempted},{"switched",receipt.switched},
        {"selected",receipt.selected},{"superseded",receipt.superseded},{"error",error}};
    if(result&&receipt.superseded)LOG_INFO("[UIBuild] Dungeon local selection skipped after a newer local tool action action={}",request.action_uuid);
    return result;
}
bool validate_scenery_ui_routes(uintptr_t image,uint32_t thread,std::string& error) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||runtime->image!=image||runtime->thread!=thread||thread!=GetCurrentThreadId()){error="scenery UI diagnostic actual hook owner/mainthread differs";return false;}
    const std::array<uintptr_t,6> entries{uintptr_t(&scene_update_entry),uintptr_t(&scene_purchase_entry),uintptr_t(&scene_add_entry),uintptr_t(&scene_press_entry),uintptr_t(&erase_press_entry),uintptr_t(&scene_release_entry)};
    return scenery_kiss::validate_ui_routes(image,runtime->scene_originals,entries,error);
}
bool place(const action::Request& request,action::BuildingState& state,json& receipt,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!enabled()){error="Dungeon Building execution has no bound native owner";return false;}
    receipt=json::object();state={};
    DungeonBuildView view(*runtime);
    const bool prepared=view.begin(request,error);
    bool success=prepared&&place_bound(request,state,receipt,error);
    std::string cleanup;
    if(!view.restore(cleanup)){
        if(!error.empty())error+="; ";
        error+=cleanup;success=false;receipt["confirmed"]=false;receipt["unknown"]=true;receipt["context_restore_error"]=cleanup;
    }
    if(request.dungeon_building)receipt["local_view_restored"]=view.finished&&(!view.active||view.local.restored);
    return success;
}
bool place_scenery(Runtime& r,const action::Request& request,action::BuildingState& state,json& receipt,std::string& error) {
    host::opened::CurrentWorld world,after_world;void* shard=nullptr;
    if(!read_world(world,shard,error)||!scenery_kiss::valid_state(request.building,error))return false;
    host::ProcessMemory memory;scenery_kiss::NativeCatalog catalog(memory,r.image,r.thread);int32_t category=0,variant=0,cost=-1;uintptr_t manager=0;
    if(!catalog.resolve(request.building,category,variant,error)||!scenery_kiss::create_permission(memory,r.image,uintptr_t(shard),request.building,category,manager,cost,error))return false;
    native::SimulationState before,after;build_actions::private_native::UndoWitness undo_before,undo_after;build_actions::private_native::GraphWitness graph;
    if(!native::read_simulation_state(before,error)||!build_actions::private_native::read_undo(memory,r.image,undo_before,error))return false;
    if(undo_before.active_graph){error="scenery native Undo graph already active";return false;}
    const std::array<uintptr_t,6> entries{uintptr_t(&scene_update_entry),uintptr_t(&scene_purchase_entry),uintptr_t(&scene_add_entry),uintptr_t(&scene_press_entry),uintptr_t(&erase_press_entry),uintptr_t(&scene_release_entry)};
    if(!scenery_kiss::validate_ui_routes(r.image,r.scene_originals,entries,error))return false;
    r.receipts.emplace(request.action_uuid,json{{"attempted",true},{"confirmed",false}});
    Scope scope;scope.scenery=true;scope.category=category;scope.variant=variant;scope.manager=reinterpret_cast<void*>(manager);scope.cost=cost;scope.position={request.building.x,request.building.y,request.building.z};
    r.scope=&scope;r.replaying=true;struct Reset {Runtime& r;~Reset(){r.scope=nullptr;r.replaying=false;}} reset{r};
    auto& owned=r.held_scenery.emplace(request.action_uuid,scenery_kiss::PrivateScenery{}).first->second;
    bool success=r.scene.prepare(request.building,category,variant,shard,owned,error);
    if(success)success=r.scene.update_once(owned,error);
    native::BuildingState actual;
    if(success&&scope.building){uint64_t uid=0;if(!memory.copy(uintptr_t(scope.building)+0x18,&uid,8,error))success=false;
        else success=scenery_kiss::read_action_prop(uid,actual,error)&&native::read_simulation_state(after,error);}
    else {success=false;if(error.empty())error=scope.cause.empty()?"scenery native original effects incomplete":scope.cause;}
    placement_kiss::Request expected;expected.action_uuid=request.action_uuid;expected.world=uintptr_t(world.world);expected.epoch=world.world_epoch;expected.generation=world.generation;expected.sequence=request.sequence;
    expected.type=request.building.type;expected.variant=request.building.variant;expected.pose={{request.building.x,request.building.y,request.building.z},request.building.rotation,request.building.scale};
    placement_kiss::Receipt effects;effects.world=expected.world;effects.epoch=expected.epoch;effects.generation=expected.generation;effects.sequence=request.sequence;effects.action_uuid=request.action_uuid;
    effects.type=actual.type;effects.variant=actual.variant;effects.native_uid=actual.prop.uid;effects.pose={{actual.prop.x,actual.prop.y,actual.prop.z},actual.rotation,actual.scale};
    effects.purchase_calls=scope.purchase_returned;effects.add_calls=scope.add_returned;effects.undo_calls=scope.undo_returned;effects.cash_before=before.cash;effects.cash_after=after.cash;effects.native_cost=scope.cost;effects.exception=owned.exception;
    if(success&&!scope.cause.empty()){success=false;error=scope.cause;}
    if(success)success=placement_kiss::valid_receipt(expected,effects,error)&&scenery_kiss::valid_effect_counts(scope.purchase_attempted,scope.purchase_returned,scope.add_attempted,scope.add_returned,scope.undo_attempted,scope.undo_returned,error);
    if(success)success=build_actions::private_native::read_undo(memory,r.image,undo_after,error)&&build_actions::private_native::read_graph(memory,r.image,scope.graph,graph,error)&&build_actions::private_native::validate_undo_commit(undo_before,undo_after,graph,error);
    void* after_shard=nullptr;
    if(success&&(!read_world(after_world,after_shard,error)||!host::same_world(world,after_world)||after_shard!=shard)){success=false;if(error.empty())error="scenery native world changed before cleanup";}
    if(success)success=r.scene.destroy_once(owned,error);
    receipt={{"attempted",owned.stage!=0},{"confirmed",success},{"native_uid",actual.prop.uid},{"purchase",scope.purchase_returned},{"add",scope.add_returned},{"undo",scope.undo_returned},{"cost",scope.cost},{"cash_before",before.cash},{"cash_after",after.cash},{"exception",owned.exception},{"cleanup_exception",owned.cleanup_exception},{"private_stage",owned.stage},{"error",error},
        {"purchase_attempted",scope.purchase_attempted},{"add_attempted",scope.add_attempted},{"undo_attempted",scope.undo_attempted},{"undo_confirmed",success&&graph.locked&&graph.pending_delete==0},
        {"update_attempted",owned.update_attempted},{"update_returned",owned.update_returned},{"cursor_dtor_attempted",owned.cursor_dtor_attempted},{"cursor_dtor_returned",owned.cursor_dtor_returned},{"options_dtor_attempted",owned.options_dtor_attempted},{"options_dtor_returned",owned.options_dtor_returned},
        {"requested_position",{request.building.x,request.building.y,request.building.z}},{"actual_position",{actual.prop.x,actual.prop.y,actual.prop.z}},{"actual_rotation",actual.rotation},{"actual_scale",actual.scale}};
    if(success)receipt["local_undo_graph"]=uint64_t(scope.graph);
    r.receipts.at(request.action_uuid)=receipt;
    if(!success){if(error.empty())error="scenery action failed without cause";receipt["error"]=error;r.receipts.at(request.action_uuid)=receipt;LOG_ERROR("[UIBuild] Scenery action failed action={} cause={}",request.action_uuid,error);return false;}
    state={actual.type,actual.variant,actual.prop.x,actual.prop.y,actual.prop.z,actual.rotation,actual.scale,true};return true;
}

json status() {
    const auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime)return {{"initialized",false}};
    return {{"initialized",true},{"enabled",runtime->ready},{"captured",runtime->captured},{"forwarded",runtime->forwarded},
        {"rejected",runtime->rejected},{"purchase_intents",runtime->purchase_intents},{"deferred_build_capture",runtime->deferred_build.has_value()},{"last_action",runtime->last_action},{"cause",runtime->cause},{"receipts",runtime->receipts}};
}
bool can_reclaim_global_minhook(std::string& error) {
    const auto* runtime=published.load(std::memory_order_acquire);
    if(runtime&&runtime->created){error="building capture retains process-lifetime typed originals";return false;}
    return true;
}
}

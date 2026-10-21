#include "dungeon_gizmo_runtime.h"
#include "dungeon_input_runtime.h"
#include "district_stroke_runtime.h"
#include "district_stroke_transition.h"
#include "district_stroke_calls.h"
#include "ui_remove_runtime.h"
#include "ui_move_runtime.h"
#include "prop_text_runtime.h"
#include "district_text_runtime.h"
#include "district_title_runtime.h"
#include "district_edit_runtime.h"
#include "district_property_ui.h"
#include "district_edit_history.h"
#include "district_widget_scope.h"
#include "mt2mp/hooks/original_body.h"
#include "group_runtime.h"
#include "group_object_runtime.h"
#include "boss_placement_runtime.h"
#include "session_provider.h"
#include "boss_placement_reader.h"
#include "boss_placement_transaction.h"
#include "boss_placement_state.h"
#include "boss_placement_calls.h"
#include "boss_placement_code.h"
#include "boss_current.h"
#include "intervention_actor_runtime.h"
#include "character_actor_records.h"
#include "group_object_move_reader.h"
#include "group_object_graph.h"
#include "group_object_calls.h"
#include "group_native.h"
#include "group_current_identity.h"
#include "group_inverse.h"
#include "group_history_reader.h"
#include "building_history_slice/native_bridge.h"
#include "district_title_reader.h"
#include "live_host_state/economic_fence.h"
#include "live_host_state/memory_query_timing.h"
#include "district_reader.h"
#include "district_saved_identity_runtime.h"
#include "boss_phase_runtime.h"
#include "service_membership_runtime.h"
#include "service_membership_reader.h"
#include "selection_move_runtime.h"
#include "building_history_slice/native_bridge.h"
#include "selection_move_reader.h"
#include "selection_move_native.h"
#include "duplicate_runtime.h"
#include "scenery_stroke_runtime.h"
#include "building_clearance_runtime.h"
#include "building_clearance_graph.h"
#include "modular_runtime.h"
#include "modular_calls.h"
#include "modular_code.h"
#include "modular_transaction.h"
#include "modular_transition.h"
#include "modular_layout_graph.h"
#include "release_ui_owner.h"
#include "native_open_runtime.h"
#include "subscriber_lifecycle/append_calls.h"
#include "scenery_alias_order.h"
#include "scenery_stroke_capture.h"
#include "scenery_stroke_calls.h"
#include "scenery_stroke_state.h"
#include "scenery_stroke_bugs.h"
#include "scenery_stroke_graph.h"
#include "scenery_stroke_history.h"
#include "scenery_stroke_native.h"
#include "scenery_stroke_code.h"
#include "scenery_stroke_helpers_code.h"
#include "duplicate_reader.h"
#include "duplicate_history_reader.h"
#include "duplicate_history_code.h"
#include "duplicate_calls.h"
#include "duplicate_codeguards.h"
#include "duplicate_records.h"
#include "duplicate_building_values.h"
#include "duplicate_building_history.h"
#include "duplicate_service_history.h"
#include "duplicate_quest_history.h"
#include "duplicate_history_effects.h"
#include "world_quest_history.h"
#include "npc_saved_identity_runtime.h"
#include "building_npc_lifecycle.h"
#include "duplicate_history_value_calls.h"
#include "subscriber_lifecycle/capture.h"
#include <memory>
#include "boss_phase_reader.h"
#include "boss_phase_property.h"
#include "prop_text_dungeon.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include "mt2mp/core/config.h"
#include "scenery_kiss/native_catalog.h"
#include "scenery_kiss/ui_contract.h"
#include "scenery_kiss/lifecycle_contract.h"
#include "scenery_kiss/lifecycle_native.h"
#include "move_contract.h"
#include "move_selection_refresh.h"
#include "move_widget_refresh.h"
#include "mt2mp/core/hash.h"
#include "move_ui_probe/contract.h"
#include "move_ui_probe/native_guards.h"
#include "object_ui_probe/contract.h"
#include "object_ui_probe/native_guards.h"
#include "typed_action_runtime.h"
#include "npc_probe.h"
#include "monster_action_runtime.h"
#include "remove_diagnostics.h"
#include "multiplayer_world/native_binding_adapter.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "object_identity/remove_runtime/copy/undo_witness.h"
#include "object_identity/remove_runtime/copy/selection_witness.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include <Windows.h>
#include "mt2mp/hooks/minhook_backend.h"
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/hooks/framework_backend.h"
#endif
#include <intrin.h>
#include <bcrypt.h>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>

// This symbol is supplied by the existing typed Controller integration.
namespace mt2mp::game::typed_action_runtime {
bool submit_move(const std::string&,const std::string&,const action_session::BuildingState&,std::string&);
}
namespace mt2mp::game::ui_move_runtime {
namespace {
namespace action=action_session;
namespace host=build_actions::private_native::host;
namespace witness=build_actions::private_native;
using json=nlohmann::json;
struct Memory final:move_kiss::Memory {
    host::ProcessMemory process;
    bool read(uintptr_t p,void* to,size_t bytes,std::string& e)override{return process.copy(p,to,bytes,e);}
};
using Purchase=bool(__fastcall*)(void*);
struct Runtime;
struct Services final:move_kiss::Services {
    Runtime& runtime;explicit Services(Runtime& r):runtime(r){}
    bool main_thread()const noexcept override;
    bool active(move_kiss::Origin&,std::string&)override;
    bool bind_target(const move_kiss::Origin&,uintptr_t ghost,uintptr_t target,uint64_t uid,action::BuildingState&,std::string&,move_kiss::Family&,std::string&)override;
    bool submit_move(const move_kiss::Intent&,std::string&)override;
    void reject(const std::string&)noexcept override;
};
struct Runtime {
    uint32_t thread=GetCurrentThreadId();uintptr_t image=0;
    bool attempted=false,created=false;std::atomic<bool> ready{false};
    Memory memory;Services services{*this};std::optional<move_kiss::Capture> capture;
    move_kiss::Calls calls;std::atomic<Purchase> original{nullptr};
    std::optional<move_kiss::Intent> pending;
    std::string pending_id,last_ui_action,cause;bool last_ui_cleaned=false;
    json last_native=json::object();
    uint64_t captured=0,submitted=0,rejected=0;std::atomic<uint64_t> forwarded{0};
};
std::atomic<Runtime*> published{nullptr};
bool ledger(action::Ledger& out,std::string& e){
    native::SimulationState s;if(!native::read_simulation_state(s,e))return false;
    out={s.cash,s.clock.flat_day,int32_t(s.record_present),s.record_day,s.record_cash,s.record_income,s.record_expense,s.line_counts,s.line_income};return true;
}
bool world(Runtime& r,host::WorldState& out,std::string& e){
    host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(r.memory.process,r.image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",api,e))return false;
    host::Reader reader(r.memory.process,origin,r.image,r.thread,api);return reader.current(out,true,e);
}
bool current_prop(Runtime& r,const remove_diagnostics::LocalTarget& target,host::WorldState& w,native::BuildingState& building,std::string& e){
    host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(r.memory.process,r.image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",api,e))return false;
    host::Reader reader(r.memory.process,origin,r.image,r.thread,api);prop_lease::Prop prop;
    if(!reader.current(w,true,e)||!reader.prop(reinterpret_cast<void*>(target.object),target.uid,prop,e)||!scenery_kiss::read_action_prop(target.uid,building,e))return false;
    if(w.origin.world_epoch!=target.world_epoch||w.origin.generation!=target.world_generation||building.prop.uid!=target.uid||building.type!=target.type||building.variant!=target.variant){e="move native World, UID or type/variant changed";return false;}
    std::string dungeon_scene;bool dungeon=false;
    if(!remove_diagnostics::dungeon_building_scene(target,dungeon_scene,dungeon,e))return false;
    if(!target.normal_building&&!target.normal_scenery&&!dungeon){e=target.kind_cause.empty()?"move target is not a supported current Building":target.kind_cause;return false;}
    if(target.normal_scenery&&!scenery_kiss::move_code(r.memory.process,r.image,e))return false;
    host::WorldState after;
    if(!reader.current(after,true,e)||!host::same_world(w.origin,after.origin)){if(e.empty())e="move complete native world changed during property read";return false;}
    return true;
}
uint32_t can_move(uintptr_t image,uintptr_t target,bool& movable){
    __try{
        auto* object=reinterpret_cast<void*>(target);auto* table=*reinterpret_cast<uintptr_t**>(target);
        const auto expected=(uintptr_t(table)==image+0x1742980||uintptr_t(table)==image+0x16c0770)?uintptr_t(0xd32130):
            scenery_kiss::can_move_rva(image,uintptr_t(table));
        if(!expected||table[0xd0/8]!=image+expected)return 0xE0010001;
        constexpr unsigned char original[]{0xb8,1,0,0,0,0xc3};
        if(std::memcmp(reinterpret_cast<const void*>(image+expected),original,sizeof original))return 0xE0010002;
        movable=reinterpret_cast<bool(__fastcall*)(const void*)>(table[0xd0/8])(object);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::string uuid(std::string& e){
    unsigned char b[16];const auto result=BCryptGenRandom(nullptr,b,sizeof b,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(result){e="move BCryptGenRandom NTSTATUS="+std::to_string(result);return {};}
    b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;char text[37];
    std::snprintf(text,sizeof text,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);return text;
}
bool npc_intent(const move_kiss::Intent& intent){
    return intent.family==move_kiss::Family::Npc;
}
bool Services::main_thread()const noexcept{return runtime.thread==GetCurrentThreadId();}
bool Services::active(move_kiss::Origin& o,std::string& e){
    if(!net::session_wire::connected()){e="move cooperative session disconnected";return false;}
    if(!typed_action_runtime::status().value("started",false)){e="move Controller is not started";return false;}
    host::WorldState w;if(!world(runtime,w,e))return false;
    const auto snapshot=net::current_snapshot_id();if(snapshot.empty()){e="move session snapshot unavailable";return false;}
    o={snapshot,w.origin.world_epoch,w.origin.generation,uintptr_t(w.origin.world)};return true;
}
bool Services::bind_target(const move_kiss::Origin& o,uintptr_t ghost,uintptr_t object,uint64_t uid,action::BuildingState& state,std::string& key,move_kiss::Family& family,std::string& e){
    uintptr_t object_ap=0;
    if(!runtime.memory.read(object,&object_ap,sizeof object_ap,e))return false;
    if(object_ap==runtime.image+0x16c0770){
        family=move_kiss::Family::DirectMonster;
        return npc_probe::monster_action_runtime::move_capture_binding(o.snapshot_uuid,o.world_epoch,
            o.generation,object,uid,state,key,e);
    }
    if(object_ap==runtime.image+0x1742980){
        family=move_kiss::Family::Npc;
        return npc_probe::action_runtime::move_capture_binding(o.snapshot_uuid,o.world_epoch,
            o.generation,object,uid,state,key,e);
    }
    family=move_kiss::Family::Prop;
    host::WorldState w;if(!world(runtime,w,e))return false;
    remove_diagnostics::LocalTarget target;
    if(!remove_diagnostics::resolve_local_target(w.selection,object,target,e)||target.uid!=uid||target.snapshot_id!=o.snapshot_uuid||
       target.world_epoch!=o.world_epoch||target.world_generation!=o.generation){if(e.empty())e="move UI native target differs from snapshot";return false;}
    native::BuildingState actual;if(!current_prop(runtime,target,w,actual,e))return false;
    std::string dungeon_scene;bool dungeon=false;
    if(!remove_diagnostics::dungeon_building_scene(target,dungeon_scene,dungeon,e))return false;
    if(!target.normal_building&&!target.normal_scenery&&!dungeon){e=target.kind_cause.empty()?"move target is not a supported current Building":target.kind_cause;return false;}
    uintptr_t ghost_blueprint=0,ap=0,shard=0;bool movable=false;
    const auto exception=can_move(runtime.image,object,movable);
    if(exception||!movable){e="move native CanMove rejected SEH="+std::to_string(exception);return false;}
    if(!runtime.memory.read(ghost+0x210,&ghost_blueprint,sizeof ghost_blueprint,e))return false;
    if(!ghost_blueprint){e="move native owned Ghost Blueprint missing";return false;}
    if(!runtime.memory.read(ghost_blueprint,&ap,sizeof ap,e)||!runtime.memory.read(ghost_blueprint+8,&shard,sizeof shard,e))return false;
    uintptr_t target_shard=w.root;
    if(dungeon&&!runtime.memory.read(target.object+0x170,&target_shard,sizeof target_shard,e))return false;
    if(ap!=runtime.image+0x16dd870||shard!=target_shard){e="move native owned Ghost Blueprint class or shard differs";return false;}
    state=world_state::building_state(actual);key=target.record_key;return true;
}
bool Services::submit_move(const move_kiss::Intent& intent,std::string& e){
    if(runtime.pending||boss_placement::pending()||group_object::pending()||duplicate_edit::pending()||selection_move::pending()||typed_action_runtime::status().value("pending",true)){e="move Controller or UI intent already pending";return false;}
    auto id=uuid(e);if(id.empty())return false;
    if(npc_intent(intent)){
        if(!npc_probe::action_runtime::move_captured(intent,id,e))return false;
    }else if(intent.family==move_kiss::Family::DirectMonster){
        if(!npc_probe::monster_action_runtime::move_ui_captured(intent,id,e))return false;
    }else if(!move_ui_probe::captured(intent,id,e))return false;
    runtime.pending=intent;runtime.pending_id=id;runtime.last_ui_cleaned=false;++runtime.captured;return true;
}
void Services::reject(const std::string& e)noexcept{
    move_ui_probe::failed(runtime.pending?runtime.pending->target:0,e);
    npc_probe::action_runtime::move_failed(e);
    npc_probe::monster_action_runtime::move_ui_failed(e);
    ++runtime.rejected;if(runtime.cause!=e){runtime.cause=e;LOG_ERROR("[UIMove] Rejected cause={}",e);}
}
bool purchase_entry(void* receiver){
    auto* r=published.load(std::memory_order_acquire);
    const uintptr_t caller=uintptr_t(_ReturnAddress());
    bool duplicate_handled=false;std::string duplicate_error;
    bool gizmo_handled=false;std::string gizmo_error;
    try{if(!npc_probe::monster_action_runtime::dungeon_input::capture_gizmo_move(uintptr_t(receiver),caller,gizmo_handled,gizmo_error)){
        LOG_ERROR("[Move] Gizmo control capture failed: {}",gizmo_error);return false;}
        if(gizmo_handled)return false;
    }catch(const std::exception& exception){LOG_ERROR("[Move] Gizmo control capture exception: {}",exception.what());return false;}
    catch(...){LOG_ERROR("[Move] Gizmo control capture non-standard exception");return false;}
    try{if(!duplicate_edit::capture_purchase(uintptr_t(receiver),caller,duplicate_handled,duplicate_error)){
            r->services.reject(duplicate_error);return false;}
        if(duplicate_handled)return false;
    }catch(const std::exception& x){r->services.reject(std::string("Duplicate capture: ")+x.what());return false;}
    bool group_handled=false;std::string group_error;
    try{if(!group_object::capture_purchase(uintptr_t(receiver),caller,group_handled,group_error)){
            r->services.reject(group_error);return false;}
        if(group_handled)return false;
    }catch(const std::exception& x){r->services.reject(std::string("Group Move capture: ")+x.what());return false;}
    bool selection_handled=false;std::string selection_error;
    try{if(!selection_move::capture_purchase(uintptr_t(receiver),caller,selection_handled,selection_error)){
            r->services.reject(selection_error);return false;}
        if(selection_handled)return false;
    }catch(const std::exception& x){r->services.reject(std::string("Selection Move capture: ")+x.what());return false;}
    if(r->ready&&r->capture->purchase(uintptr_t(receiver),caller-r->image)==move_kiss::Route::Suppress)return false;
    ++r->forwarded;return r->original.load(std::memory_order_acquire)(receiver);
}
bool cleanup(Runtime& r,const move_kiss::Intent& i,std::string& e){
    contract::CleanupView c;uintptr_t cursor=0;
    if(!r.memory.read(i.behaviour,&c.addresspoint,sizeof c.addresspoint,e)||!r.memory.read(i.behaviour+0x460,&c.records,sizeof c.records,e)||
       !r.memory.read(i.behaviour+0x468,&c.count,sizeof c.count,e)||!r.memory.read(i.behaviour+0x46c,&c.capacity,sizeof c.capacity,e)||
       !r.memory.read(i.record,&c.owned_first,sizeof c.owned_first,e)||!r.memory.read(i.record+8,&c.owned_second,sizeof c.owned_second,e)||
       !r.memory.read(i.behaviour+0x490,&c.options,sizeof c.options,e)||!r.memory.read(r.image+0x13df9d0,&cursor,sizeof cursor,e)||!cursor||
       !r.memory.read(cursor+0x190,&c.current_behaviour,sizeof c.current_behaviour,e))return false;
    return contract::cleaned(r.image,i,c,e);
}
bool trampoline_route(Runtime& r,std::string& e){
    unsigned char trampoline[20]{};
    if(!r.memory.read(uintptr_t(r.original.load(std::memory_order_acquire)),trampoline,sizeof trampoline,e))return false;
    const unsigned char first[]{0x41,0x57,0x41,0x56,0x41,0x55};
    uintptr_t back=0;std::memcpy(&back,trampoline+12,8);
    if(std::memcmp(trampoline,first,sizeof first)||std::memcmp(trampoline+6,"\xff\x25\0\0\0\0",6)||back!=r.image+0x5c25b6){e="move original trampoline bytes or return differ";return false;}
    return true;
}
bool hook_route(Runtime& r,std::string& e){
    unsigned char target[16]{},trampoline[20]{},relay[14]{};
    const auto address=r.image+0x5c25b0,original=uintptr_t(r.original.load(std::memory_order_acquire));
    if(!r.memory.read(address,target,sizeof target,e)||!r.memory.read(original,trampoline,sizeof trampoline,e))return false;
    const unsigned char first[]{0x41,0x57,0x41,0x56,0x41,0x55};
    const unsigned char tail[]{0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xd8};
    if(target[0]!=0xe9||std::memcmp(target+5,tail,sizeof tail)||std::memcmp(trampoline,first,sizeof first)||
       std::memcmp(trampoline+6,"\xff\x25\0\0\0\0",6)){e="move own hook physical bytes differ";return false;}
    uintptr_t back=0;std::memcpy(&back,trampoline+12,8);if(back!=address+6){e="move own trampoline return differs";return false;}
    int32_t displacement=0;std::memcpy(&displacement,target+1,4);const uintptr_t destination=uintptr_t(int64_t(address+5)+displacement);
    if(!r.memory.read(destination,relay,sizeof relay,e))return false;
    uintptr_t detour=0;std::memcpy(&detour,relay+6,8);
    if(std::memcmp(relay,"\xff\x25\0\0\0\0",6)||detour!=uintptr_t(&purchase_entry)){e="move own relay target differs";return false;}
    return true;
}
#include "move_selection_refresh.inc"
#include "move_widget_refresh.inc"
struct ExecuteServices final:move_kiss::ExecutionServices {
    Runtime& runtime;remove_diagnostics::LocalTarget target;
    host::WorldState before_world;witness::UndoWitness undo_before;action::Request request;
    witness::SelectionWitness selection_before;move_selection_refresh::Receipt outline_refresh;
    move_widget_refresh::Receipt widget_refresh;
    explicit ExecuteServices(Runtime& r):runtime(r){}
    bool prepare(const action::Request& req,move_kiss::LocalBinding& b,std::string& e)override{
        request=req;uint64_t revision=0;
        if(!contract::pending(typed_action_runtime::status(),req,revision,e)||!remove_diagnostics::resolve_record_target(req.snapshot_uuid,req.canonical_record_key,target,e))return false;
        native::BuildingState actual;
        if(!current_prop(runtime,target,before_world,actual,e)||!ledger(b.ledger,e)||!witness::read_undo(runtime.memory.process,runtime.image,undo_before,e))return false;
        if(undo_before.active_graph){e="move native Undo graph already active";return false;}
        if(target.normal_building){
            witness::SelectionCounts counts;
            if(!witness::read_selection(runtime.memory.process,runtime.image,before_world.selection,selection_before,counts,e))return false;
        }
        bool movable=false;const auto exception=can_move(runtime.image,target.object,movable);
        if(exception||!movable){e="move native CanMove rejected SEH="+std::to_string(exception);return false;}
        if(req.dungeon_building){
            if(!remove_diagnostics::is_dungeon_building_target(target,req.dungeon_scene,e))return false;
            if(req.dungeon_scene!="world"){
                uintptr_t shard=0,dungeon=0;int32_t instances=-1;
                if(!runtime.memory.read(target.object+0x170,&shard,sizeof shard,e)||!shard||
                   !runtime.memory.read(shard+0x18,&dungeon,sizeof dungeon,e)||!dungeon||
                   !runtime.memory.read(dungeon+0x4c8,&instances,sizeof instances,e))return false;
                if(instances){e="Dungeon Building original MovedByCursor is suppressed while Instances are active";return false;}
                if(!scenery_kiss::move_destination(runtime.memory.process,runtime.image,shard,req.building,e))return false;
            }else{
                native::RegionState region;
                if(!native::read_region_state(req.building.x,req.building.z,region,e))return false;
                if(!region.owned||!region.building_manager_present||!region.path_manager_present){e="Dungeon Exterior Move destination region unavailable or not owned";return false;}
            }
        }else if(target.normal_scenery){
            if(!scenery_kiss::move_destination(runtime.memory.process,runtime.image,before_world.root,req.building,e))return false;
        }else{
            native::RegionState region;
            if(!native::read_region_state(req.building.x,req.building.z,region,e))return false;
            if(!region.owned||!region.building_manager_present||!region.path_manager_present){e="move destination native region unavailable or not owned";return false;}
        }
        b.origin={req.snapshot_uuid,before_world.origin.world_epoch,before_world.origin.generation,uintptr_t(before_world.origin.world)};
        b.target=target.object;b.local_uid=target.uid;b.actual=world_state::building_state(actual);
        return runtime.memory.read(target.object+0x100,b.transform.data(),b.transform.size(),e);
    }
    bool readback(const move_kiss::LocalBinding& old,uintptr_t graph,move_kiss::Readback& out,std::string& e)override{
        native::BuildingState actual;host::WorldState after_world;
        if(!current_prop(runtime,target,after_world,actual,e)||!host::same_world(before_world.origin,after_world.origin)||!ledger(out.current.ledger,e))return false;
        out.current.origin=old.origin;out.current.target=target.object;out.current.local_uid=actual.prop.uid;out.current.actual=world_state::building_state(actual);
        out.manager_registered=true;
        witness::UndoWitness undo_after;witness::GraphWitness measured;
        if(!witness::read_undo(runtime.memory.process,runtime.image,undo_after,e)||!witness::read_graph(runtime.memory.process,runtime.image,graph,measured,e)||
           !witness::validate_undo_commit(undo_before,undo_after,measured,e))return false;
        uintptr_t data=0;
        if(!runtime.memory.read(graph+0x20,&data,sizeof data,e))return false;
        size_t matches=0;
        for(int32_t index=0;index<measured.operations;++index){
            uintptr_t operation=0,ap=0;uint64_t uid=0;
            if(!runtime.memory.read(data+size_t(index)*8,&operation,sizeof operation,e)||!runtime.memory.read(operation,&ap,sizeof ap,e))return false;
            if(ap!=runtime.image+0x1786ff0)continue;
            if(!runtime.memory.read(operation+0xb8,&uid,sizeof uid,e))return false;
            if(uid!=target.uid)continue;
            auto inverse=old.actual;
            if(!runtime.memory.read(operation+0xc0,inverse.rotation.data(),16,e)||!runtime.memory.read(operation+0xd0,&inverse.x,4,e)||
               !runtime.memory.read(operation+0xd4,&inverse.y,4,e)||!runtime.memory.read(operation+0xd8,&inverse.z,4,e)||!runtime.memory.read(operation+0xdc,inverse.scale.data(),12,e))return false;
            if(inverse==old.actual)++matches;
        }
        out.inverse_registered=matches==1;
        witness::SelectionWitness selected;witness::SelectionCounts counts;
        if(!witness::read_selection(runtime.memory.process,runtime.image,after_world.selection,selected,counts,e))return false;
        out.selection_present=std::find(selected.lists[1].begin(),selected.lists[1].end(),target.object)!=selected.lists[1].end();
        if(target.normal_building&&!move_selection_refresh::retained(selection_before,selected,target.object,outline_refresh,
            [&](move_selection_refresh::Receipt& receipt,std::string& error){return refresh_outline(runtime,target.object,receipt,error);},
            [&](witness::SelectionWitness& current,std::string& error){witness::SelectionCounts current_counts;
                return witness::read_selection(runtime.memory.process,runtime.image,after_world.selection,current,current_counts,error);},e))return false;
        if(outline_refresh.required){
            if(!refresh_widgets(runtime,target.object,widget_refresh,e))return false;
            if(widget_refresh.required){
                witness::SelectionWitness current;witness::SelectionCounts current_counts;
                if(!witness::read_selection(runtime.memory.process,runtime.image,after_world.selection,current,current_counts,e)){
                    widget_refresh.error=e;return false;
                }
                if(current!=selected){e="move widget refresh changed local selection";widget_refresh.error=e;return false;}
                widget_refresh.selection_preserved=true;
            }
            native::BuildingState refreshed;host::WorldState refreshed_world;
            if(!current_prop(runtime,target,refreshed_world,refreshed,e)||!host::same_world(after_world.origin,refreshed_world.origin)||
               world_state::building_state(refreshed)!=out.current.actual){
                if(e.empty())e="move presentation refresh changed current world or Building pose";
                if(widget_refresh.required)widget_refresh.error=e;return false;
            }
        }
        if(widget_refresh.required)widget_refresh.world_pose_preserved=true;
        out.edit_closed=after_world.edits==0&&after_world.active_graph==0;
        return true;
    }
};
}
#include "prop_text_runtime.inc"
#include "district_text_runtime.inc"
#include "district_title_runtime.inc"
#include "district_edit_runtime.inc"
namespace group_edit { namespace { bool find(Runtime&,uint64_t,uintptr_t&,std::string&); } }
#include "district_stroke_sample.inc"
#include "district_stroke_capture.inc"
#include "district_stroke_execute.inc"
#include "district_stroke_remove.inc"
#include "district_stroke_history.inc"
#include "district_stroke_routes.inc"
#include "group_runtime.inc"
#include "boss_phase_runtime.inc"
#include "service_membership_runtime.inc"
#include "selection_move_runtime.inc"
#include "duplicate_runtime.inc"
#include "scenery_stroke_sample.inc"
#include "scenery_stroke_capture_runtime.inc"
#include "scenery_stroke_routes.inc"
#include "scenery_stroke_port.inc"
#include "scenery_stroke_forward.inc"
#include "scenery_stroke_history_runtime.inc"
#include "building_clearance_runtime.inc"
#include "group_object_sample.inc"
#include "group_object_capture.inc"
#include "group_object_runtime.inc"
#include "group_object_history.inc"
#include "group_object_exports.inc"
#include "modular_sample.inc"
#include "modular_dependencies.inc"
#include "modular_port.inc"
#include "modular_graph.inc"
#include "modular_history.inc"
#include "modular_capture.inc"
#include "modular_forward.inc"
#include "boss_placement_sample.inc"
#include "boss_placement_capture.inc"
#include "boss_placement_execution.inc"
#include "boss_placement_routes.inc"
bool enable(std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r){r=new Runtime;published.store(r,std::memory_order_release);}
    if(r->attempted||r->thread!=GetCurrentThreadId()){e="move hook requires one installation on Mainthread";return false;}
    r->attempted=true;uint32_t thread=0;std::string sha;
    auto failed=[&](){r->cause=e;LOG_ERROR("[UIMove] Install failed cause={}",e);return false;};
    if(!net::session_wire::connected()||!typed_action_runtime::status().value("started",false)){e="move hook requires connected session and started Controller";return failed();}
    if(!native::read_resolver_receipt(r->image,thread,sha,e)||thread!=r->thread||!move_kiss::resolve_calls(r->memory,r->image,sha,r->calls,e))return failed();
    const unsigned char expected[]{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xd8};unsigned char actual[sizeof expected]{};
    if(!r->memory.read(r->image+0x5c25b0,actual,sizeof actual,e)||std::memcmp(actual,expected,sizeof actual)){if(e.empty())e="move Purchase entry differs";return failed();}
    const unsigned char movable_code[]{0xb8,1,0,0,0,0xc3};unsigned char movable_actual[sizeof movable_code]{};
    if(!r->memory.read(r->image+0xd31530,movable_actual,sizeof movable_actual,e)||std::memcmp(movable_actual,movable_code,sizeof movable_actual)){if(e.empty())e="move CanMove code differs";return failed();}
    if(!r->memory.read(r->image+0xcf3130,movable_actual,sizeof movable_actual,e)||std::memcmp(movable_actual,movable_code,sizeof movable_actual)){if(e.empty())e="Scenery CanMove code differs";return failed();}
    if(!r->memory.read(r->image+0xd32130,movable_actual,sizeof movable_actual,e)||
       std::memcmp(movable_actual,movable_code,sizeof movable_actual)){
        if(e.empty())e="NPC CanMove code differs";return failed();}
    r->capture.emplace(r->memory,r->services,r->image);
    Purchase original=nullptr;
    if(!hooks::create_hook(reinterpret_cast<void*>(r->image+0x5c25b0),reinterpret_cast<void*>(&purchase_entry),reinterpret_cast<void**>(&original))){e="move Purchase CreateHook failed";return failed();}
    r->original.store(original,std::memory_order_release);
    r->created=true;
    if(!trampoline_route(*r,e))return failed();
    const auto result=MH_EnableHook(reinterpret_cast<void*>(r->image+0x5c25b0));
    if(result!=MH_OK){e="move Purchase EnableHook failed: "+std::string(MH_StatusToString(result));return failed();}
    if(!hook_route(*r,e)){
        const auto disabled=MH_DisableHook(reinterpret_cast<void*>(r->image+0x5c25b0));
        if(disabled!=MH_OK)e+="; own hook Disable failed: "+std::string(MH_StatusToString(disabled));
        return failed();
    }
    if(!group_edit::enable(e)||!district_edit::enable(e)||!district_stroke::enable(e)||!district_title::enable(e)||!district_text::enable(e)||!prop_text::enable(e)||!boss_phase::enable(e)||!service_membership::enable(e))return failed();
    r->ready=true;
    if(!scenery_stroke::enable(e)||!modular::enable(e)||!boss_placement::enable(e)){r->ready=false;return failed();}
    r->cause.clear();return true;
}
bool enabled()noexcept{const auto* r=published.load(std::memory_order_acquire);return r&&r->ready&&r->thread==GetCurrentThreadId();}
void tick(){
    boss_placement::tick();
    group_object::tick();
    scenery_stroke::tick();
    modular::tick();
    duplicate_edit::tick();
    group_edit::tick();
    district_edit::tick();
    district_stroke::tick();
    district_title::tick();
    district_text::tick();
    prop_text::tick();
    boss_phase::tick();
    service_membership::tick();
    selection_move::tick();
    auto* r=published.load(std::memory_order_acquire);if(!r||!enabled()||!r->pending)return;
    const auto intent=*r->pending;const auto id=r->pending_id;r->pending.reset();r->pending_id.clear();std::string e;
    move_kiss::Origin fresh;remove_diagnostics::LocalTarget target;native::BuildingState actual;
    const bool npc=npc_intent(intent),monster=intent.family==move_kiss::Family::DirectMonster;
    action::BuildingState npc_before;std::string npc_key;
    if(!cleanup(*r,intent,e)||!r->services.active(fresh,e)||fresh!=intent.origin||
       (monster?!npc_probe::monster_action_runtime::move_capture_binding(intent.origin.snapshot_uuid,
                  intent.origin.world_epoch,intent.origin.generation,intent.target,intent.local_uid,
                  npc_before,npc_key,e)||npc_key!=intent.object_key||npc_before!=intent.before:
         npc?!npc_probe::action_runtime::move_capture_binding(intent.origin.snapshot_uuid,
                 intent.origin.world_epoch,intent.origin.generation,intent.target,intent.local_uid,
                 npc_before,npc_key,e)||npc_key!=intent.object_key||npc_before!=intent.before:
            !remove_diagnostics::resolve_record_target(intent.origin.snapshot_uuid,intent.object_key,target,e)||
                 target.object!=intent.target||target.uid!=intent.local_uid||
                 !scenery_kiss::read_action_prop(target.uid,actual,e)||
                 world_state::building_state(actual)!=intent.before)){
        if(e.empty())e="move cleanup world/target or old pose changed";
        if(!npc&&!monster)move_ui_probe::failed(intent.target,e);r->services.reject(e);return;}
    r->last_ui_action=id;r->last_ui_cleaned=true;
    if(!(monster?npc_probe::monster_action_runtime::submit_restored_move(intent,id,e):
         npc?npc_probe::action_runtime::submit_restored_move(intent,id,e):
         typed_action_runtime::submit_move(id,intent.object_key,intent.after,e))){
        if(!npc&&!monster)move_ui_probe::failed(intent.target,e);r->services.reject(e);return;}
    if(npc)npc_probe::action_runtime::move_submitted(id);
    else if(!monster)move_ui_probe::submitted(id);
    ++r->submitted;
}
bool move(const action::Request& req,action::BuildingState& state,json& receipt,std::string& e){
    state={};receipt=json::object();auto* r=published.load(std::memory_order_acquire);
    if(!enabled()){e="move native runtime unavailable on Mainthread";return false;}
    if(r->last_ui_action==req.action_uuid&&!r->last_ui_cleaned){e="move captured UI cleanup unconfirmed";return false;}
    move_kiss::Calls current_calls;
    if(!move_kiss::resolve_calls(r->memory,r->image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",current_calls,e)){
        receipt={{"token",req.action_uuid},{"attempted",false},{"confirmed",false},{"error",e}};
        r->last_native=receipt;r->services.reject(e);return false;
    }
    r->calls=current_calls;
    ExecuteServices io(*r);move_kiss::Receipt native_receipt;action::Evidence evidence;
    const auto outcome=move_kiss::execute(io,r->calls,req,native_receipt,evidence,e);
    const bool success=outcome==action::Outcome::Confirmed;
    receipt={{"token",req.action_uuid},{"snapshot_id",req.snapshot_uuid},{"record_key",req.canonical_record_key},
        {"world_epoch",io.target.world_epoch},{"world_generation",io.target.world_generation},
        {"attempted",native_receipt.started},{"confirmed",success},{"native_uid",io.target.uid},{"undo_graph",native_receipt.graph},
        {"undo_confirmed",evidence.undo_confirmed},{"selection_confirmed",evidence.selection_confirmed},
        {"cleanup_confirmed",success&&native_receipt.dtor_returned&&native_receipt.close_returned&&native_receipt.end_returned&&native_receipt.vector_returned&&native_receipt.string_returned},
        {"move_attempted",native_receipt.move_attempted},{"move_returned",native_receipt.move_returned},{"ctor_attempted",native_receipt.ctor_attempted},
        {"ctor_returned",native_receipt.ctor_returned},{"dtor_attempted",native_receipt.dtor_attempted},{"dtor_returned",native_receipt.dtor_returned},
        {"exception",native_receipt.exception},{"cleanup_exception",native_receipt.cleanup_exception},{"error",e}};
    receipt["outline_refresh"]={{"required",io.outline_refresh.required},{"attempted",io.outline_refresh.attempted},
        {"returned",io.outline_refresh.returned},{"selection_preserved",io.outline_refresh.selection_preserved},
        {"exception",io.outline_refresh.exception}};
    receipt["widget_refresh"]={{"required",io.widget_refresh.required},{"attempted",io.widget_refresh.attempted},
        {"returned",io.widget_refresh.returned},{"readback",io.widget_refresh.readback},
        {"selection_preserved",io.widget_refresh.selection_preserved},{"world_pose_preserved",io.widget_refresh.world_pose_preserved},
        {"widget_count",io.widget_refresh.widget_count},{"exception",io.widget_refresh.exception},{"error",io.widget_refresh.error}};
    receipt["loc_attempted"]=native_receipt.loc_attempted;receipt["loc_returned"]=native_receipt.loc_returned;
    receipt["begin_attempted"]=native_receipt.begin_attempted;receipt["begin_returned"]=native_receipt.begin_returned;
    receipt["open_attempted"]=native_receipt.open_attempted;receipt["open_returned"]=native_receipt.open_returned;
    receipt["close_attempted"]=native_receipt.close_attempted;receipt["close_returned"]=native_receipt.close_returned;
    receipt["end_attempted"]=native_receipt.end_attempted;receipt["end_returned"]=native_receipt.end_returned;
    receipt["vector_attempted"]=native_receipt.vector_attempted;receipt["vector_returned"]=native_receipt.vector_returned;
    receipt["string_attempted"]=native_receipt.string_attempted;receipt["string_returned"]=native_receipt.string_returned;
    r->last_native=receipt;
    if(success)state=evidence.object_after;
    return success;
}
json status(){const auto* r=published.load(std::memory_order_acquire);if(!r)return {{"initialized",false}};
    if(r->thread!=GetCurrentThreadId())return {{"initialized",false},{"error","move status requires bound Mainthread"}};
    return {{"initialized",true},{"enabled",r->ready.load()},{"pending_ui",r->pending.has_value()||modular::pending()||boss_placement::pending()||group_object::pending()||duplicate_edit::pending()||group_edit::pending()||district_stroke::pending()||district_edit::pending()||district_title::pending()||district_text::pending()||prop_text::pending()||boss_phase::pending()||service_membership::pending()||selection_move::pending()},{"scenery_stroke",scenery_stroke::status()},{"modular",modular::status()},{"boss_placement",boss_placement::status()},{"group_object",group_object::status()},{"duplicate",duplicate_edit::status()},{"group_edit",group_edit::status()},{"district_stroke",district_stroke::status()},{"district_edit",district_edit::status()},{"district_title",district_title::status()},{"district_text",district_text::status()},{"prop_text",prop_text::status()},{"boss_phase",boss_phase::status()},{"service_membership",service_membership::status()},{"selection_move",selection_move::status()},{"captured",r->captured},{"submitted",r->submitted},
        {"rejected",r->rejected},{"forwarded",r->forwarded.load()},{"last_ui_action",r->last_ui_action},{"ui_cleanup_confirmed",r->last_ui_cleaned},{"cause",r->cause},{"last_native",r->last_native}};
}
#include "object_ui_probe/runtime.inc"
bool can_reclaim_global_minhook(std::string& e){const auto* r=published.load(std::memory_order_acquire);if(r&&r->created){e="move hook retains process-lifetime typed original";return false;}return true;}
}

#include "typed_action_runtime.h"
#include "time_integration.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/session_wire.h"
#include "live_host_state/bridge.h"
#include "live_host_state/economic_fence.h"
#include "live_host_state/day_rollover/native_day.h"
#include "live_host_state/day_rollover/economy/native_monetary.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/net/software_sync.h"
#include "typed_action/action_session.h"
#include "typed_action/sidecar_adapter.h"
#include "typed_action/steam_action_adapter.h"
#include "session_provider.h"
#include "mt2mp/core/config.h"
#include "remove_diagnostics.h"
#include "scenery_kiss/native_catalog.h"
#include "scenery_kiss/lifecycle_contract.h"
#include "native_open_runtime.h"
#include "tutorial_replication/native.h"
#include "ui_remove_runtime.h"
#include "ui_build_runtime.h"
#include "building_history_slice/native_bridge.h"
#include "ui_move_runtime.h"
#include "ui_region_runtime.h"
#include "ui_price_runtime.h"
#include "npc_probe.h"
#include "ui_terrain_runtime.h"
#include "ui_path_runtime.h"
#include "ui_network_runtime.h"
#include "move_contract.h"
#include "multiplayer_world/native_binding_adapter.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/logger.h"
#include <Windows.h>
#include <atomic>

namespace mt2mp::game::typed_action_runtime {
namespace {
using json=nlohmann::json;
namespace action=mt2mp::action_session;
bool read_ledger(action::Ledger& out,std::string& error) {
    native::SimulationState actual;
    if(!native::read_simulation_state(actual,error))return false;
    out={actual.cash,actual.clock.flat_day,int32_t(actual.record_present),actual.record_day,
        actual.record_cash,actual.record_income,actual.record_expense,actual.line_counts,actual.line_income};
    return true;
}
class NativeEngine final:public action::Engine {
    struct Operation {action::Request request;action::Binding binding;action::Ledger before;json removal,retirement;action::BuildingState object_before;uint64_t local_uid=0;};
    std::map<std::string,Operation> operations_;
    struct BuildOperation {action::Request request;action::Binding binding;action::Ledger before;action::BuildingState state;json receipt;bool retired=false;action::BuildingState object_before;bool normal_scenery=false;};
    std::map<std::string,BuildOperation> buildings_;
    struct HistoryOperation {action::Request request;action::Binding binding;action::Ledger before,after;
        action::BuildingState object_before,object_after;uint64_t uid=0;bool confirmed=false,retired=false;};
    std::map<std::string,HistoryOperation> history_;
    world_state::Table world_;
    uint32_t thread_=0;
    live_host_state::Bridge* live_state_=nullptr;
    live_host_state::NativePort* live_port_=nullptr;
    std::optional<action::Message> live_result_;
    const action_session::ui_capture::Replay* active_ui_replay_=nullptr;
    const action::Evidence* active_host_evidence_=nullptr;
    action::Outcome perform_remove(const action::Binding& binding,const action::Request& request,
        action_session::ui_capture::ReplayPurpose purpose,const action::Evidence* authority,std::string& error) {
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(operations_.contains(request.action_uuid)){error="typed native action token already attempted; no replay";return action::Outcome::Unknown;}
        action::Ledger before;if(!read_ledger(before,error))return action::Outcome::RejectedNoEffect;
        action::BuildingState object_before;uint64_t local_uid=0;
        if(request.canonical_record_key.starts_with("action:")) {
            remove_diagnostics::LocalTarget target;native::BuildingState actual;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
               !scenery_kiss::read_action_prop(target.uid,actual,error))return action::Outcome::RejectedNoEffect;
            object_before=world_state::building_state(actual);local_uid=target.uid;
            world_state::LocalBinding settled;
            if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,settled,error)||
               !world_.refresh_current(world_.origin(),request.canonical_record_key,settled,error))return action::Outcome::RejectedNoEffect;
            const auto row=world_.rows().find(request.canonical_record_key);
            if(row==world_.rows().end()||row->second.state!=object_before) {
                error="shared removal object state differs";return action::Outcome::RejectedNoEffect;
            }
        }
        action_session::ui_capture::Replay replay;
        if(authority&&object_before!=authority->object_before) {
            error="shared removal native object differs from Host before state";return action::Outcome::RejectedNoEffect;
        }
        const bool captured=ui_remove_runtime::enabled();
        if(ui_remove_runtime::requested()&&!captured) {
            error="typed native removal requires the requested UI capture; installation is unavailable";return action::Outcome::RejectedNoEffect;
        }
        if(captured&&!ui_remove_runtime::make_replay(request.snapshot_uuid,request.canonical_record_key,request.action_uuid,purpose,replay,error))
            return action::Outcome::RejectedNoEffect;
        auto& operation=operations_.emplace(request.action_uuid,Operation{request,binding,before,{},{},object_before,local_uid}).first->second;
        const json args={{"token",request.action_uuid},{"snapshot_id",request.snapshot_uuid},{"record_key",request.canonical_record_key},
            {"expected_world_epoch",binding.native_epoch},{"expected_world_generation",binding.native_generation}};
        const auto native_call=[&](std::string& cause){return remove_diagnostics::remove(args,operation.removal,cause);};
        bool removed=false;
        if(captured) {
            if(active_ui_replay_){error="typed native authoritative UI replay is reentrant";return action::Outcome::RejectedNoEffect;}
            active_ui_replay_=&replay;active_host_evidence_=authority;
            struct Reset {NativeEngine& engine;~Reset(){engine.active_ui_replay_=nullptr;engine.active_host_evidence_=nullptr;}} reset{*this};
            removed=ui_remove_runtime::with_replay(replay,native_call,error);
        } else removed=native_call(error);
        if(!removed)return action::Outcome::Unknown;
        if(operation.removal.value("confirmed",false)!=true){error="typed native removal lacks confirmation";return action::Outcome::Unknown;}
        return action::Outcome::Confirmed;
    }
public:
    action::Outcome perform_place(const action::Binding& binding,const action::Request& request,
                                 const action::Evidence* authority,std::string& error) {
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(buildings_.contains(request.action_uuid)){error="native building action already attempted";return action::Outcome::Unknown;}
        const auto controller=typed_action_runtime::status();
        if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid) {
            error="native building action has no exact pending Controller request";return action::Outcome::RejectedNoEffect;
        }
        action::Ledger before;if(!read_ledger(before,error))return action::Outcome::RejectedNoEffect;
        if(authority&&before!=authority->before){error="native building client ledger differs from Host";return action::Outcome::RejectedNoEffect;}
        world_state::Change change{world_state::Kind::Create,controller.at("host_revision").get<uint64_t>(),
            request.action_uuid,request.canonical_record_key,{},request.building};
        if(world_.prepare(world_.origin(),change,error)!=world_state::Admission::New)return action::Outcome::RejectedNoEffect;
        auto& operation=buildings_.emplace(request.action_uuid,BuildOperation{request,binding,before,{},{}}).first->second;
        if(!ui_build_runtime::place(request,operation.state,operation.receipt,error))
            return operation.receipt.value("attempted",false)?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
        if(!building_history_slice::remember_place(request.action_uuid,
                operation.receipt.at("local_undo_graph").get<uint64_t>(),error))return action::Outcome::Unknown;
        const auto uid=operation.receipt.at("native_uid").get<uint64_t>();
        if(!remove_diagnostics::admit_created_building(request.snapshot_uuid,request.canonical_record_key,uid,error))return action::Outcome::Unknown;
        remove_diagnostics::LocalTarget target;native::BuildingState actual;
        if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
           !scenery_kiss::read_action_prop(uid,actual,error))return action::Outcome::Unknown;
        operation.normal_scenery=target.normal_scenery;
        world_state::LocalBinding observed;
        if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error))return action::Outcome::Unknown;
        if(observed.state!=operation.state){error="native building state changed before ID binding";return action::Outcome::Unknown;}
        if(authority&&operation.state!=authority->object_after){error="native client building differs from Host result";return action::Outcome::Unknown;}
        change.after=operation.state;
        if(world_.commit(world_.origin(),change,observed,error)!=world_state::Admission::New)return action::Outcome::Unknown;
        return action::Outcome::Confirmed;
    }
    action::Outcome perform_move(const action::Binding& binding,const action::Request& request,
                                 const action::Evidence* authority,std::string& error) {
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(buildings_.contains(request.action_uuid)){error="native Move action already attempted; no replay";return action::Outcome::Unknown;}
        uint64_t revision=0;
        if(!ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error))return action::Outcome::RejectedNoEffect;
        action::Ledger before;if(!read_ledger(before,error))return action::Outcome::RejectedNoEffect;
        remove_diagnostics::LocalTarget target;native::BuildingState actual;
        if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
           !scenery_kiss::read_action_prop(target.uid,actual,error))return action::Outcome::RejectedNoEffect;
        world_state::LocalBinding observed;
        if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error)||
           !world_.refresh_current(world_.origin(),request.canonical_record_key,observed,error))return action::Outcome::RejectedNoEffect;
        const auto object_before=observed.state;
        if(authority&&(before!=authority->before||object_before!=authority->object_before)) {
            error="native Move Client before ledger/object differs from Host";return action::Outcome::RejectedNoEffect;
        }
        auto native_request=request;
        if(authority)native_request.building=authority->object_after;
        world_state::Change change{world_state::Kind::Move,revision,request.action_uuid,request.canonical_record_key,object_before,native_request.building};
        if(world_.prepare(world_.origin(),change,error)!=world_state::Admission::New)return action::Outcome::RejectedNoEffect;
        auto& operation=buildings_.emplace(request.action_uuid,BuildOperation{request,binding,before,{},{},false,object_before}).first->second;
        if(!ui_move_runtime::move(native_request,operation.state,operation.receipt,error))
            return operation.receipt.value("attempted",false)?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
        const auto uid=operation.receipt.at("native_uid").get<uint64_t>();
        if(uid!=target.uid){error="native Move changed local UID";return action::Outcome::Unknown;}
        action::Ledger after;
        if(!current(binding,request.snapshot_uuid,error)||!read_ledger(after,error)||after!=before||
           (authority&&(after!=authority->after||operation.state!=authority->object_after))) {
            if(error.empty())error="native Move ledger or authoritative state differs";return action::Outcome::Unknown;
        }
        if(!scenery_kiss::read_action_prop(uid,actual,error)||world_state::building_state(actual)!=operation.state) {
            if(error.empty())error="native Move readback changed before registry refresh";return action::Outcome::Unknown;
        }
        if(!remove_diagnostics::refresh_moved_building(request.snapshot_uuid,request.canonical_record_key,uid,error)||
           !remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
           !world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error))return action::Outcome::Unknown;
        change.after=operation.state;
        if(world_.commit(world_.origin(),change,observed,error)!=world_state::Admission::New)return action::Outcome::Unknown;
        return action::Outcome::Confirmed;
    }
    action::Outcome perform_building_history(const action::Binding& binding,const action::Request& request,
                                             const action::Evidence* authority,std::string& error) {
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding) {
            error="building history direction invalid";return action::Outcome::RejectedNoEffect;
        }
        if(history_.contains(request.action_uuid)) {
            error="building history action already attempted; no replay";return action::Outcome::Unknown;
        }
        const auto original=buildings_.find(request.target_action_uuid);
        if(original==buildings_.end()||original->second.request.action!=action::Action::PlaceBuilding||
           original->second.request.canonical_record_key!=request.canonical_record_key||
           !original->second.receipt.value("confirmed",false)||!original->second.retired||
           original->second.normal_scenery) {
            error="building history target has no confirmed normal building placement";
            return action::Outcome::RejectedNoEffect;
        }
        const auto row=world_.rows().find(request.canonical_record_key);
        const bool undo=request.action==action::Action::UndoBuilding;
        auto alive=original->second.state;
        auto dead=alive;dead.alive=false;
        if(row==world_.rows().end()||row->second.state!=(undo?alive:dead)) {
            error="building history target full shared state differs";return action::Outcome::RejectedNoEffect;
        }
        const auto controller=typed_action_runtime::status();
        if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||
           !controller.contains("host_revision")) {
            error="building history lacks exact pending Controller request";return action::Outcome::RejectedNoEffect;
        }
        const auto revision=controller.at("host_revision").get<uint64_t>();
        if(revision<=world_.revision()) {error="building history shared revision stale";return action::Outcome::RejectedNoEffect;}
        action::Ledger before;
        if(!read_ledger(before,error))return action::Outcome::RejectedNoEffect;
        const auto uid=original->second.receipt.at("native_uid").get<uint64_t>();
        const auto cost=original->second.receipt.at("cost").get<int32_t>();
        if(!uid||cost<0){error="building history original UID/cost invalid";return action::Outcome::RejectedNoEffect;}
        if(undo) {
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding settled;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)) {
                error="building history Undo target resolution failed: "+error;return action::Outcome::RejectedNoEffect;
            }
            if(target.uid!=uid) {
                error="building history Undo local UID differs actual="+std::to_string(target.uid)+" required="+std::to_string(uid);
                return action::Outcome::RejectedNoEffect;
            }
            if(!scenery_kiss::read_action_prop(uid,actual,error)) {
                error="building history Undo native readback failed uid="+std::to_string(uid)+" cause="+error;
                return action::Outcome::RejectedNoEffect;
            }
            if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,settled,error)||
               !world_.refresh_current(world_.origin(),request.canonical_record_key,settled,error))return action::Outcome::RejectedNoEffect;
            // Native Undo records its inverse from the current settled transform.
            // Keep that exact pose for the tombstone, Host evidence and later Redo.
            alive=settled.state;dead=alive;dead.alive=false;original->second.state=alive;
        }
        if(authority&&(authority->before!=before||authority->object_before!=(undo?alive:dead))) {
            error="building history Client before state differs from Host";return action::Outcome::RejectedNoEffect;
        }
        auto& operation=history_.emplace(request.action_uuid,HistoryOperation{request,binding,before,{},
            undo?alive:dead,undo?dead:alive,uid,false}).first->second;
        const auto outcome=building_history_slice::perform(request.action,request.target_action_uuid,error);
        if(outcome!=action::Outcome::Confirmed)return outcome;
        if(!read_ledger(operation.after,error))return action::Outcome::Unknown;
        const auto expected_cash=int64_t(before.cash)+(undo?int64_t(cost):-int64_t(cost));
        if(expected_cash<INT32_MIN||expected_cash>INT32_MAX||operation.after.cash!=expected_cash||
           (authority&&operation.after!=authority->after)) {
            error="building history actual cash/Host ledger differs";return action::Outcome::Unknown;
        }
        if(undo) {
            if(!remove_diagnostics::retire_undone_building(request.snapshot_uuid,request.canonical_record_key,uid,error)||
               !world_.retire_native_effects(world_.origin(),revision,request.action_uuid,
                                             {request.canonical_record_key},error))return action::Outcome::Unknown;
        } else {
            if(!remove_diagnostics::restore_redone_building(request.snapshot_uuid,request.canonical_record_key,uid,error))
                return action::Outcome::Unknown;
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
               target.uid!=uid||!scenery_kiss::read_action_prop(uid,actual,error)||
               world_state::building_state(actual)!=alive||
               !world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error)||
               !world_.restore_native_building(world_.origin(),revision,request.action_uuid,
                                               request.target_action_uuid,observed,error))return action::Outcome::Unknown;
        }
        operation.confirmed=true;error.clear();return action::Outcome::Confirmed;
    }
    bool authorize_ui_replay(const action_session::ui_capture::Replay& replay,const action::Context& context,
        const action::Status& controller,std::string& error) {
        if(!active_ui_replay_||replay.purpose!=active_ui_replay_->purpose||replay.intent.action_uuid!=active_ui_replay_->intent.action_uuid||
           replay.intent.snapshot_uuid!=active_ui_replay_->intent.snapshot_uuid||replay.intent.canonical_record_key!=active_ui_replay_->intent.canonical_record_key||
           replay.binding!=active_ui_replay_->binding) {error="typed UI replay has no exact active private bridge scope";return false;}
        const auto operation=operations_.find(replay.intent.action_uuid);
        if(operation==operations_.end()||!controller.started||controller.locked||!controller.pending||controller.action_uuid!=replay.intent.action_uuid||
           context.snapshot_uuid!=replay.intent.snapshot_uuid||!current(operation->second.binding,replay.intent.snapshot_uuid,error)||
           operation->second.request.snapshot_uuid!=replay.intent.snapshot_uuid||operation->second.request.canonical_record_key!=replay.intent.canonical_record_key||
           operation->second.binding.native_epoch!=replay.binding.native_epoch||operation->second.binding.native_generation!=replay.binding.native_generation) {
            if(error.empty())error="typed UI replay actual Controller/request/world authority differs";return false;
        }
        if(replay.purpose==action_session::ui_capture::ReplayPurpose::HostExecution) {
            if(context.role!=action::Role::Host||context.local_peer!=context.host_peer||active_host_evidence_) {
                error="typed UI replay HostExecution is outside actual Host execute callback";return false;
            }
        } else if(replay.purpose==action_session::ui_capture::ReplayPurpose::ConfirmedHostApplication) {
            if(context.role!=action::Role::Client||context.local_peer==context.host_peer||!active_host_evidence_||
               active_host_evidence_->before!=operation->second.before) {
                error="typed UI replay application lacks actual Client HostResult evidence";return false;
            }
        } else {error="typed UI replay purpose invalid";return false;}
        return true;
    }
    bool bind(const std::string& snapshot,action::Binding& out,std::string& error)override {
        building_mixed_identity::opened_file::CurrentWorld before,opened;
        if(!native_open_runtime::read_current_world(before,error))return false;
        json sources;if(!remove_diagnostics::bindings(sources,error))return false;
        if(!native_open_runtime::read_current_world(opened,error))return false;
        if(before.game!=opened.game||before.mode!=opened.mode||before.state!=opened.state||
           before.world!=opened.world||before.map!=opened.map||before.uuid!=opened.uuid||
           before.sha256!=opened.sha256||before.world_epoch!=opened.world_epoch||
           before.generation!=opened.generation){error="typed native action current world changed during binding";return false;}
        out={sources.at("world_epoch").get<uint64_t>(),sources.at("world_generation").get<uint64_t>()};
        if(out.native_epoch!=opened.world_epoch||out.native_generation!=opened.generation){
            error="typed native action baseline world epoch or generation differs";return false;
        }
        bool present=false;
        for(const auto& entry:sources.at("bindings"))if(entry.at("snapshot_id")==snapshot&&!entry.value("revoked",false))present=true;
        if(!present){
            if(!sources.at("bindings").empty()||sources.value("baseline_snapshot_id",std::string{})!=snapshot||
               sources.value("baseline_sha256",std::string{}).empty()){
                error="typed actions require actual current NativeSnapshot bindings or confirmed empty baseline";return false;
            }
        }
        thread_=GetCurrentThreadId();
        if(!current(out,snapshot,error))return false;
        world_state::Origin origin{snapshot,present?opened.sha256:sources.at("baseline_sha256").get<std::string>(),out.native_epoch,out.native_generation};
        std::vector<world_state::SnapshotObject> objects;
        for(const auto& entry:sources.at("bindings")) {
            if(entry.at("snapshot_id")!=snapshot||entry.value("revoked",false))continue;
            const auto key=entry.at("record_key").get<std::string>();
            native::BuildingState native_state;
            if(!scenery_kiss::read_action_prop(entry.at("local_uid").get<uint64_t>(),native_state,error))return false;
            origin.sha256=key.substr(0,64);
            objects.push_back({key,key,world_state::building_state(native_state)});
        }
        return world_.bind_snapshot(origin,objects,0,error);
    }
    bool current(const action::Binding& binding,const std::string&,std::string& error)override {
        if(!thread_||GetCurrentThreadId()!=thread_){error="typed native action requires bound Mainthread";return false;}
        building_mixed_identity::opened_file::CurrentWorld world;
        if(!native_open_runtime::read_current_world(world,error))return false;
        if(world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation) {
            error="typed native action bound world changed";return false;
        }
        return true;
    }
    bool ledger(const action::Binding& binding,action::Ledger& out,std::string& error)override {
        return current(binding,{},error)&&read_ledger(out,error);
    }
    action::Outcome execute_remove(const action::Binding& binding,const action::Request& request,std::string& error)override {
        return perform_remove(binding,request,action_session::ui_capture::ReplayPurpose::HostExecution,nullptr,error);
    }
    action::Outcome apply_remove(const action::Binding& binding,const action::Request& request,const action::Evidence& host,std::string& error)override {
        action::Ledger before;
        if(!ledger(binding,before,error)||before!=host.before){if(error.empty())error="typed client before ledger differs from authority";return action::Outcome::RejectedNoEffect;}
        const auto outcome=perform_remove(binding,request,action_session::ui_capture::ReplayPurpose::ConfirmedHostApplication,&host,error);
        if(outcome!=action::Outcome::Confirmed)return outcome;
        action::Ledger after;
        if(!ledger(binding,after,error)||after!=host.after){if(error.empty())error="typed client after ledger differs from authority";return action::Outcome::Unknown;}
        return outcome;
    }
    action::Outcome execute_action_native(const action::Binding& binding,const action::Request& request,std::string& error) {
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC)
            return npc_probe::action_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetPrices)return ui_price_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)
            return perform_building_history(binding,request,nullptr,error);
        if((request.action==action::Action::PlaceNetworkNode||request.action==action::Action::PlaceNetworkPipe))return ui_network_runtime::perform(binding,request,nullptr,error);
        if((request.action==action::Action::PlacePath||request.action==action::Action::RemovePath))return ui_path_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::TerrainStroke)return ui_terrain_runtime::perform(binding,request,nullptr,error);
        if((request.action==action::Action::SetRegionPreset||request.action==action::Action::SetRegionStyles||request.action==action::Action::EditRegionProperty))return ui_region_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::PlaceBuilding)return perform_place(binding,request,nullptr,error);
        if(request.action==action::Action::MoveBuilding)return perform_move(binding,request,nullptr,error);
        return action::Engine::execute_action(binding,request,error);
    }
    action::Outcome apply_action_native(const action::Binding& binding,const action::Request& request,const action::Evidence& host,std::string& error) {
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC)
            return npc_probe::action_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::SetPrices)return ui_price_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)
            return perform_building_history(binding,request,&host,error);
        if((request.action==action::Action::PlaceNetworkNode||request.action==action::Action::PlaceNetworkPipe))return ui_network_runtime::perform(binding,request,&host,error);
        if((request.action==action::Action::PlacePath||request.action==action::Action::RemovePath))return ui_path_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::TerrainStroke)return ui_terrain_runtime::perform(binding,request,&host,error);
        if((request.action==action::Action::SetRegionPreset||request.action==action::Action::SetRegionStyles||request.action==action::Action::EditRegionProperty))return ui_region_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::MoveBuilding)return perform_move(binding,request,&host,error);
        if(request.action!=action::Action::PlaceBuilding)return action::Engine::apply_action(binding,request,host,error);
        const auto result=perform_place(binding,request,&host,error);
        if(result!=action::Outcome::Confirmed)return result;
        action::Ledger after;
        if(!read_ledger(after,error)||after!=host.after){if(error.empty())error="native client building cost differs from Host";return action::Outcome::Unknown;}
        return result;
    }
    void bind_live_state(live_host_state::Bridge& state,live_host_state::NativePort& port) {live_state_=&state;live_port_=&port;}
    action::Outcome execute_action(const action::Binding& binding,const action::Request& request,std::string& error)override {
        if(live_state_&&live_host_state::economic_fence().locked()){error=live_host_state::economic_fence().cause();return action::Outcome::RejectedNoEffect;}
        const auto outcome=execute_action_native(binding,request,error);
        if(live_state_&&live_host_state::economic_fence().locked()){error=live_host_state::economic_fence().cause();return action::Outcome::Unknown;}return outcome;
    }
    bool receive_authority_state(uint64_t sender,const uint8_t* bytes,size_t size,bool& handled,std::string& error)override {
        handled=false;if(!live_state_)return true;return live_state_->route_receive(sender,bytes,size,handled,error);
    }
    bool capture_host_authority_before(const action::Binding&,const action::Request& r,const action::Ledger& before,uint64_t previous,std::string& error)override {
        if(!live_state_)return true;return live_state_->capture_before(r,before,previous,error);
    }
    bool publish_host_authority_before(const action::Message& m,std::string& error)override {
        if(!live_state_)return true;return live_state_->publish_before(m,error);
    }
    action::AuthorityReadiness client_authority_before_ready(const action::Message& m,uint64_t previous,std::string& error)override {
        if(!live_state_)return action::AuthorityReadiness::Ready;return live_state_->before_ready(1,m,previous,error);
    }
    bool apply_client_authority_before(const action::Binding&,const action::Message& m,uint64_t previous,std::string& error)override {
        if(!live_state_)return true;if(!live_state_->before_result(1,m,previous,error))return false;live_result_=m;return true;
    }
    action::Outcome apply_action(const action::Binding& binding,const action::Request& request,const action::Evidence& host,std::string& error)override {
        if(!live_state_)return apply_action_native(binding,request,host,error);
        if(!live_result_||live_result_->request!=request||live_result_->evidence!=host||!live_port_) {error="live native action lacks exact validated HostResult";return action::Outcome::RejectedNoEffect;}
        live_host_state::Pin pin;auto& fence=live_host_state::economic_fence();
        if(!live_port_->read(pin,error)||!fence.begin_action(*live_result_,pin,error))return action::Outcome::RejectedNoEffect;
        const auto authority=*live_result_;live_result_.reset();action::Outcome outcome=action::Outcome::Unknown;
        try {outcome=apply_action_native(binding,request,host,error);}
        catch(const std::exception& failure){error="live native action original exception: "+std::string(failure.what());}
        catch(...){error="live native action original nonstandard exception";}
        const auto original=error;std::string readback;
        if(!fence.end_action(readback)){error="live native cash scope: "+readback+" original="+original;return action::Outcome::Unknown;}
        if(outcome==action::Outcome::Confirmed&&!live_state_->after_action(authority,readback)){error="live actual Day after-action receipt: "+readback+" original="+original;return action::Outcome::Unknown;}
        error=original;return outcome;
    }
    bool observe_action(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error)override {
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC)
            return npc_probe::action_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::SetPrices)return ui_price_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding) {
            if(!current(binding,request.snapshot_uuid,error))return false;
            const auto found=history_.find(request.action_uuid);
            if(found==history_.end()||found->second.request!=request||found->second.binding!=binding||
               !found->second.confirmed) {error="building history matching native receipt absent";return false;}
            const auto& operation=found->second;
            action::Ledger actual;
            if(!read_ledger(actual,error)||actual!=operation.after) {
                if(error.empty())error="building history actual ledger changed";return false;
            }
            const auto row=world_.rows().find(request.canonical_record_key);
            if(row==world_.rows().end()||row->second.state!=operation.object_after) {
                error="building history shared object state changed";return false;
            }
            if(request.action==action::Action::RedoBuilding) {
                remove_diagnostics::LocalTarget target;native::BuildingState native_state;
                if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
                   target.uid!=operation.uid||!scenery_kiss::read_action_prop(target.uid,native_state,error)||
                   world_state::building_state(native_state)!=operation.object_after)return false;
            }
            out={};out.before=operation.before;out.after=operation.after;
            out.object_before=operation.object_before;out.object_after=operation.object_after;
            out.undo_confirmed=true;out.target_retired=request.action==action::Action::UndoBuilding;
            out.retirement_confirmed=operation.retired;return true;
        }

        if((request.action==action::Action::PlaceNetworkNode||request.action==action::Action::PlaceNetworkPipe))return ui_network_runtime::observe(binding,request,out,error);
        if((request.action==action::Action::PlacePath||request.action==action::Action::RemovePath))return ui_path_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::TerrainStroke)return ui_terrain_runtime::observe(binding,request,out,error);
        if((request.action==action::Action::SetRegionPreset||request.action==action::Action::SetRegionStyles||request.action==action::Action::EditRegionProperty))return ui_region_runtime::observe(binding,request,out,error);
        if(request.action!=action::Action::PlaceBuilding&&request.action!=action::Action::MoveBuilding)return action::Engine::observe_action(binding,request,out,error);
        if(!current(binding,request.snapshot_uuid,error))return false;
        const auto found=buildings_.find(request.action_uuid);
        if(found==buildings_.end()||found->second.request!=request||found->second.binding!=binding||!found->second.receipt.value("confirmed",false)) {
            error="native building observation has no matching confirmed action";return false;
        }
        const auto& operation=found->second;
        remove_diagnostics::LocalTarget target;native::BuildingState actual;
        if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
           !scenery_kiss::read_action_prop(target.uid,actual,error)||world_state::building_state(actual)!=operation.state) {
            if(error.empty())error="native building observation differs from confirmed state";return false;
        }
        out={};out.before=operation.before;if(!read_ledger(out.after,error))return false;
        out.object_before=operation.object_before;out.object_after=operation.state;out.undo_confirmed=operation.receipt.value("undo_confirmed",false);
        if(request.action==action::Action::MoveBuilding) {
            if(target.uid!=operation.receipt.at("native_uid").get<uint64_t>()){error="native Move observation local UID changed";return false;}
            out.selection_confirmed=operation.receipt.value("selection_confirmed",false);
        }
        out.retirement_confirmed=operation.retired;return true;
    }
    bool observe_remove(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error)override {
        if(!current(binding,request.snapshot_uuid,error))return false;
        const auto found=operations_.find(request.action_uuid);
        if(found==operations_.end()||found->second.request!=request||found->second.binding!=binding) {
            error="typed native observation has no exact retained operation";return false;
        }
        const auto& operation=found->second;
        if(operation.removal.value("confirmed",false)!=true||!operation.removal.contains("result")) {
            error="typed native observation removal was not confirmed";return false;
        }
        const auto& result=operation.removal.at("result");
        if(result.value("unknown_locked",true)||!result.value("confirmed",false)||!result.value("attempted",false)||!result.value("returned",false)) {
            error="typed native observation removal is partial or Unknown";return false;
        }
        out={};out.before=operation.before;if(!read_ledger(out.after,error))return false;
        if(operation.object_before.alive) {
            out.object_before=operation.object_before;out.object_after=operation.object_before;out.object_after.alive=false;
        }
        out.target_retired=result.at("target_retired").get<bool>();out.undo_confirmed=result.at("undo_confirmed").get<bool>();
        out.selection_confirmed=result.at("selection_confirmed").get<bool>();
        if(!operation.retirement.empty()) {
            out.retirement_confirmed=operation.retirement.value("confirmed",false)&&
                operation.retirement.at("result").value("retirement_confirmed",false)&&
                operation.retirement.at("effects").value("confirmed",false)&&
                !operation.retirement.at("effects").value("unknown",true);
        }
        return true;
    }
    bool retire(const action::Binding& binding,const action::Request& request,std::string& error)override {
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC) {
            if(!current(binding,request.snapshot_uuid,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){
                error="NPC retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return npc_probe::action_runtime::retire(binding,request,error);
        }
        if(request.action==action::Action::SetPrices) {
            if(!current(binding,request.snapshot_uuid,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid) {
                error="price retirement has no exact Controller request";return false;
            }
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return ui_price_runtime::retire(binding,request,error);
        }
        if(request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding) {
            if(!current(binding,request.snapshot_uuid,error))return false;
            const auto found=history_.find(request.action_uuid);
            if(found==history_.end()||found->second.request!=request||found->second.binding!=binding||
               !found->second.confirmed||found->second.retired) {
                error="building history retirement lacks exact confirmed receipt";return false;
            }
            found->second.retired=true;return true;
        }

        if((request.action==action::Action::PlaceNetworkNode||request.action==action::Action::PlaceNetworkPipe)) {
            if(!current(binding,request.snapshot_uuid,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="network retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return ui_network_runtime::retire(binding,request,error);
        }
        if((request.action==action::Action::PlacePath||request.action==action::Action::RemovePath)) {
            if(!current(binding,request.snapshot_uuid,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="path retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return ui_path_runtime::retire(binding,request,error);
        }
        if(request.action==action::Action::TerrainStroke) {
            if(!current(binding,request.snapshot_uuid,error))return false;
            std::vector<std::string> keys;
            if(!ui_terrain_runtime::retired_keys(binding,request,keys,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="terrain retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,keys,error))return false;
            return ui_terrain_runtime::retire(binding,request,error);
        }
        if((request.action==action::Action::SetRegionPreset||request.action==action::Action::SetRegionStyles||request.action==action::Action::EditRegionProperty)) {
            if(!current(binding,request.snapshot_uuid,error))return false;
            std::vector<std::string> keys;
            if(!ui_region_runtime::retired_keys(binding,request,keys,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid) {
                error="region retirement has no exact Controller request";return false;
            }
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,keys,error))return false;
            return ui_region_runtime::retire(binding,request,error);
        }
        if(!current(binding,request.snapshot_uuid,error))return false;
        if(request.action==action::Action::MoveBuilding) {
            const auto found=buildings_.find(request.action_uuid);
            if(found==buildings_.end()||found->second.request!=request||found->second.binding!=binding||
               !found->second.receipt.value("confirmed",false)||!found->second.receipt.value("cleanup_confirmed",false)||
               found->second.receipt.value("exception",uint32_t(1))||found->second.receipt.value("cleanup_exception",uint32_t(1))) {
                error="native Move retirement requires measured inverse Undo and original cleanup";return false;
            }
            found->second.retired=true;return true;
        }
        if(request.action==action::Action::PlaceBuilding) {
            const auto found=buildings_.find(request.action_uuid);
            if(found==buildings_.end()||found->second.request!=request||found->second.binding!=binding||
               !found->second.receipt.value("confirmed",false)||!scenery_kiss::place_cleanup(found->second.receipt,found->second.normal_scenery,error)) {
                if(error.empty())error="native building bookkeeping requires confirmed original cleanup";return false;
            }
            found->second.retired=true;return true;
        }
        const auto found=operations_.find(request.action_uuid);
        if(found==operations_.end()||found->second.request!=request||found->second.binding!=binding) {
            error="typed native retirement has no exact retained operation";return false;
        }
        if(!found->second.retirement.empty()){error="typed native retirement already attempted; no replay";return false;}
        if(!remove_diagnostics::retire(json{{"token",request.action_uuid}},found->second.retirement,error))return false;
        if(found->second.object_before.alive) {
            auto after=found->second.object_before;after.alive=false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid) {
                error="shared removal commit has no current Controller request";return false;
            }
            world_state::Change change{world_state::Kind::Remove,controller.at("host_revision").get<uint64_t>(),
                request.action_uuid,request.canonical_record_key,found->second.object_before,after};
            world_state::LocalBinding observed{world_.origin(),request.canonical_record_key,found->second.local_uid,after};
            if(world_.commit(world_.origin(),change,observed,error)!=world_state::Admission::New)return false;
        }
        return true;
    }
};
struct Runtime {
    NativeEngine engine;
    action::SidecarTransport local_transport;action::SteamTransport steam_transport;
    action::Session& session;
    action::Transport& transport;
    action::Controller controller;
    uint32_t thread=GetCurrentThreadId();bool attempted=false,live_attempted=false,lobby_retiring=false;
    std::unique_ptr<live_host_state::NativePort> live_port;
    std::unique_ptr<live_host_state::Bridge> live_state;
    std::unique_ptr<live_host_state::day::economy::NativePort> economy_port;
    std::unique_ptr<live_host_state::day::economy::Coupling> economy_state;
    std::unique_ptr<live_host_state::day::NativePort> day_port;
    std::unique_ptr<live_host_state::day::Sync> day_state;
    std::string live_error;
    Runtime():session(action::selected_session()),
        transport(Config::instance().local_test?static_cast<action::Transport&>(local_transport):static_cast<action::Transport&>(steam_transport)),
        controller(session,engine,transport,[](const char* level,const std::string& operation,const std::string& cause) {
        if(std::string_view(level)=="error")LOG_ERROR("[Action] {}: {}",operation,cause);
        else if(std::string_view(level)=="warn")LOG_WARN("[Action] {}: {}",operation,cause);
        else LOG_INFO("[Action] {}: {}",operation,cause);
    }){}
};
std::atomic<Runtime*> published_runtime{nullptr};
}
bool start(std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime){runtime=new Runtime;published_runtime.store(runtime,std::memory_order_release);}
    if(runtime->thread!=GetCurrentThreadId()){error="typed action start requires bound Mainthread";return false;}
    if(runtime->lobby_retiring){error="typed lobby owner is retiring; no start";return false;}
    if(runtime->attempted){error="typed action start already attempted; no restart";return false;}
    runtime->attempted=true;
    if(!ui_price_runtime::enable(error)||!npc_probe::action_runtime::enable(error))return false;
    return runtime->controller.start(GetTickCount64(),error);
}
bool enable_live_state(std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||!runtime->controller.status().started||runtime->controller.status().locked||runtime->controller.status().pending||runtime->live_attempted) {error="live state requires idle actual bound Controller; no restart";return false;}
    runtime->live_attempted=true;action::Context context;
    struct KeepFailure {Runtime& runtime;std::string& cause;~KeepFailure(){if(!cause.empty())runtime.live_error=cause;}} keep{*runtime,error};
    if(!runtime->session.current(context,error))return false;if(context!=runtime->controller.bound_context()){error="live actual Session differs from Controller";return false;}
    const auto binding=runtime->controller.bound_binding();const auto epoch=net::current_world_epoch();uint64_t time=0;
    if(!epoch){error="live actual common Sidecar epoch absent";return false;}if(!live_host_state::read_time_authority(context,time,error))return false;
    auto& fence=live_host_state::economic_fence();
    if(!fence.arm(context,binding,[](const char* level,const std::string& cause){if(std::string_view(level)=="error")LOG_ERROR("[LiveState] {}",cause);else LOG_WARN("[LiveState] {}",cause);},error))return false;
    runtime->live_port=std::make_unique<live_host_state::NativePort>(context,binding,&fence);
    runtime->live_state=std::make_unique<live_host_state::Bridge>(context,binding,epoch,*runtime->live_port,runtime->transport,[context](uint64_t& revision,std::string& cause){return live_host_state::read_time_authority(context,revision,cause);});
    runtime->day_port=std::make_unique<live_host_state::day::NativePort>(context,binding,fence);runtime->day_state=std::make_unique<live_host_state::day::Sync>(*runtime->day_port,*runtime->live_port,context,binding);if(!runtime->day_state->bootstrap(error))return false;runtime->live_state->bind_day(*runtime->day_state);runtime->economy_port=std::make_unique<live_host_state::day::economy::NativePort>(*runtime->day_port,fence,live_host_state::day::Target{});runtime->economy_state=std::make_unique<live_host_state::day::economy::Coupling>(*runtime->day_state,*runtime->economy_port);runtime->live_state->bind_economy(*runtime->economy_state);if(context.role==action::Role::Host)live_host_state::day::bind_host_journal(runtime->day_state.get());runtime->engine.bind_live_state(*runtime->live_state,*runtime->live_port);return true;
}
bool live_state_active() {auto* runtime=published_runtime.load(std::memory_order_acquire);return runtime&&runtime->live_state!=nullptr;}
bool network_binding(const std::string& snapshot,action::Binding& out,std::string& error){
    out={};auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="network admission requires bound Controller Mainthread";return false;}
    const auto state=runtime->controller.status();
    if(!state.started||state.locked||state.pending||runtime->lobby_retiring){error="network admission requires idle unlocked Controller";return false;}
    action::Context context;if(!runtime->session.current(context,error))return false;
    if(context!=runtime->controller.bound_context()||context.snapshot_uuid!=snapshot){error="network admission actual Session/snapshot differs from Controller";return false;}
    const auto binding=runtime->controller.bound_binding();
    if(!runtime->engine.current(binding,snapshot,error))return false;
    out=binding;return true;
}
bool live_state_available(std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||!runtime->controller.status().started||runtime->controller.status().locked||runtime->controller.status().pending||runtime->live_attempted){error="live state actual Controller unavailable/pending/attempted";return false;}
    action::Context context;if(!runtime->session.current(context,error))return false;if(context!=runtime->controller.bound_context()){error="live actual Session differs from Controller";return false;}
    uint64_t time=0;if(!live_host_state::read_time_authority(context,time,error)||!live_host_state::economic_fence().can_arm(error))return false;
    live_host_state::NativePort reader(context,runtime->controller.bound_binding(),nullptr);live_host_state::Pin actual;return reader.read(actual,error);
}
void tick() {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->lobby_retiring||!runtime->controller.status().started)return;
    if(runtime->live_state&&!runtime->controller.status().locked) {std::string cause;const auto state=runtime->controller.status();auto& fence=live_host_state::economic_fence();const bool okay=!fence.locked()&&runtime->live_state->tick(state.host_revision,state.pending,cause);if(!okay){if(cause.empty())cause=fence.cause();if(runtime->live_error!=cause)LOG_ERROR("[LiveState] Apply failed: {}",cause);runtime->live_error=cause;}else {if(!runtime->live_error.empty())LOG_INFO("[LiveState] Apply recovered");runtime->live_error.clear();}}
    ui_move_runtime::tick();
    ui_region_runtime::tick();
    ui_price_runtime::tick();
    npc_probe::action_runtime::tick();
    ui_terrain_runtime::tick();
    ui_path_runtime::tick();
    ui_network_runtime::tick();
    std::string error;
    if(!runtime->controller.tick(GetTickCount64(),error)&&error.empty())LOG_ERROR("[Action] Tick failed without cause");
}
json controller_status() {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime)return {{"initialized",false}};
    if(runtime->thread!=GetCurrentThreadId())return {{"initialized",false},{"error","typed action status requires bound Mainthread"}};
    const auto s=runtime->controller.status();
    // Frame guards need Controller receipts, not a diagnostic rescan of the whole economy.
    return {{"initialized",true},{"lobby_retiring",runtime->lobby_retiring},{"started",s.started},{"locked",s.locked},
        {"pending",s.pending||runtime->lobby_retiring},{"host_revision",s.host_revision},{"action_uuid",s.action_uuid},
        {"outcome",int(s.outcome)},{"cause",s.cause}};
}
json status() {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime)return {{"initialized",false}};
    if(runtime->thread!=GetCurrentThreadId())return {{"initialized",false},{"error","typed action status requires bound Mainthread"}};
    const auto s=runtime->controller.status();
    const auto receipt=runtime->live_state?runtime->live_state->receipt():live_host_state::ApplyReceipt{};
    nlohmann::json native_economy=nlohmann::json::object();std::string economy_read_error;
    if(runtime->economy_state&&runtime->live_state&&runtime->live_state->last()){
        live_host_state::day::economy::State actual;std::vector<uint8_t> encoded;
        if(runtime->economy_state->read_current(*runtime->live_state->last(),actual,economy_read_error)&&live_host_state::day::economy::encode(actual,encoded,economy_read_error))native_economy=nlohmann::json::parse(encoded);
    }
    return {{"initialized",true},{"ui_price",ui_price_runtime::status()},{"ui_npc",npc_probe::action_runtime::status()},{"lobby_retiring",runtime->lobby_retiring},{"started",s.started},{"locked",s.locked},{"pending",s.pending||runtime->lobby_retiring},
        {"host_revision",s.host_revision},{"attempted",s.attempted},{"confirmed",s.confirmed},{"failed",s.failed},
        {"duplicates",s.duplicates},{"suppressed_errors",s.suppressed_errors},{"required_mask",s.required_mask},{"ack_mask",s.ack_mask},
        {"outcome",int(s.outcome)},{"action_uuid",s.action_uuid},{"cause",s.cause},{"last_packet_accepted",s.last_packet_accepted},
        {"subscriber_additions",runtime->economy_state?runtime->economy_state->addition_receipt():nlohmann::json(nullptr)},
        {"network_receipt",runtime->live_state?live_host_state::network::encode_receipt(runtime->live_state->network_receipt()):nlohmann::json::object()},
        {"network_activation_receipt",runtime->live_state?runtime->live_state->network_activation_receipt().status():nlohmann::json(nullptr)},
        {"network_frame_revision",runtime->live_state&&runtime->live_state->last()?runtime->live_state->last()->revision:0},
        {"network_target",runtime->live_state&&runtime->live_state->last()&&runtime->live_state->last()->network?live_host_state::network::encode(*runtime->live_state->last()->network):nlohmann::json(nullptr)},
        {"native_economy",native_economy},{"native_economy_read_error",economy_read_error},
        {"economy_receipt",runtime->economy_state?nlohmann::json{{"attempted",runtime->economy_state->receipt().attempted},{"confirmed",runtime->economy_state->receipt().confirmed},{"unknown",runtime->economy_state->receipt().unknown},{"attempted_stores",runtime->economy_state->receipt().attempted_stores},{"completed_stores",runtime->economy_state->receipt().stores},{"cause",runtime->economy_state->receipt().cause}}:nlohmann::json::object()},
        {"live_state",{{"requested",runtime->live_attempted},{"active",runtime->live_state!=nullptr},{"locked",(runtime->live_state&&runtime->live_state->locked())||live_host_state::economic_fence().locked()},
            {"day_record_sequence",runtime->day_state?runtime->day_state->sequence():0},{"day_receipt",runtime->day_state?nlohmann::json{{"attempted",runtime->day_state->day_receipt().attempted},{"confirmed",runtime->day_state->day_receipt().confirmed},{"unknown",runtime->day_state->day_receipt().unknown},{"created",runtime->day_state->day_receipt().created_records},{"appended",runtime->day_state->day_receipt().appended_records},{"pruned",runtime->day_state->day_receipt().pruned_records},{"cause",runtime->day_state->day_receipt().cause}}:nlohmann::json::object()},
            {"economic_fence_locked",live_host_state::economic_fence().locked()},{"economic_fence_cause",live_host_state::economic_fence().cause()},{"economic_suppressed",live_host_state::economic_fence().suppressed()},
            {"attempted",receipt.attempted},{"confirmed",receipt.confirmed},{"unknown",receipt.unknown},{"attempted_stores",receipt.attempted_stores},{"completed_stores",receipt.completed_stores},
            {"cause",runtime->live_error.empty()?receipt.cause:runtime->live_error}}}};
}
bool request_remove(const json& args,json& out,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!args.is_object()||args.size()!=2||!args.contains("action_uuid")||!args.contains("record_key")||
       !args.at("action_uuid").is_string()||!args.at("record_key").is_string()) {
        error="typed remove requires exact action_uuid and record_key";return false;
    }
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="typed action session unavailable on bound Mainthread";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; action not attempted";return false;}
    const bool accepted=runtime->controller.submit_remove(args.at("action_uuid").get<std::string>(),args.at("record_key").get<std::string>(),GetTickCount64(),error);
    out=status();return accepted;
}
bool submit_place(const std::string& id,const action::BuildingState& state,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="building action requires started game-thread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; action not attempted";return false;}
    return runtime->controller.submit_place(id,state,GetTickCount64(),error);
}
bool submit_move(const std::string& id,const std::string& key,const action::BuildingState& state,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Move requires started game-thread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; action not attempted";return false;}
    return runtime->controller.submit_move(id,key,state,GetTickCount64(),error);
}
bool submit_building_history(action::Action action,const std::string& id,const std::string& target,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="building history requires started game-thread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; building history not attempted";return false;}
    return runtime->controller.submit_building_history(action,id,target,GetTickCount64(),error);
}
bool submit_network_pipe(const std::string& id,const action::NetworkPipeIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="network Pipe requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Pipe not attempted";return false;}
    return runtime->controller.submit_network_pipe(id,intent,GetTickCount64(),error);
}
bool submit_network(const std::string& id,const action::NetworkIntent& intent,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="network requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; network not attempted";return false;}
    return runtime->controller.submit_network(id,intent,GetTickCount64(),error);
}
bool submit_path_delete(const std::string& id,const action::PathDeleteIntent& intent,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="path delete requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; path delete not attempted";return false;}
    return runtime->controller.submit_path_delete(id,intent,GetTickCount64(),error);
}
bool submit_path(const std::string& id,const action::PathIntent& intent,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="path requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; path not attempted";return false;}
    return runtime->controller.submit_path(id,intent,GetTickCount64(),error);
}
bool submit_terrain(const std::string& id,const action::TerrainIntent& intent,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="terrain requires started Mainthread Controller";return false;}
    return runtime->controller.submit_terrain(id,intent,GetTickCount64(),error);
}
bool submit_region(const std::string& id,const action::RegionIntent& intent,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="region action requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; action not attempted";return false;}
    return runtime->controller.submit_region(id,intent,GetTickCount64(),error);
}
bool submit_region_property(const std::string& id,const action::RegionPropertyIntent& intent,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="region property requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; region property not attempted";return false;}
    return runtime->controller.submit_region_property(id,intent,GetTickCount64(),error);
}
bool submit_prices(const std::string& id,const action::PriceIntent& intent,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="price action requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; price action not attempted";return false;}
    return runtime->controller.submit_prices(id,intent,GetTickCount64(),error);
}
bool submit_npc(const std::string& id,const action::NpcIntent& intent,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="NPC action requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; NPC action not attempted";return false;}
    return runtime->controller.submit_npc(id,intent,GetTickCount64(),error);
}
bool submit_remove_npc(const std::string& id,const std::string& target,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){
        error="NPC Remove requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; NPC Remove not attempted";return false;}
    return runtime->controller.submit_remove_npc(id,target,GetTickCount64(),error);
}
bool submit_move_npc(const std::string& id,const std::string& target,
                     const action::BuildingState& transform,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){
        error="NPC Move requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; NPC Move not attempted";return false;}
    return runtime->controller.submit_move_npc(id,target,transform,GetTickCount64(),error);
}
bool request_place(const json& args,json& out,std::string& error) {
    out=json::object();
    if(!args.is_object()||args.size()!=2||!args.contains("action_uuid")||!args.contains("building")||
       !args.at("action_uuid").is_string()||!args.at("building").is_object()) {
        error="building request requires action_uuid and full building state";return false;
    }
    const auto& value=args.at("building");
    if(value.size()!=8){error="building state requires type, variant, x, y, z, rotation, scale, alive";return false;}
    action::BuildingState state;
    state.type=value.at("type").get<std::string>();state.variant=value.at("variant").get<std::string>();
    state.x=value.at("x").get<float>();state.y=value.at("y").get<float>();state.z=value.at("z").get<float>();
    state.rotation=value.at("rotation").get<std::array<float,4>>();state.scale=value.at("scale").get<std::array<float,3>>();
    state.alive=value.at("alive").get<bool>();
    const bool accepted=submit_place(args.at("action_uuid").get<std::string>(),state,error);
    out=status();return accepted;
}
bool request_move(const json& args,json& out,std::string& error) {
    out=json::object();
    if(!args.is_object()||args.size()!=3||!args.contains("action_uuid")||!args.contains("record_key")||!args.contains("building")||
       !args.at("action_uuid").is_string()||!args.at("record_key").is_string()||!args.at("building").is_object()) {
        error="Move request requires action_uuid, record_key and full building state";return false;
    }
    const auto& value=args.at("building");
    if(value.size()!=8){error="Move building state requires all eight fields";return false;}
    action::BuildingState state;
    state.type=value.at("type").get<std::string>();state.variant=value.at("variant").get<std::string>();
    state.x=value.at("x").get<float>();state.y=value.at("y").get<float>();state.z=value.at("z").get<float>();
    state.rotation=value.at("rotation").get<std::array<float,4>>();state.scale=value.at("scale").get<std::array<float,3>>();
    state.alive=value.at("alive").get<bool>();
    const bool accepted=submit_move(args.at("action_uuid").get<std::string>(),args.at("record_key").get<std::string>(),state,error);
    out=status();return accepted;
}
bool authorize_ui_replay(const action_session::ui_capture::Replay& replay,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="typed UI replay requires actual bound Mainthread runtime";return false;}
    action::Context context;
    if(!runtime->session.current(context,error))return false;
    if(context!=runtime->controller.bound_context()) {
        error="typed UI replay actual session differs from bound Controller context";return false;
    }
    return runtime->engine.authorize_ui_replay(replay,context,runtime->controller.status(),error);
}
}

namespace mt2mp::game::typed_action_runtime {
bool begin_lobby_leave(std::string& e){
    const auto current=status();
    const auto build=ui_build_runtime::status(),remove=ui_remove_runtime::status(),move=ui_move_runtime::status(),region=ui_region_runtime::status(),price=ui_price_runtime::status();
    if(build.value("deferred_build_capture",false)||remove.value("replay_active",false)||remove.value("inflight",uint64_t{})||move.value("pending_ui",false)||region.value("pending_ui",false)||price.value("pending_ui",false)||npc_probe::action_runtime::pending_ui()){e="native lobby Leave blocked by retained active/pending UI capture intent";return false;}
    if(current.value("snapshot_handoff",false)||!current.value("snapshot_error",std::string{}).empty()){e="native lobby Leave blocked by current snapshot handoff: "+current.value("snapshot_error",std::string{});return false;}
    auto* r=published_runtime.load(std::memory_order_acquire);
    if(r){const auto s=r->controller.status();if(r->thread!=GetCurrentThreadId()||!r->controller.can_retire_lobby()||r->lobby_retiring){e="lobby Leave requires idle actual Controller; pending/Unknown/retiring cause="+s.cause;return false;}
        r->lobby_retiring=true;net::session_wire::suspend_actions();
        if(r->live_state&&!r->live_state->suspend_snapshot(e)){r->live_error=e;return false;}
        if(r->live_port&&!live_host_state::economic_fence().suspend_snapshot(e)){r->live_error=e;return false;}
    }else net::session_wire::suspend_actions();
    time_integration::Status time;
    if(time_integration::instance().status(time,e)){
        if(!time_integration::instance().suspend("native lobby Leave",e))return false;
    }else if(e!="time_integration_not_initialized")return false;
    net::invalidate_snapshot_world("native lobby Leave");e.clear();return true;
}
bool finish_lobby_leave(bool& ready,std::string& e){
    ready=false;native::WorldStatus actual;if(!native::read_world_status(actual,e))return false;
    if(actual.active_mode_index!=2||actual.title_transition_pending||actual.saves_in_progress>0){e.clear();return true;}
    auto* r=published_runtime.load(std::memory_order_acquire);
    if(r&&(r->thread!=GetCurrentThreadId()||!r->lobby_retiring||!r->controller.can_retire_lobby())){e="lobby owner retirement not idle or Unknown";return false;}
    time_integration::Status time;
    if(time_integration::instance().status(time,e)){
        if(time.runtime.phase!=time_runtime::Phase::Waiting||time.runtime.epoch){e.clear();return true;}
    }else if(e!="time_integration_not_initialized")return false;
    if(!live_host_state::retire_lobby_fence(e))return false;
    if(!native::retire_software_replica(e))return false;
    net::retire_software_sync();
    // Retain callback storage until the native World and its bound Time owner have ended.
    net::SteamMatchmaking::instance().leave_lobby();
    if(net::SteamMatchmaking::instance().in_lobby()){e="native lobby Leave local membership readback still present";return false;}
    if(!net::session_wire::retire_lobby(e))return false;
    tutorial_replication::retire("confirmed native Title after lobby Leave");
    live_host_state::day::bind_host_journal(nullptr);published_runtime.store(nullptr,std::memory_order_release);delete r;
    ready=true;e.clear();return true;
}
}

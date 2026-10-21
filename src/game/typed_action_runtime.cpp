#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "zone_edit_runtime.h"
#include "district_stroke_runtime.h"
#include "mt2mp/net/chat_runtime.h"
#include "marketing_authority/native_port.h"
#include "bug_authority/runtime.h"
#include "bug_authority/manual_action.h"
#include "bug_authority/codec.h"
#include "typed_action_runtime.h"
#include "intervention_actor_live.h"
#include "scenery_stroke_runtime.h"
#include "building_clearance_runtime.h"
#include "modular_runtime.h"
#include "toon_weapon_visual/authority.h"
#include "toon_weapon_visual/probe.h"
#include "toon_time_fence/native.h"
#include "scenario_vip_authority/native.h"
#include "scenario_vip_authority/host_witness.h"
#include "scenario_vip_authority/frame_adapter.h"
#include "scenario_vip_authority/alert_cleanup_hook.h"
#include "time_integration.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/session_wire.h"
#include "live_host_state/bridge.h"
#include "live_host_state/apply_timing.h"
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
#include "scenery_scene_identity.h"
#include "scenery_kiss/native_catalog.h"
#include "scenery_kiss/lifecycle_contract.h"
#include "native_open_runtime.h"
#include "tutorial_replication/native.h"
#include "tutorial_replication/start_ledger.h"
#include "tutorial_replication/staff_action.h"
#include "ui_remove_runtime.h"
#include "ui_build_runtime.h"
#include "building_history_slice/native_bridge.h"
#include "building_history_slice/cash_effect.h"
#include "ui_move_runtime.h"
#include "prop_text_runtime.h"
#include "district_text_runtime.h"
#include "district_title_runtime.h"
#include "district_edit_runtime.h"
#include "group_runtime.h"
#include "group_object_runtime.h"
#include "boss_placement_runtime.h"
#include "boss_phase_runtime.h"
#include "service_membership_runtime.h"
#include "selection_move_runtime.h"
#include "duplicate_runtime.h"
#include "ui_region_runtime.h"
#include "region_history_key.h"
#include "quest_history_runtime.h"
#include "ui_price_runtime.h"
#include "management_runtime.h"
#include "network_remove_runtime.h"
#include "network_history_runtime.h"
#include "logo_runtime.h"
#include "general_project_runtime.h"
#include "release_runtime.h"
#include "party_frame_native.h"
#include "dungeon_instance_native.h"
#include "management_live_port.h"
#include "release_live_port.h"
#include "developer_staff/action_native.h"
#include "developer_staff/construction_native.h"
#include "npc_probe.h"
#include "monster_zone_action_runtime.h"
#include "monster_action_runtime.h"
#include "npc_quest_authoring_runtime.h"
#include "npc_quest_list_runtime.h"
#include "character_rename_runtime.h"
#include "character_membership_runtime.h"
#include "ability_edit_runtime.h"
#include "ability_membership_runtime.h"
#include "costume_edit_runtime.h"
#include "dungeon_building_runtime.h"
#include "dungeon_building_graph.h"
#include "dungeon_config_runtime.h"
#include "dungeon_sound_runtime.h"
#include "dungeon_scene_identity_runtime.h"
#include "dungeon_palette_runtime.h"
#include "dungeon_input_runtime.h"
#include "dungeon_gizmo_runtime.h"
#include "dungeon_light_runtime.h"
#include "building_status_runtime.h"
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
    struct Operation {action::Request request;action::Binding binding;action::Ledger before;json removal,retirement;action::BuildingState object_before;uint64_t local_uid=0;bool normal_scenery=false,history_ready=false;int32_t refund=0;bool scene_scenery=false;};
    std::map<std::string,Operation> operations_;
    struct BuildOperation {action::Request request;action::Binding binding;action::Ledger before;action::BuildingState state;json receipt;bool retired=false;action::BuildingState object_before;bool normal_scenery=false;developer_staff::construction::LocalJob construction_job;bool construction_confirmed=false;
        ui_move_runtime::building_clearance::Snapshot clearance_before,clearance_after;};
    std::map<std::string,BuildOperation> buildings_;
    struct HistoryOperation {action::Request request;action::Binding binding;action::Ledger before,after;
        action::BuildingState object_before,object_after;uint64_t uid=0;bool confirmed=false,retired=false;bool scene_scenery=false;
        ui_move_runtime::building_clearance::Snapshot clearance_before,clearance_after;};
    std::map<std::string,HistoryOperation> history_;
    struct CommitOperation {action::Request request;action::Binding binding;action::Evidence evidence;bool confirmed=false;};
    std::map<std::string,CommitOperation> commits_;
    static bool research_action(action::Action value){
        return value==action::Action::StartResearch||value==action::Action::SetResearchWorkers||value==action::Action::CommitResearch;
    }
    action::Outcome perform_research(const action::Binding& binding,const action::Request& request,const action::Evidence* host,std::string& error){
        if(!research_action(request.action)||request.canonical_record_key!="tech:bridges"||!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(commits_.contains(request.action_uuid)){error="Research action already attempted; no replay";return action::Outcome::Unknown;}
        if(commits_.size()>=action::history_limit){error="Research retained action receipt limit reached";return action::Outcome::RejectedNoEffect;}
        action::Context context;if(!action::selected_session().current(context,error))return action::Outcome::RejectedNoEffect;
        if((context.role==action::Role::Client)!=(host!=nullptr)){error="Research action role/authority differs";return action::Outcome::RejectedNoEffect;}
        action::Evidence evidence;tutorial_replication::research::Row before;
        if(!read_ledger(evidence.before,error)||!tutorial_replication::read_research_row(context,binding,"bridges",before,error))return action::Outcome::RejectedNoEffect;
        evidence.research_before=before;
        if(host&&(host->before!=evidence.before||!tutorial_replication::research::same_state(host->research_before,evidence.research_before)||!host->research_after)){
            error="Research action differs from exact Host BEFORE";return action::Outcome::RejectedNoEffect;}
        bug_authority::State bugs_before,bugs_after;
        if(request.action==action::Action::CommitResearch&&!host){
            if(!live_state_){error="Research Commit Host live publication owner absent";return action::Outcome::RejectedNoEffect;}
            if(!bug_authority::capture(context,binding,live_state_->next_publication_revision(),bugs_before,error))return action::Outcome::RejectedNoEffect;
        }
        auto& receipt=commits_.emplace(request.action_uuid,CommitOperation{request,binding,evidence}).first->second;
        bool attempted=false,unknown=false,applied=false;
        if(request.action==action::Action::StartResearch){
            action::Ledger expected;tutorial_replication::research::Row target;
            applied=host?tutorial_replication::apply_research_start(context,binding,request.action_uuid,evidence.before,host->after,before,*host->research_after,attempted,unknown,error):
                tutorial_replication::start_research(context,binding,evidence.before,before,expected,target,attempted,error);
        }else if(request.action==action::Action::SetResearchWorkers){
            applied=tutorial_replication::set_research_workers(context,binding,before,request.desired_workers,attempted,error);
        }else{
            applied=host?tutorial_replication::apply_research_commit(context,binding,before,*host->research_after,attempted,unknown,error):
                tutorial_replication::commit_research(context,binding,before,attempted,error);
        }
        if(!applied)return attempted||unknown?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
        tutorial_replication::research::Row after;
        if(!read_ledger(receipt.evidence.after,error)||!tutorial_replication::read_research_row(context,binding,"bridges",after,error))return action::Outcome::Unknown;
        receipt.evidence.research_after=after;
        if(host&&(host->after!=receipt.evidence.after||!tutorial_replication::research::same_state(host->research_after,receipt.evidence.research_after))){
            error="Research Client actual result differs from Host";return action::Outcome::Unknown;}
        if(request.action==action::Action::StartResearch){
            if(int64_t(evidence.before.cash)-receipt.evidence.after.cash!=10000||
               !tutorial_replication::start_intent::payment(evidence.before,receipt.evidence.after,error)||
               !tutorial_replication::start_intent::rows(before.value,after.value,error)){
                if(error.empty())error="Research original Start price/ledger/project result differs";return action::Outcome::Unknown;}
        }else if(request.action==action::Action::SetResearchWorkers){
            if(evidence.before!=receipt.evidence.after||!tutorial_replication::staff::rows(before.value,after.value,request.desired_workers,error)){
                if(error.empty())error="Research Workers ledger/project result differs";return action::Outcome::Unknown;}
        }else{
            tutorial_replication::research::Plan operation=tutorial_replication::research::Plan::None;
            if(evidence.before!=receipt.evidence.after||!tutorial_replication::research::plan(before,after,operation,error)||operation!=tutorial_replication::research::Plan::Deploy){
                if(error.empty())error="Research Commit ledger/deployment result differs";return action::Outcome::Unknown;}
            if(!host&&!bug_authority::capture(context,binding,live_state_->next_publication_revision(),bugs_after,error))return action::Outcome::Unknown;
            if(!tutorial_replication::retain_research_commit_effect(context,binding,request.action_uuid,
               host?nullptr:&bugs_before,host?nullptr:&bugs_after,error))return action::Outcome::Unknown;
        }
        receipt.confirmed=true;return action::Outcome::Confirmed;
    }
    action::Outcome perform_staff(const action::Binding& binding,const action::Request& request,const action::Evidence* host,std::string& error){
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(commits_.contains(request.action_uuid)){error="startingpoint staff action already attempted; no replay";return action::Outcome::Unknown;}
        if(commits_.size()>=action::history_limit){error="startingpoint retained action receipt limit reached";return action::Outcome::RejectedNoEffect;}
        action::Context context;if(!action::selected_session().current(context,error))return action::Outcome::RejectedNoEffect;
        if((context.role==action::Role::Client)!=(host!=nullptr)){error="startingpoint staff action role/authority differs";return action::Outcome::RejectedNoEffect;}
        action::Evidence evidence;tutorial_replication::startingpoint::Row before;
        if(!read_ledger(evidence.before,error)||!tutorial_replication::read_startingpoint_row(context,binding,before,error))return action::Outcome::RejectedNoEffect;
        evidence.startingpoint_before=before;
        if(host&&(host->before!=evidence.before||host->startingpoint_before!=evidence.startingpoint_before||!host->startingpoint_after)){
            error="startingpoint native action differs from Host before receipt";return action::Outcome::RejectedNoEffect;}
        auto& receipt=commits_.emplace(request.action_uuid,CommitOperation{request,binding,evidence}).first->second;
        bool attempted=false,unknown=false;
        const bool applied=tutorial_replication::set_startingpoint_workers(context,binding,before,request.desired_workers,attempted,error);
        if(!applied)return attempted||unknown?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
        tutorial_replication::startingpoint::Row after;
        if(!read_ledger(receipt.evidence.after,error)||!tutorial_replication::read_startingpoint_row(context,binding,after,error))return action::Outcome::Unknown;
        receipt.evidence.startingpoint_after=after;
        if(receipt.evidence.before!=receipt.evidence.after||
           !tutorial_replication::staff::rows(before,after,request.desired_workers,error)){
            if(error.empty())error="startingpoint native action result/ledger differs";return action::Outcome::Unknown;}
        if(host&&(host->after!=receipt.evidence.after||host->startingpoint_after!=receipt.evidence.startingpoint_after)){
            error="startingpoint Client native result differs from Host";return action::Outcome::Unknown;}
        receipt.confirmed=true;return action::Outcome::Confirmed;
    }
    action::Outcome perform_commit(const action::Binding& binding,const action::Request& request,const action::Evidence* host,std::string& error){
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(commits_.contains(request.action_uuid)){error="startingpoint action already attempted; no replay";return action::Outcome::Unknown;}
        if(commits_.size()>=action::history_limit){error="startingpoint retained action receipt limit reached";return action::Outcome::RejectedNoEffect;}
        action::Context context;if(!action::selected_session().current(context,error))return action::Outcome::RejectedNoEffect;
        if((context.role==action::Role::Client)!=(host!=nullptr)){error="startingpoint action role/authority differs";return action::Outcome::RejectedNoEffect;}
        action::Evidence evidence;tutorial_replication::startingpoint::Row before;
        if(!read_ledger(evidence.before,error)||!tutorial_replication::read_startingpoint_row(context,binding,before,error))return action::Outcome::RejectedNoEffect;
        evidence.startingpoint_before=before;
        if(host&&(host->before!=evidence.before||host->startingpoint_before!=evidence.startingpoint_before||!host->startingpoint_after)){
            error="startingpoint native action differs from Host before receipt";return action::Outcome::RejectedNoEffect;}
        auto& receipt=commits_.emplace(request.action_uuid,CommitOperation{request,binding,evidence}).first->second;
        bool attempted=false,unknown=false;
        const bool applied=host?
            tutorial_replication::apply_startingpoint_commit(context,binding,before,*host->startingpoint_after,attempted,unknown,error):
            tutorial_replication::commit_startingpoint(context,binding,before,attempted,error);
        if(!applied)return attempted||unknown?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
        tutorial_replication::startingpoint::Row after;
        if(!read_ledger(receipt.evidence.after,error)||!tutorial_replication::read_startingpoint_row(context,binding,after,error))return action::Outcome::Unknown;
        receipt.evidence.startingpoint_after=after;
        tutorial_replication::startingpoint::Plan operation;
        if(receipt.evidence.before!=receipt.evidence.after||
           !tutorial_replication::startingpoint::plan(before,after,operation,error)||operation!=tutorial_replication::startingpoint::Plan::Deploy){
            if(error.empty())error="startingpoint native action result/ledger differs";return action::Outcome::Unknown;}
        if(host&&(host->after!=receipt.evidence.after||host->startingpoint_after!=receipt.evidence.startingpoint_after)){
            error="startingpoint Client native result differs from Host";return action::Outcome::Unknown;}
        receipt.confirmed=true;return action::Outcome::Confirmed;
    }
    action::Outcome perform_start(const action::Binding& binding,const action::Request& request,const action::Evidence* host,std::string& error){
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(commits_.contains(request.action_uuid)){error="startingpoint Start already attempted; no replay";return action::Outcome::Unknown;}
        if(commits_.size()>=action::history_limit){error="startingpoint retained action receipt limit reached";return action::Outcome::RejectedNoEffect;}
        action::Context context;if(!action::selected_session().current(context,error))return action::Outcome::RejectedNoEffect;
        if((context.role==action::Role::Client)!=(host!=nullptr)){error="startingpoint Start role/authority differs";return action::Outcome::RejectedNoEffect;}
        action::Evidence evidence;tutorial_replication::startingpoint::Row before;
        if(!read_ledger(evidence.before,error)||!tutorial_replication::read_startingpoint_row(context,binding,before,error))return action::Outcome::RejectedNoEffect;
        evidence.startingpoint_before=before;
        if(host&&(host->before!=evidence.before||host->startingpoint_before!=evidence.startingpoint_before||!host->startingpoint_after)){
            error="startingpoint Start differs from exact Host BEFORE";return action::Outcome::RejectedNoEffect;}
        auto& receipt=commits_.emplace(request.action_uuid,CommitOperation{request,binding,evidence}).first->second;
        bool attempted=false,unknown=false;action::Ledger expected;tutorial_replication::startingpoint::Row target;
        const bool applied=host?
            tutorial_replication::apply_startingpoint_start(context,binding,request.action_uuid,evidence.before,host->after,before,*host->startingpoint_after,attempted,unknown,error):
            tutorial_replication::start_startingpoint(context,binding,evidence.before,before,expected,target,attempted,error);
        if(!applied)return attempted||unknown?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
        tutorial_replication::startingpoint::Row after;
        if(!read_ledger(receipt.evidence.after,error)||!tutorial_replication::read_startingpoint_row(context,binding,after,error))return action::Outcome::Unknown;
        receipt.evidence.startingpoint_after=after;
        if(!tutorial_replication::start_intent::rows(before,after,error)||
           !tutorial_replication::start_intent::payment(evidence.before,receipt.evidence.after,error))return action::Outcome::Unknown;
        if(host&&(host->after!=receipt.evidence.after||host->startingpoint_after!=receipt.evidence.startingpoint_after)){
            error="startingpoint Start Client actual result differs from Host";return action::Outcome::Unknown;}
        receipt.confirmed=true;return action::Outcome::Confirmed;
    }
    bool observe_commit(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
        if(!current(binding,request.snapshot_uuid,error))return false;
        const auto found=commits_.find(request.action_uuid);
        if(found==commits_.end()||found->second.request!=request||found->second.binding!=binding||!found->second.confirmed){
            error="startingpoint exact confirmed native action receipt absent";return false;}
        if(research_action(request.action)){
            action::Context context;action::Ledger ledger;tutorial_replication::research::Row row;
            if(!action::selected_session().current(context,error)||!read_ledger(ledger,error)||
               !tutorial_replication::read_research_row(context,binding,"bridges",row,error))return false;
            if(ledger!=found->second.evidence.after||!tutorial_replication::research::same_state(row,found->second.evidence.research_after)){
                error="Research observation differs from retained result";return false;}
            out=found->second.evidence;return true;
        }
        action::Context context;action::Ledger ledger;tutorial_replication::startingpoint::Row row;
        if(!action::selected_session().current(context,error)||!read_ledger(ledger,error)||
           !tutorial_replication::read_startingpoint_row(context,binding,row,error))return false;
        if(ledger!=found->second.evidence.after||std::optional(row)!=found->second.evidence.startingpoint_after){
            error="startingpoint native action observation differs from retained result";return false;}
        out=found->second.evidence;return true;
    }
    world_state::Table world_;
    uint32_t thread_=0;
    live_host_state::Bridge* live_state_=nullptr;
    live_host_state::NativePort* live_port_=nullptr;
    std::optional<action::Message> live_result_;
    const action_session::ui_capture::Replay* active_ui_replay_=nullptr;
    const action::Evidence* active_host_evidence_=nullptr;
    const action::Request* active_dungeon_building_=nullptr;
    struct DungeonOperation {
        action::Request request;action::Binding binding;
        action::Ledger ledger_before,ledger_after;
        npc_probe::dungeon_building::Snapshot before,after;
        action::DungeonBuildingEffect effect;
        npc_probe::dungeon_building::Snapshot inverse_sample;
        action::BuildingState inverse_destination;
        uintptr_t inverse_graph=0;
        bool inverse_destroying=false,inverse_moving=false;
        bool confirmed=false,retired=false,removal_finalized=false;
    };
    std::map<std::string,std::unique_ptr<DungeonOperation>> dungeon_buildings_;
    bool dungeon_scope(const action::Request& request,std::string& error){
        if(!request.dungeon_building)return true;
        if(!active_dungeon_building_||*active_dungeon_building_!=request){
            error="Dungeon Building original requires its exact prepared native scope";return false;
        }
        return true;
    }
    action::Outcome perform_remove(const action::Binding& binding,const action::Request& request,
        action_session::ui_capture::ReplayPurpose purpose,const action::Evidence* authority,std::string& error) {
        if(!dungeon_scope(request,error))return action::Outcome::RejectedNoEffect;
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(operations_.contains(request.action_uuid)){error="typed native action token already attempted; no replay";return action::Outcome::Unknown;}
        action::Ledger before;if(!read_ledger(before,error))return action::Outcome::RejectedNoEffect;
        action::BuildingState object_before;uint64_t local_uid=0;bool normal_scenery=false,scene_scenery=false;
        {
            remove_diagnostics::LocalTarget target;native::BuildingState actual;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
               !scenery_kiss::read_action_prop(target.uid,actual,error))return action::Outcome::RejectedNoEffect;
            object_before=world_state::building_state(actual);local_uid=target.uid;normal_scenery=target.normal_scenery;
            scene_scenery=normal_scenery&&remove_diagnostics::scene_scenery::registered(request.snapshot_uuid,request.canonical_record_key,target.uid);
            world_state::LocalBinding settled;
            if(request.dungeon_building&&!remove_diagnostics::is_dungeon_building_target(target,request.dungeon_scene,error))return action::Outcome::RejectedNoEffect;
            if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,settled,error)||
               (!request.dungeon_building&&!scene_scenery&&!world_.refresh_current(world_.origin(),request.canonical_record_key,settled,error)))return action::Outcome::RejectedNoEffect;
            const auto row=world_.rows().find(request.canonical_record_key);
            if(!request.dungeon_building&&!scene_scenery&&(row==world_.rows().end()||row->second.state!=object_before)) {
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
        operation.normal_scenery=normal_scenery;operation.scene_scenery=scene_scenery;
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
    bool dungeon_history_scene(const std::string& id,const std::string& key,std::string& scene,bool& matched,std::string& error){
        scene.clear();matched=false;const action::Request* request=nullptr;
        const auto placed=buildings_.find(id);const auto removed=operations_.find(id);
        if(placed!=buildings_.end())request=&placed->second.request;
        if(removed!=operations_.end()){
            if(request){error="Dungeon Building history source is ambiguous";return false;}
            request=&removed->second.request;
        }
        if(!request||!request->dungeon_building)return true;
        matched=true;
        if(request->canonical_record_key!=key||!action::valid_dungeon_building_scene(request->dungeon_scene)){
            error="Dungeon Building history source key/scene differs";return false;
        }
        scene=request->dungeon_scene;error.clear();return true;
    }
    bool bug_parent_alive(const action::Binding& binding,const std::string& snapshot,const std::string& key,
                          bool& known,bool& alive,std::string& error){
        known=alive=false;if(!current(binding,snapshot,error))return false;
        bug_authority::Row parent;parent.parent_kind=bug_authority::ParentKind::Building;parent.parent_key=key;
        if(!bug_authority::codec_detail::parent(parent)){error="Bug parent requires canonical Building key";return false;}
        const auto row=world_.rows().find(key);
        if(row!=world_.rows().end()){known=true;alive=row->second.state.alive;}
        error.clear();return true;
    }
    action::Binding loaded_construction_binding_{};
    std::string loaded_construction_snapshot_;
    std::vector<developer_staff::construction::LocalJob> loaded_construction_;
    struct DuplicateConstruction {
        action::Request request;action::Binding binding;
        std::vector<developer_staff::construction::LocalJob> jobs;
        bool confirmed=false,restored=false;
    };
    std::map<std::string,DuplicateConstruction> duplicate_construction_;
    void retire_absent_construction(const developer_staff::construction::PlacementBefore& before){
        const auto retire=[&](developer_staff::construction::LocalJob& job){
            if(job.retired)return;
            job.retired=std::none_of(before.queue.jobs.begin(),before.queue.jobs.end(),
                [&](const auto& current_job){return current_job.pointer==job.job;});
        };
        for(auto& job:loaded_construction_)retire(job);
        for(auto& [id,op]:buildings_){(void)id;if(op.construction_confirmed&&op.construction_job.job)retire(op.construction_job);}
        for(auto& [id,op]:duplicate_construction_){(void)id;if(op.confirmed)for(auto& job:op.jobs)retire(job);}
    }
    bool construction_jobs(const action::Binding& binding,const std::string& snapshot,
                           std::vector<developer_staff::construction::LocalJob>& out,std::string& error){
        out.clear();if(!current(binding,snapshot,error))return false;
        std::vector<developer_staff::construction::LocalJob> result;
        std::set<std::string> keys;
        const auto append=[&](developer_staff::construction::LocalJob job,const std::string& id,bool duplicate_restored=false){
            if(!keys.insert(job.building_key).second){error="Construction canonical Building occurrence is duplicated";return false;}
            const auto row=world_.rows().find(job.building_key);
            if(row==world_.rows().end()){error="Construction local Job has no current ordered Building row";return false;}
            job.target_alive=row->second.state.alive;
            job.target_restored=job.target_alive&&duplicate_restored;
            if(job.target_alive)for(const auto& [history_id,history]:history_){
                const auto removed=operations_.find(history.request.target_action_uuid);
                const bool original_place=!id.empty()&&history.request.action==action::Action::RedoBuilding&&history.request.target_action_uuid==id;
                const bool original_remove=history.request.action==action::Action::UndoBuilding&&removed!=operations_.end()&&
                    removed->second.history_ready&&removed->second.request.action_uuid==history.request.target_action_uuid&&
                    removed->second.binding==binding&&removed->second.local_uid==job.building_uid&&
                    removed->second.request.action==action::Action::RemoveSnapshotBuilding&&
                    removed->second.request.snapshot_uuid==snapshot&&removed->second.request.canonical_record_key==job.building_key;
                if(history.confirmed&&!history.object_before.alive&&(original_place||original_remove)&&
                   history.request.action_uuid==history_id&&
                   history.binding==binding&&history.request.snapshot_uuid==snapshot&&
                   history.request.canonical_record_key==job.building_key&&history.uid==job.building_uid&&history.object_after.alive){
                    job.target_restored=true;break;
                }
            }
            result.push_back(std::move(job));return true;
        };
        if(!loaded_construction_.empty()){
            if(loaded_construction_binding_!=binding||loaded_construction_snapshot_!=snapshot){
                error="Loaded construction occurrences belong to another snapshot/native binding";return false;}
            for(const auto& job:loaded_construction_)if(!append(job,{}))return false;
        }
        for(const auto& [id,op]:buildings_){
            if(!op.construction_job.job)continue;
            if(op.binding!=binding||op.request.snapshot_uuid!=snapshot||op.request.action!=action::Action::PlaceBuilding||
               op.request.action_uuid!=id||op.request.canonical_record_key!=op.construction_job.building_key||!op.construction_confirmed){
                error="Construction local Job does not belong to confirmed current PlaceBuilding";return false;}
            if(!append(op.construction_job,id))return false;
        }
        for(const auto& [id,op]:duplicate_construction_){
            if(op.jobs.empty())continue;
            if(!op.confirmed||op.binding!=binding||op.request.snapshot_uuid!=snapshot||
               op.request.action!=action::Action::DuplicateSelection||op.request.action_uuid!=id){
                error="Construction local Job does not belong to confirmed current DuplicateSelection";return false;}
            for(const auto& job:op.jobs)if(!append(job,{},op.restored))return false;
        }
        out=std::move(result);error.clear();return true;
    }
    action::Outcome perform_place(const action::Binding& binding,const action::Request& request,
                                 const action::Evidence* authority,std::string& error) {
        if(!dungeon_scope(request,error))return action::Outcome::RejectedNoEffect;
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
        if(!request.dungeon_building&&world_.prepare(world_.origin(),change,error)!=world_state::Admission::New)return action::Outcome::RejectedNoEffect;
        developer_staff::construction::PlacementBefore construction_before;
        if(!developer_staff::construction::capture_placement(construction_before,error))return action::Outcome::RejectedNoEffect;
        retire_absent_construction(construction_before);
        ui_move_runtime::building_clearance::Snapshot clearance_before;
        if(!request.dungeon_building&&!ui_move_runtime::building_clearance::capture(binding,request,clearance_before,error,authority))return action::Outcome::RejectedNoEffect;
        if(clearance_before.local){std::vector<world_state::LocalBinding> current_scenery;
            if(!ui_move_runtime::building_clearance::current_bindings(clearance_before,world_.origin(),current_scenery,error))return action::Outcome::RejectedNoEffect;
            for(const auto& row:current_scenery)if(!world_.refresh_current(world_.origin(),row.object_key,row,error))return action::Outcome::RejectedNoEffect;}
        auto& operation=buildings_.emplace(request.action_uuid,BuildOperation{request,binding,before,{},{}}).first->second;
        operation.clearance_before=std::move(clearance_before);
        if(!ui_build_runtime::place(request,operation.state,operation.receipt,error))
            return operation.receipt.value("attempted",false)?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
        if(!building_history_slice::remember_place(request.action_uuid,
                operation.receipt.at("local_undo_graph").get<uint64_t>(),error))return action::Outcome::Unknown;
        const auto uid=operation.receipt.at("native_uid").get<uint64_t>();
        if(operation.clearance_before.local&&!ui_move_runtime::building_clearance::settle(binding,request,operation.clearance_before,nullptr,uid,
            operation.receipt.at("local_undo_graph").get<uint64_t>(),operation.clearance_after,error))return action::Outcome::Unknown;
        if(!(request.dungeon_building?
            remove_diagnostics::admit_created_dungeon_building(request.snapshot_uuid,request.canonical_record_key,uid,request.dungeon_scene,error):
            remove_diagnostics::admit_created_building(request.snapshot_uuid,request.canonical_record_key,uid,error)))return action::Outcome::Unknown;
        remove_diagnostics::LocalTarget target;native::BuildingState actual;
        if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
           !scenery_kiss::read_action_prop(uid,actual,error))return action::Outcome::Unknown;
        if(!developer_staff::construction::finish_placement(construction_before,request.snapshot_uuid,
                request.canonical_record_key,uid,operation.construction_job,error))return action::Outcome::Unknown;
        operation.normal_scenery=target.normal_scenery;
        world_state::LocalBinding observed;
        if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error))return action::Outcome::Unknown;
        if(observed.state!=operation.state){error="native building state changed before ID binding";return action::Outcome::Unknown;}
        if(authority&&operation.state!=authority->object_after){error="native client building differs from Host result";return action::Outcome::Unknown;}
        change.after=operation.state;
        if(operation.clearance_after.local){
            std::vector<std::string> retired;std::vector<world_state::LocalBinding> created;
            if(!ui_move_runtime::building_clearance::finish(binding,request,operation.clearance_before,operation.clearance_after,authority,error)||
               !ui_move_runtime::building_clearance::publication(operation.clearance_after,world_.origin(),retired,created,error))return action::Outcome::Unknown;
            created.push_back(observed);
            if(world_.commit_scenery_stroke(world_.origin(),change.revision,request.action_uuid,retired,created,error)!=world_state::Admission::New)return action::Outcome::Unknown;
        }else if(!request.dungeon_building&&world_.commit(world_.origin(),change,observed,error)!=world_state::Admission::New)return action::Outcome::Unknown;
        operation.construction_confirmed=true;
        return action::Outcome::Confirmed;
    }
    action::Outcome perform_move(const action::Binding& binding,const action::Request& request,
                                 const action::Evidence* authority,std::string& error) {
        if(!dungeon_scope(request,error))return action::Outcome::RejectedNoEffect;
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
           (!request.dungeon_building&&!world_.refresh_current(world_.origin(),request.canonical_record_key,observed,error)))return action::Outcome::RejectedNoEffect;
        const auto object_before=observed.state;
        if(authority&&(before!=authority->before||object_before!=authority->object_before)) {
            error="native Move Client before ledger/object differs from Host";return action::Outcome::RejectedNoEffect;
        }
        auto native_request=request;
        if(authority)native_request.building=authority->object_after;
        world_state::Change change{world_state::Kind::Move,revision,request.action_uuid,request.canonical_record_key,object_before,native_request.building};
        if(!request.dungeon_building&&world_.prepare(world_.origin(),change,error)!=world_state::Admission::New)return action::Outcome::RejectedNoEffect;
        auto& operation=buildings_.emplace(request.action_uuid,BuildOperation{request,binding,before,{},{},false,object_before}).first->second;
        if(!ui_move_runtime::move(native_request,operation.state,operation.receipt,error))
            return operation.receipt.value("attempted",false)?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
        if(!building_history_slice::remember_move(request.action_uuid,request.canonical_record_key,
                operation.receipt.at("undo_graph").get<uint64_t>(),error))return action::Outcome::Unknown;
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
        if(!remove_diagnostics::refresh_moved_building(request.snapshot_uuid,request.canonical_record_key,uid,error,request.dungeon_building?request.dungeon_scene:std::string{})||
           !remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
           !world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error))return action::Outcome::Unknown;
        change.after=operation.state;
        if(!request.dungeon_building&&world_.commit(world_.origin(),change,observed,error)!=world_state::Admission::New)return action::Outcome::Unknown;
        return action::Outcome::Confirmed;
    }
    action::Outcome perform_move_history(const action::Binding& binding,const action::Request& request,
                                         const action::Evidence* authority,std::string& error) {
        if(!dungeon_scope(request,error))return action::Outcome::RejectedNoEffect;
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(history_.contains(request.action_uuid)){
            error="shared Move history action already attempted; no replay";return action::Outcome::Unknown;
        }
        const auto found=buildings_.find(request.target_action_uuid);
        if(found==buildings_.end()){
            error="shared Move history original action missing";return action::Outcome::RejectedNoEffect;
        }
        const auto& source=found->second;
        if(source.request.action!=action::Action::MoveBuilding||source.request.action_uuid!=request.target_action_uuid||
           source.binding!=binding||source.request.snapshot_uuid!=request.snapshot_uuid||
           source.request.canonical_record_key!=request.canonical_record_key||!source.retired||
           !source.receipt.value("confirmed",false)||!source.object_before.alive||!source.state.alive||
           (request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)){
            error="shared Move history original action is not confirmed in this world";return action::Outcome::RejectedNoEffect;
        }
        const auto uid=source.receipt.at("native_uid").get<uint64_t>();
        const bool undo=request.action==action::Action::UndoBuilding;
        const auto& before_state=undo?source.state:source.object_before;
        const auto& after_state=undo?source.object_before:source.state;
        const auto controller=typed_action_runtime::status();
        if(!uid||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||
           !controller.contains("host_revision")){
            error="shared Move history lacks exact pending Controller request";return action::Outcome::RejectedNoEffect;
        }
        const auto revision=controller.at("host_revision").get<uint64_t>();
        action::Ledger before;
        remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
        if(!read_ledger(before,error)||
           !remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
           !scenery_kiss::read_action_prop(uid,actual,error))return action::Outcome::RejectedNoEffect;
        if(target.uid!=uid||target.normal_scenery!=source.normal_scenery||
           (request.dungeon_building?!remove_diagnostics::is_dungeon_building_target(target,request.dungeon_scene,error):
              (!target.normal_building&&!target.normal_scenery))||world_state::building_state(actual)!=before_state){
            error="shared Move history current native UID, family or transform differs";return action::Outcome::RejectedNoEffect;
        }
        if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error)||
           (!request.dungeon_building&&!world_.refresh_current(world_.origin(),request.canonical_record_key,observed,error)))return action::Outcome::RejectedNoEffect;
        world_state::Change change{world_state::Kind::Move,revision,request.action_uuid,
            request.canonical_record_key,before_state,after_state};
        if(!request.dungeon_building&&world_.prepare(world_.origin(),change,error)!=world_state::Admission::New)return action::Outcome::RejectedNoEffect;
        if(authority&&(authority->before!=before||authority->after!=before||
                       authority->object_before!=before_state||authority->object_after!=after_state)){
            error="shared Move history Client full before/effect differs from Host";return action::Outcome::RejectedNoEffect;
        }
        auto& operation=history_.emplace(request.action_uuid,HistoryOperation{request,binding,before,{},
            before_state,after_state,uid,false}).first->second;
        const auto outcome=building_history_slice::perform(request.action,request.target_action_uuid,error);
        if(outcome!=action::Outcome::Confirmed)return outcome;
        if(!read_ledger(operation.after,error)||!scenery_kiss::read_action_prop(uid,actual,error))return action::Outcome::Unknown;
        if(operation.after!=before||world_state::building_state(actual)!=after_state){
            error="shared Move history actual transform or full Ledger differs";return action::Outcome::Unknown;
        }
        if(!remove_diagnostics::refresh_moved_building(request.snapshot_uuid,request.canonical_record_key,uid,error,request.dungeon_building?request.dungeon_scene:std::string{})||
           !remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
           target.uid!=uid||target.normal_scenery!=source.normal_scenery||
           !world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error))return action::Outcome::Unknown;
        if(!request.dungeon_building&&world_.commit(world_.origin(),change,observed,error)!=world_state::Admission::New)return action::Outcome::Unknown;
        operation.confirmed=true;error.clear();return action::Outcome::Confirmed;
    }

    action::Outcome perform_selection_history(const action::Binding& binding,const action::Request& request,
                                           const action::Evidence* authority,std::string& error){
        uint64_t revision=0;
        action::SelectionMoveIntent intent;
        if(!ui_move_runtime::selection_move::history_intent(binding,request,intent,error))return action::Outcome::RejectedNoEffect;
        auto poses=request;poses.selection_move=std::move(intent);
        if(!action::valid_selection_move(poses.selection_move)){error="Selection Move complete intent invalid before shared preparation";return action::Outcome::RejectedNoEffect;}
        if(!current(binding,request.snapshot_uuid,error)||
           !ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error))return action::Outcome::RejectedNoEffect;
        std::vector<world_state::Change> changes;std::vector<uint64_t> uids;
        for(const auto& row:poses.selection_move.rows){
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,row.key,target,error)||
               !scenery_kiss::read_action_prop(target.uid,actual,error)||
               !world_state::bind_root_readback(world_.origin(),row.key,target,actual,observed,error)||
               !world_.refresh_current(world_.origin(),row.key,observed,error))return action::Outcome::RejectedNoEffect;
            if(observed.state!=row.before){error="Selection Move shared before differs key="+row.key;return action::Outcome::RejectedNoEffect;}
            changes.push_back({world_state::Kind::Move,revision,request.action_uuid,row.key,row.before,row.after});uids.push_back(target.uid);
            if(world_.prepare(world_.origin(),changes.back(),error)!=world_state::Admission::New)return action::Outcome::RejectedNoEffect;
        }
        const auto outcome=ui_move_runtime::selection_move::perform_history(binding,request,authority,error);
        if(outcome!=action::Outcome::Confirmed)return outcome;
        std::vector<world_state::LocalBinding> after;
        for(size_t i=0;i<changes.size();++i){
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,changes[i].object_key,target,error)||
               !scenery_kiss::read_action_prop(target.uid,actual,error)||
               !world_state::bind_root_readback(world_.origin(),changes[i].object_key,target,actual,observed,error))return action::Outcome::Unknown;
            if(target.uid!=uids[i]){error="Selection Move shared actual full UID changed index="+std::to_string(i);return action::Outcome::Unknown;}
            after.push_back(std::move(observed));
        }
        return world_.commit_moves(world_.origin(),changes,after,error)==world_state::Admission::New?action::Outcome::Confirmed:action::Outcome::Unknown;
    }
    action::Outcome perform_selection_move(const action::Binding& binding,const action::Request& request,
                                           const action::Evidence* authority,std::string& error){
        uint64_t revision=0;
        if(!action::valid_selection_move(request.selection_move)){error="Selection Move complete intent invalid before shared preparation";return action::Outcome::RejectedNoEffect;}
        if(!current(binding,request.snapshot_uuid,error)||
           !ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error))return action::Outcome::RejectedNoEffect;
        std::vector<world_state::Change> changes;std::vector<uint64_t> uids;
        for(const auto& row:request.selection_move.rows){
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,row.key,target,error)||
               !scenery_kiss::read_action_prop(target.uid,actual,error)||
               !world_state::bind_root_readback(world_.origin(),row.key,target,actual,observed,error)||
               !world_.refresh_current(world_.origin(),row.key,observed,error))return action::Outcome::RejectedNoEffect;
            if(observed.state!=row.before){error="Selection Move shared before differs key="+row.key;return action::Outcome::RejectedNoEffect;}
            changes.push_back({world_state::Kind::Move,revision,request.action_uuid,row.key,row.before,row.after});uids.push_back(target.uid);
            if(world_.prepare(world_.origin(),changes.back(),error)!=world_state::Admission::New)return action::Outcome::RejectedNoEffect;
        }
        const auto outcome=ui_move_runtime::selection_move::perform(binding,request,authority,error);
        if(outcome!=action::Outcome::Confirmed)return outcome;
        std::vector<world_state::LocalBinding> after;
        for(size_t i=0;i<changes.size();++i){
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,changes[i].object_key,target,error)||
               !scenery_kiss::read_action_prop(target.uid,actual,error)||
               !world_state::bind_root_readback(world_.origin(),changes[i].object_key,target,actual,observed,error))return action::Outcome::Unknown;
            if(target.uid!=uids[i]){error="Selection Move shared actual full UID changed index="+std::to_string(i);return action::Outcome::Unknown;}
            after.push_back(std::move(observed));
        }
        return world_.commit_moves(world_.origin(),changes,after,error)==world_state::Admission::New?action::Outcome::Confirmed:action::Outcome::Unknown;
    }
    action::Outcome perform_boss_placement(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
        uint64_t revision=0;
        if(!current(binding,request.snapshot_uuid,error)||!ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error)||revision<=world_.revision()){
            if(error.empty())error="Boss shared native revision must advance once";return action::Outcome::RejectedNoEffect;}
        const auto result=ui_move_runtime::boss_placement::perform(binding,request,authority,error);
        if(result!=action::Outcome::Confirmed)return result;
        return world_.retire_native_effects(world_.origin(),revision,request.action_uuid,{},error)?action::Outcome::Confirmed:action::Outcome::Unknown;
    }
    action::Outcome perform_group_object(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
        uint64_t revision=0;bool moving=false,restoring=false;std::vector<std::string> members;
        if(!current(binding,request.snapshot_uuid,error)||!ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error)||
           !ui_move_runtime::group_object::targets(binding,request,moving,restoring,members,error))return action::Outcome::RejectedNoEffect;
        if(revision<=world_.revision()){error="Group transaction shared revision must advance once for its whole original graph";return action::Outcome::RejectedNoEffect;}
        const bool history=request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding;
        std::map<std::string,world_state::LocalBinding> before;
        for(const auto& key:members){
            if(restoring){if(world_.rows().contains(key)){error="Group restoration shared member key is already live key="+key;return action::Outcome::RejectedNoEffect;}continue;}
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,error)||!target.normal_scenery||
               !scenery_kiss::read_action_prop(target.uid,actual,error)||!world_state::bind_root_readback(world_.origin(),key,target,actual,observed,error)||
               !world_.refresh_current(world_.origin(),key,observed,error))return action::Outcome::RejectedNoEffect;
            before.emplace(key,std::move(observed));
        }
        const auto result=history?ui_move_runtime::group_object::perform_history(binding,request,authority,error):ui_move_runtime::group_object::perform(binding,request,authority,error);
        if(result!=action::Outcome::Confirmed)return result;
        action::GroupObjectState native_before,native_after;
        if(!ui_move_runtime::group_object::publication(binding,request,native_before,native_after,error))return action::Outcome::Unknown;
        if(members.empty())return world_.retire_native_effects(world_.origin(),revision,request.action_uuid,{},error)?action::Outcome::Confirmed:action::Outcome::Unknown;
        std::vector<world_state::LocalBinding> after;std::vector<world_state::Change> changes;
        if(moving||restoring)for(const auto& key:members){remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,error)||!target.normal_scenery||!scenery_kiss::read_action_prop(target.uid,actual,error)||
               !world_state::bind_root_readback(world_.origin(),key,target,actual,observed,error))return action::Outcome::Unknown;
            if(moving){const auto prior=before.find(key);if(prior==before.end()||prior->second.local_uid!=observed.local_uid){error="Group moved member native full UID changed key="+key;return action::Outcome::Unknown;}
                if(prior->second.state!=observed.state){changes.push_back({world_state::Kind::Move,revision,request.action_uuid,key,prior->second.state,observed.state});after.push_back(std::move(observed));}}
            else after.push_back(std::move(observed));
        }
        if(moving){if(changes.empty())return world_.retire_native_effects(world_.origin(),revision,request.action_uuid,{},error)?action::Outcome::Confirmed:action::Outcome::Unknown;
            return world_.commit_moves(world_.origin(),changes,after,error)==world_state::Admission::New?action::Outcome::Confirmed:action::Outcome::Unknown;}
        const auto removed=restoring?std::vector<std::string>{}:members;
        const auto accepted=history?world_.commit_scenery_history(world_.origin(),revision,request.action_uuid,removed,after,error):
            world_.commit_scenery_stroke(world_.origin(),revision,request.action_uuid,removed,after,error);
        return accepted==world_state::Admission::New?action::Outcome::Confirmed:action::Outcome::Unknown;
    }
    action::Outcome perform_scenery_stroke(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
        uint64_t revision=0;
        if(!current(binding,request.snapshot_uuid,error)||!ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error))return action::Outcome::RejectedNoEffect;
        if(revision<=world_.revision()){error="Scenery stroke shared revision must advance once for its complete graph";return action::Outcome::RejectedNoEffect;}
        const auto result=ui_move_runtime::scenery_stroke::perform(binding,request,authority,error);
        if(result!=action::Outcome::Confirmed)return result;
        action::SceneryStrokeEffect effect;if(!ui_move_runtime::scenery_stroke::publication(binding,request,effect,error))return action::Outcome::Unknown;
        std::vector<std::string> removed;for(const auto& row:effect.removed)removed.push_back(row.key);
        std::vector<world_state::LocalBinding> created;
        for(const auto& row:effect.created){remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,row.key,target,error)||!target.normal_scenery||
               !scenery_kiss::read_action_prop(target.uid,actual,error)||!world_state::bind_root_readback(world_.origin(),row.key,target,actual,observed,error))return action::Outcome::Unknown;
            created.push_back(std::move(observed));
        }
        return world_.commit_scenery_stroke(world_.origin(),revision,request.action_uuid,removed,created,error)==world_state::Admission::New?
            action::Outcome::Confirmed:action::Outcome::Unknown;
    }
    action::Outcome perform_duplicate(const action::Binding& binding,const action::Request& request,
                                      const action::Evidence* authority,std::string& error){
        uint64_t revision=0;
        if(!action::valid_duplicate(request.duplicate)||!current(binding,request.snapshot_uuid,error)||
           !ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error))return action::Outcome::RejectedNoEffect;
        if(revision<=world_.revision()){error="Duplicate shared revision must advance before native creation";return action::Outcome::RejectedNoEffect;}
        for(const auto& row:request.duplicate.rows)for(const auto& key:row.created_keys)
            if(world_.rows().contains(key)){error="Duplicate new shared key already exists key="+key;return action::Outcome::RejectedNoEffect;}
        const bool building=std::any_of(request.duplicate.rows.begin(),request.duplicate.rows.end(),
            [](const auto& row){return row.family==action::DuplicateFamily::Building;});
        developer_staff::construction::PlacementBefore construction_before;
        if(building){
            if(duplicate_construction_.contains(request.action_uuid)){error="Duplicate construction already attempted; no replay";return action::Outcome::Unknown;}
            if(!developer_staff::construction::capture_placement(construction_before,error))return action::Outcome::RejectedNoEffect;
            retire_absent_construction(construction_before);
            duplicate_construction_.emplace(request.action_uuid,DuplicateConstruction{request,binding});
        }
        const auto result=ui_move_runtime::duplicate_edit::perform(binding,request,authority,error);
        if(result!=action::Outcome::Confirmed)return result;
        std::vector<world_state::LocalBinding> created;std::map<uint64_t,std::string> construction_targets;
        for(const auto& row:request.duplicate.rows)for(const auto& key:row.created_keys){
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,error)||
               !scenery_kiss::read_action_prop(target.uid,actual,error)||
               !world_state::bind_root_readback(world_.origin(),key,target,actual,observed,error))return action::Outcome::Unknown;
            if(row.family==action::DuplicateFamily::Building){
                if(!target.normal_building||target.normal_scenery||!construction_targets.emplace(target.uid,key).second){
                    error="Duplicate construction created Building family/full UID differs or is duplicated";return action::Outcome::Unknown;}}
            created.push_back(std::move(observed));
        }
        if(building&&!developer_staff::construction::finish_duplicate(construction_before,request.snapshot_uuid,
            construction_targets,duplicate_construction_.at(request.action_uuid).jobs,error))return action::Outcome::Unknown;
        if(world_.commit_creates(world_.origin(),revision,request.action_uuid,created,error)!=world_state::Admission::New)return action::Outcome::Unknown;
        if(building)duplicate_construction_.at(request.action_uuid).confirmed=true;
        return action::Outcome::Confirmed;
    }
    action::Outcome perform_duplicate_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
        uint64_t revision=0;std::map<uint64_t,std::string> targets;
        if(!current(binding,request.snapshot_uuid,error)||!ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error)||
           !ui_move_runtime::duplicate_edit::history_targets(binding,request,targets,error))return action::Outcome::RejectedNoEffect;
        if(revision<=world_.revision()){error="Duplicate history shared revision must advance once for complete graph";return action::Outcome::RejectedNoEffect;}
        const auto construction=duplicate_construction_.find(request.target_action_uuid);
        if(construction!=duplicate_construction_.end()){
            const auto& source=construction->second;
            if(!source.confirmed||source.binding!=binding||source.request.snapshot_uuid!=request.snapshot_uuid||
               source.request.action_uuid!=request.target_action_uuid){error="Duplicate history construction source is not current confirmed original";return action::Outcome::RejectedNoEffect;}
            for(const auto& job:source.jobs){const auto target=targets.find(job.building_uid);
                if(target==targets.end()||target->second!=job.building_key){error="Duplicate history construction original Building key/full UID differs";return action::Outcome::RejectedNoEffect;}}
        }
        const bool undo=request.action==action::Action::UndoBuilding;std::vector<std::string> removed;
        for(const auto& [uid,key]:targets){const auto row=world_.rows().find(key);
            if(row==world_.rows().end()||row->second.state.alive!=undo){error="Duplicate history shared target live/tombstone direction differs key="+key;return action::Outcome::RejectedNoEffect;}
            if(undo){remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
                if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,error)||target.uid!=uid||
                   !scenery_kiss::read_action_prop(uid,actual,error)||!world_state::bind_root_readback(world_.origin(),key,target,actual,observed,error)||
                   !world_.refresh_current(world_.origin(),key,observed,error))return action::Outcome::RejectedNoEffect;
                removed.push_back(key);
            }else if(!remove_diagnostics::history_target_absent(request.snapshot_uuid,key,uid,error))return action::Outcome::RejectedNoEffect;
        }
        const auto result=ui_move_runtime::duplicate_edit::perform_history(binding,request,authority,error);
        if(result!=action::Outcome::Confirmed)return result;
        if(undo){
            if(!world_.retire_native_effects(world_.origin(),revision,request.action_uuid,removed,error))return action::Outcome::Unknown;
            if(construction!=duplicate_construction_.end())construction->second.restored=false;
            return action::Outcome::Confirmed;
        }
        std::vector<world_state::LocalBinding> restored;
        for(const auto& [uid,key]:targets){remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,error)||target.uid!=uid||
               !scenery_kiss::read_action_prop(uid,actual,error)||!world_state::bind_root_readback(world_.origin(),key,target,actual,observed,error))return action::Outcome::Unknown;
            restored.push_back(std::move(observed));
        }
        if(!world_.restore_native_buildings(world_.origin(),revision,request.action_uuid,restored,error))return action::Outcome::Unknown;
        if(construction!=duplicate_construction_.end())construction->second.restored=true;
        return action::Outcome::Confirmed;
    }
    action::Outcome perform_modular(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
        uint64_t revision=0;std::vector<std::string> targets;
        if(!current(binding,request.snapshot_uuid,error)||!ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error)||!ui_move_runtime::modular::targets(binding,request,targets,error))return action::Outcome::RejectedNoEffect;
        if(revision<=world_.revision()){error="Modular shared revision must advance once for the complete original graph";return action::Outcome::RejectedNoEffect;}
        for(const auto& key:targets){remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,error)||(!target.normal_building&&!target.normal_scenery&&!remove_diagnostics::is_modular_target(target,error))||!scenery_kiss::read_action_prop(target.uid,actual,error)||!world_state::bind_root_readback(world_.origin(),key,target,actual,observed,error)||!world_.refresh_current(world_.origin(),key,observed,error))return action::Outcome::RejectedNoEffect;}
        const auto result=ui_move_runtime::modular::perform(binding,request,authority,error);if(result!=action::Outcome::Confirmed)return result;
        action::ModularEffect effect;if(!ui_move_runtime::modular::publication(binding,request,effect,error))return action::Outcome::Unknown;
        std::vector<std::string> removed;for(const auto& row:effect.removed)removed.push_back(row.key);std::vector<world_state::LocalBinding> created;
        for(const auto& row:effect.created){remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,row.key,target,error)||(!target.normal_building&&!target.normal_scenery&&!remove_diagnostics::is_modular_target(target,error))||!scenery_kiss::read_action_prop(target.uid,actual,error)||!world_state::bind_root_readback(world_.origin(),row.key,target,actual,observed,error))return action::Outcome::Unknown;created.push_back(std::move(observed));}
        if(!removed.empty()||!created.empty()){const bool history=request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding;const auto admitted=history?world_.commit_scenery_history(world_.origin(),revision,request.action_uuid,removed,created,error):world_.commit_scenery_stroke(world_.origin(),revision,request.action_uuid,removed,created,error);return admitted==world_state::Admission::New?action::Outcome::Confirmed:action::Outcome::Unknown;}
        for(const auto& pair:effect.changed)if(pair[1].family==action::DuplicateFamily::Building){remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;const auto prior=world_.rows().find(pair[1].key);
            if(prior==world_.rows().end()||!remove_diagnostics::resolve_record_target(request.snapshot_uuid,pair[1].key,target,error)||(!target.normal_building&&!remove_diagnostics::is_modular_target(target,error))||!scenery_kiss::read_action_prop(target.uid,actual,error)||!world_state::bind_root_readback(world_.origin(),pair[1].key,target,actual,observed,error))return action::Outcome::Unknown;
            if(prior->second.state!=observed.state){const world_state::Change change{world_state::Kind::Move,revision,request.action_uuid,pair[1].key,prior->second.state,observed.state};return world_.commit(world_.origin(),change,observed,error)==world_state::Admission::New?action::Outcome::Confirmed:action::Outcome::Unknown;}}
        return world_.retire_native_effects(world_.origin(),revision,request.action_uuid,{},error)?action::Outcome::Confirmed:action::Outcome::Unknown;
    }
    action::Outcome perform_terrain_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
        const auto result=ui_terrain_runtime::perform_history(binding,request,authority,error);if(result!=action::Outcome::Confirmed)return result;
        std::vector<std::string> keys;if(!ui_terrain_runtime::history_keys(binding,request,keys,error))return action::Outcome::Unknown;
        const auto controller=typed_action_runtime::status();const auto revision=controller.at("host_revision").get<uint64_t>();
        if(request.action==action::Action::RedoBuilding||keys.empty())return world_.retire_native_effects(world_.origin(),revision,request.action_uuid,keys,error)?action::Outcome::Confirmed:action::Outcome::Unknown;
        std::vector<world_state::LocalBinding> restored;
        for(const auto& key:keys){remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding row;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,error)||!scenery_kiss::read_action_prop(target.uid,actual,error)||
               !world_state::bind_root_readback(world_.origin(),key,target,actual,row,error))return action::Outcome::Unknown;restored.push_back(std::move(row));}
        return world_.restore_native_buildings(world_.origin(),revision,request.action_uuid,restored,error)?action::Outcome::Confirmed:action::Outcome::Unknown;
    }
    action::Outcome perform_building_history(const action::Binding& binding,const action::Request& request,
                                             const action::Evidence* authority,std::string& error) {
        if(!dungeon_scope(request,error))return action::Outcome::RejectedNoEffect;
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(request.canonical_record_key=="modular:"+request.target_action_uuid)return perform_modular(binding,request,authority,error);
        if(request.canonical_record_key=="district-edit:"+request.target_action_uuid)return ui_move_runtime::district_edit::perform_history(binding,request,authority,error);
        if(request.canonical_record_key=="district-stroke:"+request.target_action_uuid)return ui_move_runtime::district_stroke::perform_history(binding,request,authority,error);
        if(request.canonical_record_key=="zone-edit:"+request.target_action_uuid)return npc_probe::zone_edit_runtime::perform_history(binding,request,authority,error);
        if(request.canonical_record_key=="group-object:"+request.target_action_uuid)return perform_group_object(binding,request,authority,error);
        if(request.canonical_record_key=="scenery-stroke:"+request.target_action_uuid){
            uint64_t revision=0;std::map<uint64_t,std::string> targets;
            if(!ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error)||
               !ui_move_runtime::scenery_stroke::history_targets(binding,request,targets,error))return action::Outcome::RejectedNoEffect;
            if(revision<=world_.revision()){error="Scenery history shared revision must advance once for its complete graph";return action::Outcome::RejectedNoEffect;}
            const auto result=ui_move_runtime::scenery_stroke::perform_history(binding,request,authority,error);
            if(result!=action::Outcome::Confirmed)return result;
            action::SceneryStrokeEffect effect;if(!ui_move_runtime::scenery_stroke::history_publication(binding,request,effect,error))return action::Outcome::Unknown;
            std::vector<std::string> removed;for(const auto& row:effect.removed)removed.push_back(row.key);
            std::vector<world_state::LocalBinding> restored;
            for(const auto& row:effect.created){remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
                if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,row.key,target,error)||!target.normal_scenery||
                   !scenery_kiss::read_action_prop(target.uid,actual,error)||!world_state::bind_root_readback(world_.origin(),row.key,target,actual,observed,error))return action::Outcome::Unknown;
                restored.push_back(std::move(observed));
            }
            return world_.commit_scenery_history(world_.origin(),revision,request.action_uuid,removed,restored,error)==world_state::Admission::New?
                action::Outcome::Confirmed:action::Outcome::Unknown;
        }
        if(request.canonical_record_key=="duplicate:"+request.target_action_uuid)return perform_duplicate_history(binding,request,authority,error);
        if(request.canonical_record_key=="groups:"+request.target_action_uuid)return ui_move_runtime::group_edit::perform_history(binding,request,authority,error);
        if(request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding) {
            error="building history direction invalid";return action::Outcome::RejectedNoEffect;
        }
        if(request.canonical_record_key=="selection:"+request.target_action_uuid)
            return perform_selection_history(binding,request,authority,error);
        if(request.canonical_record_key=="dungeon-tiles:"+request.target_action_uuid)return npc_probe::monster_action_runtime::dungeon_cells::perform_history(binding,request,authority,error);
        if(action::npc_history(request))return npc_probe::action_runtime::history::perform(binding,request,authority,error);
        if(action::monster_history(request))return npc_probe::monster_action_runtime::history::perform(binding,request,authority,error);
        if(request.canonical_record_key=="dungeon-input:"+request.target_action_uuid)return npc_probe::monster_action_runtime::dungeon_input::perform_history(binding,request,authority,error);
        if(action::network_history(request))return network_history_runtime::perform(binding,request,authority,error);
        if(request.canonical_record_key=="path:"+request.target_action_uuid)return ui_path_runtime::perform_history(binding,request,authority,error);
        if(request.canonical_record_key=="terrain:"+request.target_action_uuid)return perform_terrain_history(binding,request,authority,error);
        if(request.canonical_record_key=="quests:"+request.target_action_uuid)return npc_probe::monster_action_runtime::quest_history::perform(binding,request,authority,error);
        if(region_history_key::parse(request.canonical_record_key))
            return ui_region_runtime::perform_history(binding,request,authority,error);
        const auto moved=buildings_.find(request.target_action_uuid);
        if(moved!=buildings_.end()&&moved->second.request.action==action::Action::MoveBuilding)
            return perform_move_history(binding,request,authority,error);
        if(history_.contains(request.action_uuid)) {
            error="building history action already attempted; no replay";return action::Outcome::Unknown;
        }
        action::BuildingState* source_state=nullptr;uint64_t uid=0;int32_t amount=0;bool source_remove=false,source_scenery=false,source_scene_scenery=false;
        const auto placed=buildings_.find(request.target_action_uuid);
        const auto removed=operations_.find(request.target_action_uuid);
        if(placed!=buildings_.end()&&removed!=operations_.end()) {
            error="building history target mutation is ambiguous";return action::Outcome::RejectedNoEffect;
        }
        if(placed!=buildings_.end()) {
            auto& source=placed->second;
            if(source.request.action!=action::Action::PlaceBuilding||source.request.action_uuid!=request.target_action_uuid||source.binding!=binding||
               source.request.snapshot_uuid!=request.snapshot_uuid||source.request.canonical_record_key!=request.canonical_record_key||
               !source.receipt.value("confirmed",false)||!source.retired) {
                error="prop history target has no confirmed current normal placement";return action::Outcome::RejectedNoEffect;
            }
            source_state=&source.state;uid=source.receipt.at("native_uid").get<uint64_t>();amount=source.receipt.at("cost").get<int32_t>();source_scenery=source.normal_scenery;
        } else if(removed!=operations_.end()) {
            auto& source=removed->second;
            if(source.request.action!=action::Action::RemoveSnapshotBuilding||source.request.action_uuid!=request.target_action_uuid||source.binding!=binding||
               source.request.snapshot_uuid!=request.snapshot_uuid||source.request.canonical_record_key!=request.canonical_record_key||
               !source.history_ready||!source.removal.value("confirmed",false)||
               !source.retirement.value("confirmed",false)) {
                error="prop history target has no confirmed retired normal removal";return action::Outcome::RejectedNoEffect;
            }
            source_remove=true;source_scene_scenery=source.scene_scenery;source_state=&source.object_before;uid=source.local_uid;amount=source.refund;source_scenery=source.normal_scenery;
        } else {
            error="building history target confirmed mutation missing";return action::Outcome::RejectedNoEffect;
        }
        if(!uid||amount<0||!source_state->alive){error="building history original UID/amount/state invalid";return action::Outcome::RejectedNoEffect;}
        const bool clearance=placed!=buildings_.end()&&bool(placed->second.clearance_after.local);
        const bool destroying=(request.action==action::Action::UndoBuilding)!=source_remove;
        auto alive=*source_state;auto dead=alive;dead.alive=false;
        const auto row=world_.rows().find(request.canonical_record_key);
        if(!source_scene_scenery&&(row==world_.rows().end()||row->second.state!=(destroying?alive:dead))) {
            error="building history target full shared state differs";return action::Outcome::RejectedNoEffect;
        }
        const auto controller=typed_action_runtime::status();
        if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||
           !controller.contains("host_revision")) {
            error="building history lacks exact pending Controller request";return action::Outcome::RejectedNoEffect;
        }
        const auto revision=controller.at("host_revision").get<uint64_t>();
        if(revision<=world_.revision()){error="building history shared revision stale";return action::Outcome::RejectedNoEffect;}
        action::Ledger before;if(!read_ledger(before,error))return action::Outcome::RejectedNoEffect;
        if(destroying) {
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding settled;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
               !scenery_kiss::read_action_prop(uid,actual,error))return action::Outcome::RejectedNoEffect;
            if(target.uid!=uid||target.normal_scenery!=source_scenery||
               (request.dungeon_building?!remove_diagnostics::is_dungeon_building_target(target,request.dungeon_scene,error):target.normal_building==source_scenery)){error="prop history destructive target UID/family differs";return action::Outcome::RejectedNoEffect;}
            if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,settled,error)||
               (!source_scene_scenery&&!world_.refresh_current(world_.origin(),request.canonical_record_key,settled,error)))return action::Outcome::RejectedNoEffect;
            alive=settled.state;dead=alive;dead.alive=false;*source_state=alive;
        } else if(!remove_diagnostics::history_target_absent(request.snapshot_uuid,request.canonical_record_key,uid,error))
            return action::Outcome::RejectedNoEffect;
        action::Ledger expected;
        if(!building_history_slice::cash_effect(before,destroying?amount:-amount,expected,error))return action::Outcome::RejectedNoEffect;
        if(authority&&(authority->before!=before||authority->object_before!=(destroying?alive:dead)||
                       authority->object_after!=(destroying?dead:alive)||authority->after!=expected)) {
            error="building history Client before/effect/full Ledger differs from Host";return action::Outcome::RejectedNoEffect;
        }
        ui_move_runtime::building_clearance::Snapshot clearance_before;
        if(clearance&&!ui_move_runtime::building_clearance::prepare_history(binding,request,placed->second.clearance_after,clearance_before,error,authority))return action::Outcome::RejectedNoEffect;
        if(clearance){std::vector<world_state::LocalBinding> current_scenery;
            if(!ui_move_runtime::building_clearance::current_bindings(clearance_before,world_.origin(),current_scenery,error))return action::Outcome::RejectedNoEffect;
            for(const auto& scenery_binding:current_scenery)if(!world_.refresh_current(world_.origin(),scenery_binding.object_key,scenery_binding,error))return action::Outcome::RejectedNoEffect;}
        auto& operation=history_.emplace(request.action_uuid,HistoryOperation{request,binding,before,{},
            destroying?alive:dead,destroying?dead:alive,uid,false}).first->second;operation.scene_scenery=source_scene_scenery;operation.clearance_before=std::move(clearance_before);
        const auto outcome=building_history_slice::perform(request.action,request.target_action_uuid,error);
        if(outcome!=action::Outcome::Confirmed)return outcome;
        if(!read_ledger(operation.after,error))return action::Outcome::Unknown;
        if(operation.after!=expected){error="building history actual full Ledger differs from original cash inverse";return action::Outcome::Unknown;}
        if(clearance){
            uintptr_t inverse=0;
            const auto opposite=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
            if(!building_history_slice::graph_for(opposite,request.target_action_uuid,inverse,error)||
               !ui_move_runtime::building_clearance::settle(binding,request,operation.clearance_before,&placed->second.clearance_after,uid,inverse,operation.clearance_after,error))return action::Outcome::Unknown;
        }
        if(destroying) {
            if(!remove_diagnostics::retire_undone_building(request.snapshot_uuid,request.canonical_record_key,uid,error)||
               (!clearance&&!request.dungeon_building&&!source_scene_scenery&&!world_.retire_native_effects(world_.origin(),revision,request.action_uuid,{request.canonical_record_key},error)))return action::Outcome::Unknown;
        } else {
            if(!(request.dungeon_building?
                remove_diagnostics::restore_redone_dungeon_building(request.snapshot_uuid,request.canonical_record_key,uid,request.dungeon_scene,error):
                remove_diagnostics::restore_redone_building(request.snapshot_uuid,request.canonical_record_key,uid,error)))return action::Outcome::Unknown;
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
                target.uid!=uid||target.normal_scenery!=source_scenery||
               (request.dungeon_building?!remove_diagnostics::is_dungeon_building_target(target,request.dungeon_scene,error):target.normal_building==source_scenery)||
               !scenery_kiss::read_action_prop(uid,actual,error)||
               world_state::building_state(actual)!=alive||
               !world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error)||
               (!clearance&&!request.dungeon_building&&!source_scene_scenery&&!world_.restore_native_building_key(world_.origin(),revision,request.action_uuid,request.canonical_record_key,observed,error))) {
                if(error.empty())error="building history restored native UID/family/full state differs";return action::Outcome::Unknown;
            }
        }
        if(clearance){
            std::vector<std::string> retired;std::vector<world_state::LocalBinding> restored;
            if(!ui_move_runtime::building_clearance::finish(binding,request,operation.clearance_before,operation.clearance_after,authority,error)||
               !ui_move_runtime::building_clearance::publication(operation.clearance_after,world_.origin(),retired,restored,error))return action::Outcome::Unknown;
            if(destroying)retired.push_back(request.canonical_record_key);
            else{remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
                if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
                   !scenery_kiss::read_action_prop(uid,actual,error)||!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error))return action::Outcome::Unknown;
                restored.push_back(std::move(observed));}
            if(world_.commit_scenery_history(world_.origin(),revision,request.action_uuid,retired,restored,error)!=world_state::Admission::New)return action::Outcome::Unknown;
        }
        operation.confirmed=true;error.clear();return action::Outcome::Confirmed;
    }
    #include "dungeon_building_engine.inc"
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
        {
        mt2mp::build_actions::private_native::host::RootSceneryReadBatch batch;
        building_mixed_identity::opened_file::CurrentWorld before,opened;
        if(!mt2mp::build_actions::private_native::host::RootSceneryReadBatch::read_current(before,error))return false;
        json sources;if(!remove_diagnostics::bindings(sources,error))return false;
        if(!mt2mp::build_actions::private_native::host::RootSceneryReadBatch::read_current(opened,error))return false;
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
            if(remove_diagnostics::scene_scenery::registered(snapshot,key,entry.at("local_uid").get<uint64_t>()))continue;
            native::BuildingState native_state;
            if(!scenery_kiss::read_action_prop(entry.at("local_uid").get<uint64_t>(),native_state,error))return false;
            const auto record=entry.value("native_record_key",key);
            origin.sha256=record.substr(0,64);
            objects.push_back({key,record,world_state::building_state(native_state)});
        }
        if(!batch.finish(error))return false;
        if(!world_.bind_snapshot(origin,objects,0,error))return false;
        }
        std::vector<developer_staff::construction::LocalJob> loaded;
        if(!developer_staff::construction::capture_loaded(snapshot,out,loaded,error))return false;
        for(const auto& job:loaded){const auto row=world_.rows().find(job.building_key);
            if(row==world_.rows().end()||!row->second.state.alive){error="Loaded construction lacks an admitted alive snapshot Building row";return false;}}
        loaded_construction_binding_=out;loaded_construction_snapshot_=snapshot;loaded_construction_=std::move(loaded);
        return true;
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
        if(request.dungeon_building)return perform_dungeon_building(binding,request,nullptr,error);
        if(request.action==action::Action::ReleaseEdit||request.action==action::Action::ReleaseCycle)return release_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::GeneralProject)return general_project_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetLogo)return logo_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::RemoveNetworkObject)return network_remove_runtime::perform(binding,request,nullptr,error);
        if((request.action==action::Action::ManagementCommand||request.action==action::Action::GmPolicy||request.action==action::Action::GameRules||request.action==action::Action::Intervention))return management_runtime::perform(binding,request,nullptr,error);
        if(research_action(request.action))return perform_research(binding,request,nullptr,error);
        if(request.action==action::Action::SetBugFix)return bug_authority::manual_action::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetBuildingStatus)return npc_probe::monster_action_runtime::building_status::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetDungeonLighting)return npc_probe::monster_action_runtime::dungeon_light::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditDungeonGizmo)return npc_probe::monster_action_runtime::dungeon_gizmo::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetDungeonPalette)return npc_probe::monster_action_runtime::dungeon_palette::perform(binding,request,nullptr,error);
        if(request.action==action::Action::DungeonInput)return npc_probe::monster_action_runtime::dungeon_input::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetDungeonAtmosphere)return npc_probe::monster_action_runtime::dungeon_sound::perform(binding,request,nullptr,error);
        if(request.action==action::Action::ConfigureDungeonInstances)return npc_probe::monster_action_runtime::dungeon_config::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditCharacterCostume)return npc_probe::monster_action_runtime::costume_edit::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditCharacterAbility)return npc_probe::monster_action_runtime::ability_edit::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditAbilityMembership)return npc_probe::monster_action_runtime::ability_membership::perform(binding,request,nullptr,error);
        if((request.action>=action::Action::RenameCharacter&&request.action<=action::Action::SetCharacterSpeed))return npc_probe::monster_action_runtime::character_rename::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditCharacterMembership)return npc_probe::monster_action_runtime::character_membership::perform(binding,request,nullptr,error);
        if((request.action==action::Action::SetNpcQuestList||request.action==action::Action::EditNpcQuest))return npc_probe::monster_action_runtime::quest_list::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetNpcQuestDestination)return npc_probe::monster_action_runtime::quest_authoring::perform(binding,request,nullptr,error);
        if(request.action==action::Action::RemoveMonster)return npc_probe::monster_action_runtime::remove_perform(binding,request,nullptr,error);
        if(request.action==action::Action::MoveMonster)return npc_probe::monster_action_runtime::move_perform(binding,request,nullptr,error);
        if(request.action==action::Action::PlaceMonster)return npc_probe::monster_action_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::PlaceMonsterZone)return npc_probe::zone_action_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetStartingpointWorkers)return perform_staff(binding,request,nullptr,error);
        if(request.action==action::Action::StartStartingpoint)return perform_start(binding,request,nullptr,error);
        if(request.action==action::Action::CommitStartingpoint)return perform_commit(binding,request,nullptr,error);
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC||request.action==action::Action::EditNpcProperty)
            return npc_probe::action_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetPrices)return ui_price_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::HireDeveloper||request.action==action::Action::FireDeveloper)
            return developer_staff::action_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)
            return perform_building_history(binding,request,nullptr,error);
        if((request.action==action::Action::PlaceNetworkNode||request.action==action::Action::PlaceNetworkPipe))return ui_network_runtime::perform(binding,request,nullptr,error);
        if((request.action==action::Action::PlacePath||request.action==action::Action::RemovePath||request.action==action::Action::PlaceWall||request.action==action::Action::PlacePass||request.action==action::Action::RemoveWorldPath))return ui_path_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::TerrainStroke)return ui_terrain_runtime::perform(binding,request,nullptr,error);
        if((request.action==action::Action::SetRegionPreset||request.action==action::Action::SetRegionStyles||request.action==action::Action::EditRegionProperty))return ui_region_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::PlaceBuilding)return perform_place(binding,request,nullptr,error);
        if(request.action==action::Action::MoveSelection)return perform_selection_move(binding,request,nullptr,error);
        if(request.action==action::Action::DuplicateSelection)return perform_duplicate(binding,request,nullptr,error);
        if(request.action==action::Action::PlaceBoss)return perform_boss_placement(binding,request,nullptr,error);
        if(request.action==action::Action::MoveGroup||request.action==action::Action::RemoveGroup)return perform_group_object(binding,request,nullptr,error);
        if(request.action==action::Action::SceneryBrushStroke||request.action==action::Action::SceneryEraseStroke)return perform_scenery_stroke(binding,request,nullptr,error);
        if(request.action==action::Action::EditServiceMembership)return ui_move_runtime::service_membership::perform(binding,request,nullptr,error);
        if(request.action==action::Action::ReorderBossPhase)return ui_move_runtime::boss_phase::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditGroup)return ui_move_runtime::group_edit::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditModularBuilding)return perform_modular(binding,request,nullptr,error);
        if(request.action==action::Action::EditDistrict)return ui_move_runtime::district_edit::perform(binding,request,nullptr,error);
        if(request.action==action::Action::DistrictStroke)return ui_move_runtime::district_stroke::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditZone)return npc_probe::zone_edit_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::MoveDistrictTitle)return ui_move_runtime::district_title::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditDistrictText)return ui_move_runtime::district_text::perform(binding,request,nullptr,error);
        if(request.action==action::Action::EditPropText)return ui_move_runtime::prop_text::perform(binding,request,nullptr,error);
        if(request.action==action::Action::MoveBuilding)return perform_move(binding,request,nullptr,error);
        return action::Engine::execute_action(binding,request,error);
    }
    action::Outcome apply_action_native(const action::Binding& binding,const action::Request& request,const action::Evidence& host,std::string& error) {
        if(request.dungeon_building)return perform_dungeon_building(binding,request,&host,error);
        if(request.action==action::Action::ReleaseEdit||request.action==action::Action::ReleaseCycle)return release_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::GeneralProject)return general_project_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::SetLogo)return logo_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::RemoveNetworkObject)return network_remove_runtime::perform(binding,request,&host,error);
        if((request.action==action::Action::ManagementCommand||request.action==action::Action::GmPolicy||request.action==action::Action::GameRules||request.action==action::Action::Intervention))return management_runtime::perform(binding,request,&host,error);
        if(research_action(request.action))return perform_research(binding,request,&host,error);
        if(request.action==action::Action::SetBugFix)return bug_authority::manual_action::perform(binding,request,&host,error);
        if(request.action==action::Action::SetBuildingStatus)return npc_probe::monster_action_runtime::building_status::perform(binding,request,&host,error);
        if(request.action==action::Action::SetDungeonLighting)return npc_probe::monster_action_runtime::dungeon_light::perform(binding,request,&host,error);
        if(request.action==action::Action::EditDungeonGizmo)return npc_probe::monster_action_runtime::dungeon_gizmo::perform(binding,request,&host,error);
        if(request.action==action::Action::SetDungeonPalette)return npc_probe::monster_action_runtime::dungeon_palette::perform(binding,request,&host,error);
        if(request.action==action::Action::DungeonInput)return npc_probe::monster_action_runtime::dungeon_input::perform(binding,request,&host,error);
        if(request.action==action::Action::SetDungeonAtmosphere)return npc_probe::monster_action_runtime::dungeon_sound::perform(binding,request,&host,error);
        if(request.action==action::Action::ConfigureDungeonInstances)return npc_probe::monster_action_runtime::dungeon_config::perform(binding,request,&host,error);
        if(request.action==action::Action::EditCharacterCostume)return npc_probe::monster_action_runtime::costume_edit::perform(binding,request,&host,error);
        if(request.action==action::Action::EditCharacterAbility)return npc_probe::monster_action_runtime::ability_edit::perform(binding,request,&host,error);
        if(request.action==action::Action::EditAbilityMembership)return npc_probe::monster_action_runtime::ability_membership::perform(binding,request,&host,error);
        if((request.action>=action::Action::RenameCharacter&&request.action<=action::Action::SetCharacterSpeed))return npc_probe::monster_action_runtime::character_rename::perform(binding,request,&host,error);
        if(request.action==action::Action::EditCharacterMembership)return npc_probe::monster_action_runtime::character_membership::perform(binding,request,&host,error);
        if((request.action==action::Action::SetNpcQuestList||request.action==action::Action::EditNpcQuest))return npc_probe::monster_action_runtime::quest_list::perform(binding,request,&host,error);
        if(request.action==action::Action::SetNpcQuestDestination)return npc_probe::monster_action_runtime::quest_authoring::perform(binding,request,&host,error);
        if(request.action==action::Action::RemoveMonster)return npc_probe::monster_action_runtime::remove_perform(binding,request,&host,error);
        if(request.action==action::Action::MoveMonster)return npc_probe::monster_action_runtime::move_perform(binding,request,&host,error);
        if(request.action==action::Action::PlaceMonster)return npc_probe::monster_action_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::PlaceMonsterZone)return npc_probe::zone_action_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::SetStartingpointWorkers)return perform_staff(binding,request,&host,error);
        if(request.action==action::Action::StartStartingpoint)return perform_start(binding,request,&host,error);
        if(request.action==action::Action::CommitStartingpoint)return perform_commit(binding,request,&host,error);
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC||request.action==action::Action::EditNpcProperty)
            return npc_probe::action_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::SetPrices)return ui_price_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::HireDeveloper||request.action==action::Action::FireDeveloper)
            return developer_staff::action_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)
            return perform_building_history(binding,request,&host,error);
        if((request.action==action::Action::PlaceNetworkNode||request.action==action::Action::PlaceNetworkPipe))return ui_network_runtime::perform(binding,request,&host,error);
        if((request.action==action::Action::PlacePath||request.action==action::Action::RemovePath||request.action==action::Action::PlaceWall||request.action==action::Action::PlacePass||request.action==action::Action::RemoveWorldPath))return ui_path_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::TerrainStroke)return ui_terrain_runtime::perform(binding,request,&host,error);
        if((request.action==action::Action::SetRegionPreset||request.action==action::Action::SetRegionStyles||request.action==action::Action::EditRegionProperty))return ui_region_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::MoveSelection)return perform_selection_move(binding,request,&host,error);
        if(request.action==action::Action::DuplicateSelection)return perform_duplicate(binding,request,&host,error);
        if(request.action==action::Action::PlaceBoss)return perform_boss_placement(binding,request,&host,error);
        if(request.action==action::Action::MoveGroup||request.action==action::Action::RemoveGroup)return perform_group_object(binding,request,&host,error);
        if(request.action==action::Action::SceneryBrushStroke||request.action==action::Action::SceneryEraseStroke)return perform_scenery_stroke(binding,request,&host,error);
        if(request.action==action::Action::EditServiceMembership)return ui_move_runtime::service_membership::perform(binding,request,&host,error);
        if(request.action==action::Action::ReorderBossPhase)return ui_move_runtime::boss_phase::perform(binding,request,&host,error);
        if(request.action==action::Action::EditGroup)return ui_move_runtime::group_edit::perform(binding,request,&host,error);
        if(request.action==action::Action::EditModularBuilding)return perform_modular(binding,request,&host,error);
        if(request.action==action::Action::EditDistrict)return ui_move_runtime::district_edit::perform(binding,request,&host,error);
        if(request.action==action::Action::DistrictStroke)return ui_move_runtime::district_stroke::perform(binding,request,&host,error);
        if(request.action==action::Action::EditZone)return npc_probe::zone_edit_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::MoveDistrictTitle)return ui_move_runtime::district_title::perform(binding,request,&host,error);
        if(request.action==action::Action::EditDistrictText)return ui_move_runtime::district_text::perform(binding,request,&host,error);
        if(request.action==action::Action::EditPropText)return ui_move_runtime::prop_text::perform(binding,request,&host,error);
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
        action::Context bug_context;if(live_state_&&(!action::selected_session().current(bug_context,error)||!bug_authority::begin_action(bug_context,binding,request.action_uuid,error)))return action::Outcome::RejectedNoEffect;
        if(live_state_&&!interventions::actors::live::begin_ordered(bug_context,binding,request.action_uuid,error)){
            const auto original=error;std::string teardown;if(!bug_authority::end_action(false,teardown))error=teardown+" original="+original;return action::Outcome::RejectedNoEffect;
        }
        if(live_state_&&!dungeon_instance_native::begin_action(request.action_uuid,error)){
            const auto original=error;std::string cleanup;
            if(!interventions::actors::live::end_ordered(false,cleanup))error=cleanup+" original="+original;
            std::string bug_cleanup;if(!bug_authority::end_action(false,bug_cleanup))error=bug_cleanup+" original="+error;
            return action::Outcome::RejectedNoEffect;
        }
        action::Outcome outcome=action::Outcome::Unknown;
        try{outcome=execute_action_native(binding,request,error);}
        catch(const std::exception& e){error=std::string("Original ordered action: ")+e.what();}
        catch(...){error="Original ordered action nonstandard exception";}
        if(live_state_){std::string instance_error;if(!dungeon_instance_native::end_action(request.action_uuid,outcome==action::Outcome::Confirmed,instance_error)){error=instance_error+" original="+error;outcome=action::Outcome::Unknown;}}
        if(live_state_){std::string actor_error;if(!interventions::actors::live::end_ordered(outcome==action::Outcome::Confirmed,actor_error)){error=actor_error+" original="+error;outcome=action::Outcome::Unknown;}}
        if(live_state_){std::string bug_error;if(!bug_authority::end_action(outcome==action::Outcome::Confirmed,bug_error)){error=bug_error+" original="+error;return action::Outcome::Unknown;}}
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
        action::Context bug_context;bool bug_scope=false,actor_scope=false,instance_scope=false;
        if(action::selected_session().current(bug_context,error))bug_scope=bug_authority::begin_action(bug_context,binding,request.action_uuid,error);
        if(bug_scope)actor_scope=interventions::actors::live::begin_ordered(bug_context,binding,request.action_uuid,error);
        if(actor_scope)instance_scope=dungeon_instance_native::begin_action(request.action_uuid,error);
        try {if(bug_scope&&actor_scope&&instance_scope)outcome=apply_action_native(binding,request,host,error);}
        catch(const std::exception& failure){error="live native action original exception: "+std::string(failure.what());}
        catch(...){error="live native action original nonstandard exception";}
        if(instance_scope){std::string instance_error;if(!dungeon_instance_native::end_action(request.action_uuid,outcome==action::Outcome::Confirmed,instance_error)){error=instance_error+" original="+error;outcome=action::Outcome::Unknown;}}
        if(actor_scope){std::string actor_error;if(!interventions::actors::live::end_ordered(outcome==action::Outcome::Confirmed,actor_error)){error=actor_error+" original="+error;outcome=action::Outcome::Unknown;}}
        if(bug_scope){std::string bug_error;if(!bug_authority::end_action(outcome==action::Outcome::Confirmed,bug_error)){error=bug_error+" original="+error;outcome=action::Outcome::Unknown;}}
        const auto original=error;std::string readback;
        if(!fence.end_action(readback)){error="live native cash scope: "+readback+" original="+original;return action::Outcome::Unknown;}
        if(outcome==action::Outcome::Confirmed&&!live_state_->after_action(authority,readback)){error="live actual Day after-action receipt: "+readback+" original="+original;return action::Outcome::Unknown;}
        error=original;return outcome;
    }
    bool observe_action(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error)override {
        return request.dungeon_building?observe_dungeon_building(binding,request,out,error):observe_action_base(binding,request,out,error);
    }
    bool observe_action_base(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error) {
        if(request.action==action::Action::ReleaseEdit||request.action==action::Action::ReleaseCycle)return release_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::GeneralProject)return general_project_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::SetLogo)return logo_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::RemoveNetworkObject)return network_remove_runtime::observe(binding,request,out,error);
        if((request.action==action::Action::ManagementCommand||request.action==action::Action::GmPolicy||request.action==action::Action::GameRules||request.action==action::Action::Intervention))return management_runtime::observe(binding,request,out,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="district-edit:"+request.target_action_uuid)
            return ui_move_runtime::district_edit::observe_history(binding,request,out,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="district-stroke:"+request.target_action_uuid)
            return ui_move_runtime::district_stroke::observe_history(binding,request,out,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="zone-edit:"+request.target_action_uuid)
            return npc_probe::zone_edit_runtime::observe_history(binding,request,out,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="group-object:"+request.target_action_uuid)
            return ui_move_runtime::group_object::observe_history(binding,request,out,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="scenery-stroke:"+request.target_action_uuid)
            return ui_move_runtime::scenery_stroke::observe_history(binding,request,out,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="duplicate:"+request.target_action_uuid)
            return ui_move_runtime::duplicate_edit::observe_history(binding,request,out,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="groups:"+request.target_action_uuid)
            return ui_move_runtime::group_edit::observe_history(binding,request,out,error);
        if(request.action==action::Action::SetBugFix)return bug_authority::manual_action::observe(binding,request,out,error);
        if(request.action==action::Action::SetBuildingStatus)return npc_probe::monster_action_runtime::building_status::observe(binding,request,out,error);
        if(request.action==action::Action::SetDungeonLighting)return npc_probe::monster_action_runtime::dungeon_light::observe(binding,request,out,error);
        if(request.action==action::Action::EditDungeonGizmo)return npc_probe::monster_action_runtime::dungeon_gizmo::observe(binding,request,out,error);
        if(request.action==action::Action::SetDungeonPalette)return npc_probe::monster_action_runtime::dungeon_palette::observe(binding,request,out,error);
        if(request.action==action::Action::DungeonInput)return npc_probe::monster_action_runtime::dungeon_input::observe(binding,request,out,error);
        if(request.action==action::Action::SetDungeonAtmosphere)return npc_probe::monster_action_runtime::dungeon_sound::observe(binding,request,out,error);
        if(request.action==action::Action::ConfigureDungeonInstances)return npc_probe::monster_action_runtime::dungeon_config::observe(binding,request,out,error);
        if(request.action==action::Action::EditCharacterCostume)return npc_probe::monster_action_runtime::costume_edit::observe(binding,request,out,error);
        if(request.action==action::Action::EditCharacterAbility)return npc_probe::monster_action_runtime::ability_edit::observe(binding,request,out,error);
        if(request.action==action::Action::EditAbilityMembership)return npc_probe::monster_action_runtime::ability_membership::observe(binding,request,out,error);
        if((request.action>=action::Action::RenameCharacter&&request.action<=action::Action::SetCharacterSpeed))return npc_probe::monster_action_runtime::character_rename::observe(binding,request,out,error);
        if(request.action==action::Action::EditCharacterMembership)return npc_probe::monster_action_runtime::character_membership::observe(binding,request,out,error);
        if((request.action==action::Action::SetNpcQuestList||request.action==action::Action::EditNpcQuest))return npc_probe::monster_action_runtime::quest_list::observe(binding,request,out,error);
        if(request.action==action::Action::SetNpcQuestDestination)return npc_probe::monster_action_runtime::quest_authoring::observe(binding,request,out,error);
        if(request.action==action::Action::RemoveMonster)return npc_probe::monster_action_runtime::remove_observe(binding,request,out,error);
        if(request.action==action::Action::MoveMonster)return npc_probe::monster_action_runtime::move_observe(binding,request,out,error);
        if(request.action==action::Action::PlaceMonster)return npc_probe::monster_action_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::PlaceMonsterZone)return npc_probe::zone_action_runtime::observe(binding,request,out,error);
        if(research_action(request.action)||request.action==action::Action::CommitStartingpoint||request.action==action::Action::StartStartingpoint||request.action==action::Action::SetStartingpointWorkers)return observe_commit(binding,request,out,error);
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC||request.action==action::Action::EditNpcProperty)
            return npc_probe::action_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::SetPrices)return ui_price_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::HireDeveloper||request.action==action::Action::FireDeveloper)
            return developer_staff::action_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding) {
            if(request.canonical_record_key=="selection:"+request.target_action_uuid)
                return current(binding,request.snapshot_uuid,error)&&ui_move_runtime::selection_move::observe_history(binding,request,out,error);
            if(!current(binding,request.snapshot_uuid,error))return false;
            if(request.canonical_record_key=="dungeon-tiles:"+request.target_action_uuid)return npc_probe::monster_action_runtime::dungeon_cells::observe_history(binding,request,out,error);
            if(action::npc_history(request))return npc_probe::action_runtime::history::observe(binding,request,out,error);
            if(action::monster_history(request))return npc_probe::monster_action_runtime::history::observe(binding,request,out,error);
            if(request.canonical_record_key=="dungeon-input:"+request.target_action_uuid)return npc_probe::monster_action_runtime::dungeon_input::observe_history(binding,request,out,error);
            if(action::network_history(request))return network_history_runtime::observe(binding,request,out,error);
            if(request.canonical_record_key=="path:"+request.target_action_uuid)return ui_path_runtime::observe_history(binding,request,out,error);
            if(request.canonical_record_key=="terrain:"+request.target_action_uuid)return ui_terrain_runtime::observe_history(binding,request,out,error);
            if(request.canonical_record_key=="quests:"+request.target_action_uuid)return npc_probe::monster_action_runtime::quest_history::observe(binding,request,out,error);
            if(region_history_key::parse(request.canonical_record_key))return ui_region_runtime::observe_history(binding,request,out,error);
            const auto found=history_.find(request.action_uuid);
            if(found==history_.end()||found->second.request!=request||found->second.binding!=binding||
               !found->second.confirmed) {error="building history matching native receipt absent";return false;}
            const auto& operation=found->second;
            action::Ledger actual;
            if(!read_ledger(actual,error)||actual!=operation.after) {
                if(error.empty())error="building history actual ledger changed";return false;
            }
            const auto row=world_.rows().find(request.canonical_record_key);
            if(!operation.scene_scenery&&(row==world_.rows().end()||row->second.state!=operation.object_after)) {
                error="building history shared object state changed";return false;
            }
            if(operation.object_after.alive) {
                remove_diagnostics::LocalTarget target;native::BuildingState native_state;
                if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
                   target.uid!=operation.uid||!scenery_kiss::read_action_prop(target.uid,native_state,error)||
                   world_state::building_state(native_state)!=operation.object_after)return false;
            }
            out={};out.before=operation.before;out.after=operation.after;
            out.object_before=operation.object_before;out.object_after=operation.object_after;
            if(!operation.object_after.alive&&!remove_diagnostics::history_target_absent(request.snapshot_uuid,request.canonical_record_key,operation.uid,error))return false;
            if(operation.clearance_after.local){action::SceneryStrokeEffect effect;
                if(!ui_move_runtime::building_clearance::observe(binding,request,operation.clearance_after,effect,error))return false;
                out.building_clearance=std::move(effect);}
            out.undo_confirmed=true;out.target_retired=!operation.object_after.alive;
            out.retirement_confirmed=operation.retired;return true;
        }

        if((request.action==action::Action::PlaceNetworkNode||request.action==action::Action::PlaceNetworkPipe))return ui_network_runtime::observe(binding,request,out,error);
        if((request.action==action::Action::PlacePath||request.action==action::Action::RemovePath||request.action==action::Action::PlaceWall||request.action==action::Action::PlacePass||request.action==action::Action::RemoveWorldPath))return ui_path_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::TerrainStroke)return ui_terrain_runtime::observe(binding,request,out,error);
        if((request.action==action::Action::SetRegionPreset||request.action==action::Action::SetRegionStyles||request.action==action::Action::EditRegionProperty))return ui_region_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::MoveSelection)return ui_move_runtime::selection_move::observe(binding,request,out,error);
        if(request.action==action::Action::DuplicateSelection)return ui_move_runtime::duplicate_edit::observe(binding,request,out,error);
        if(request.action==action::Action::MoveGroup||request.action==action::Action::RemoveGroup)return ui_move_runtime::group_object::observe(binding,request,out,error);
        if(request.action==action::Action::SceneryBrushStroke||request.action==action::Action::SceneryEraseStroke)return ui_move_runtime::scenery_stroke::observe(binding,request,out,error);
        if(request.action==action::Action::EditServiceMembership)return ui_move_runtime::service_membership::observe(binding,request,out,error);
        if(request.action==action::Action::PlaceBoss)return ui_move_runtime::boss_placement::observe(binding,request,out,error);
        if(request.action==action::Action::ReorderBossPhase)return ui_move_runtime::boss_phase::observe(binding,request,out,error);
        if(request.action==action::Action::EditGroup)return ui_move_runtime::group_edit::observe(binding,request,out,error);
        if(request.action==action::Action::EditModularBuilding||((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="modular:"+request.target_action_uuid))return ui_move_runtime::modular::observe(binding,request,out,error);
        if(request.action==action::Action::EditDistrict)return ui_move_runtime::district_edit::observe(binding,request,out,error);
        if(request.action==action::Action::DistrictStroke)return ui_move_runtime::district_stroke::observe(binding,request,out,error);
        if(request.action==action::Action::EditZone)return npc_probe::zone_edit_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::MoveDistrictTitle)return ui_move_runtime::district_title::observe(binding,request,out,error);
        if(request.action==action::Action::EditDistrictText)return ui_move_runtime::district_text::observe(binding,request,out,error);
        if(request.action==action::Action::EditPropText)return ui_move_runtime::prop_text::observe(binding,request,out,error);
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
        if(operation.clearance_after.local){action::SceneryStrokeEffect effect;
            if(!ui_move_runtime::building_clearance::observe(binding,request,operation.clearance_after,effect,error))return false;
            out.building_clearance=std::move(effect);}
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
        return request.dungeon_building?retire_dungeon_building(binding,request,error):retire_base(binding,request,error);
    }
    bool retire_base(const action::Binding& binding,const action::Request& request,std::string& error) {
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="terrain:"+request.target_action_uuid)
            return current(binding,request.snapshot_uuid,error)&&ui_terrain_runtime::retire_history(binding,request,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="dungeon-tiles:"+request.target_action_uuid){
            if(!current(binding,request.snapshot_uuid,error))return false;
            action::Evidence actual;if(!npc_probe::monster_action_runtime::dungeon_cells::observe_history(binding,request,actual,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Dungeon tile history retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return npc_probe::monster_action_runtime::dungeon_cells::retire_history(binding,request,error);
        }
        if(action::npc_history(request)){
            if(!current(binding,request.snapshot_uuid,error))return false;
            action::Evidence actual;if(!npc_probe::action_runtime::history::observe(binding,request,actual,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="NPC history retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return npc_probe::action_runtime::history::retire(binding,request,error);
        }
        if(action::monster_history(request)){
            if(!current(binding,request.snapshot_uuid,error))return false;
            action::Evidence actual;if(!npc_probe::monster_action_runtime::history::observe(binding,request,actual,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Monster history retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return npc_probe::monster_action_runtime::history::retire(binding,request,error);
        }
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="dungeon-input:"+request.target_action_uuid){
            if(!current(binding,request.snapshot_uuid,error))return false;
            action::Evidence actual;if(!npc_probe::monster_action_runtime::dungeon_input::observe_history(binding,request,actual,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Dungeon input history retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return npc_probe::monster_action_runtime::dungeon_input::retire_history(binding,request,error);
        }
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="quests:"+request.target_action_uuid){
            if(!current(binding,request.snapshot_uuid,error))return false;
            action::Evidence actual;if(!npc_probe::monster_action_runtime::quest_history::observe(binding,request,actual,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Quest history retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return npc_probe::monster_action_runtime::quest_history::retire(binding,request,error);
        }
        if(action::network_history(request)){
            if(!current(binding,request.snapshot_uuid,error))return false;
            action::Evidence actual;if(!network_history_runtime::observe(binding,request,actual,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Network history retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return network_history_runtime::retire(binding,request,error);
        }
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="path:"+request.target_action_uuid){
            if(!current(binding,request.snapshot_uuid,error))return false;
            action::Evidence actual;if(!ui_path_runtime::observe_history(binding,request,actual,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Path history retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return ui_path_runtime::retire_history(binding,request,error);
        }
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&region_history_key::parse(request.canonical_record_key)){
            if(!current(binding,request.snapshot_uuid,error))return false;
            action::Evidence actual;if(!ui_region_runtime::observe_history(binding,request,actual,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Region history retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return ui_region_runtime::retire_history(binding,request,error);
        }
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&
           request.canonical_record_key=="selection:"+request.target_action_uuid)
            return current(binding,request.snapshot_uuid,error)&&ui_move_runtime::selection_move::retire_history(binding,request,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="district-edit:"+request.target_action_uuid)
            return ui_move_runtime::district_edit::retire_history(binding,request,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="district-stroke:"+request.target_action_uuid)
            return ui_move_runtime::district_stroke::retire_history(binding,request,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="zone-edit:"+request.target_action_uuid)
            return npc_probe::zone_edit_runtime::retire_history(binding,request,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="group-object:"+request.target_action_uuid)
            return ui_move_runtime::group_object::retire_history(binding,request,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="scenery-stroke:"+request.target_action_uuid)
            return ui_move_runtime::scenery_stroke::retire_history(binding,request,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="duplicate:"+request.target_action_uuid)
            return ui_move_runtime::duplicate_edit::retire_history(binding,request,error);
        if((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="groups:"+request.target_action_uuid)
            return ui_move_runtime::group_edit::retire_history(binding,request,error);
        if(request.action==action::Action::MoveSelection)return ui_move_runtime::selection_move::retire(binding,request,error);
        if(request.action==action::Action::DuplicateSelection)return ui_move_runtime::duplicate_edit::retire(binding,request,error);
        if(request.action==action::Action::MoveGroup||request.action==action::Action::RemoveGroup)return ui_move_runtime::group_object::retire(binding,request,error);
        if(request.action==action::Action::SceneryBrushStroke||request.action==action::Action::SceneryEraseStroke)return ui_move_runtime::scenery_stroke::retire(binding,request,error);
        if(request.action==action::Action::EditServiceMembership)return ui_move_runtime::service_membership::retire(binding,request,error);
        if(request.action==action::Action::PlaceBoss)return ui_move_runtime::boss_placement::retire(binding,request,error);
        if(request.action==action::Action::ReorderBossPhase)return ui_move_runtime::boss_phase::retire(binding,request,error);
        if(request.action==action::Action::EditGroup)return ui_move_runtime::group_edit::retire(binding,request,error);
        if(request.action==action::Action::EditModularBuilding||((request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="modular:"+request.target_action_uuid))return ui_move_runtime::modular::retire(binding,request,error);
        if(request.action==action::Action::EditDistrict)return ui_move_runtime::district_edit::retire(binding,request,error);
        if(request.action==action::Action::DistrictStroke)return ui_move_runtime::district_stroke::retire(binding,request,error);
        if(request.action==action::Action::EditZone)return npc_probe::zone_edit_runtime::retire(binding,request,error);
        if(request.action==action::Action::MoveDistrictTitle)return ui_move_runtime::district_title::retire(binding,request,error);
        if(request.action==action::Action::EditDistrictText)return ui_move_runtime::district_text::retire(binding,request,error);
        if(request.action==action::Action::EditPropText)return ui_move_runtime::prop_text::retire(binding,request,error);
        if(request.action==action::Action::ReleaseEdit||request.action==action::Action::ReleaseCycle)return release_runtime::retire(binding,request,error);
        if(request.action==action::Action::GeneralProject)return general_project_runtime::retire(binding,request,error);
        if(request.action==action::Action::SetLogo)return logo_runtime::retire(binding,request,error);
        if(request.action==action::Action::RemoveNetworkObject)return network_remove_runtime::retire(binding,request,error);
        if((request.action==action::Action::ManagementCommand||request.action==action::Action::GmPolicy||request.action==action::Action::GameRules||request.action==action::Action::Intervention))return management_runtime::retire(binding,request,error);
        if(request.action==action::Action::EditCharacterMembership){
            if(!current(binding,request.snapshot_uuid,error))return false;
            std::vector<std::string> keys;
            if(!npc_probe::monster_action_runtime::character_membership::retired_keys(binding,request,keys,error))return false;
            const auto status=typed_action_runtime::controller_status();
            if(!status.value("pending",false)||status.value("action_uuid",std::string{})!=request.action_uuid){error="Character membership retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),status.at("host_revision").get<uint64_t>(),request.action_uuid,keys,error))return false;
            return npc_probe::monster_action_runtime::character_membership::retire(binding,request,error);
        }
        if(request.action==action::Action::DungeonInput||request.action==action::Action::SetBuildingStatus||request.action==action::Action::SetDungeonAtmosphere||request.action==action::Action::EditDungeonGizmo||request.action==action::Action::SetDungeonPalette||request.action==action::Action::SetDungeonLighting||request.action==action::Action::ConfigureDungeonInstances||request.action==action::Action::EditCharacterCostume||request.action==action::Action::EditCharacterAbility||request.action==action::Action::EditAbilityMembership||(request.action>=action::Action::RenameCharacter&&request.action<=action::Action::SetCharacterSpeed)||request.action==action::Action::PlaceMonsterZone||request.action==action::Action::PlaceMonster||request.action==action::Action::MoveMonster||request.action==action::Action::RemoveMonster||request.action==action::Action::SetNpcQuestDestination||(request.action==action::Action::SetNpcQuestList||request.action==action::Action::EditNpcQuest)){
            if(!current(binding,request.snapshot_uuid,error))return false;
            const auto status=typed_action_runtime::controller_status();
            if(!status.value("pending",false)||status.value("action_uuid",std::string{})!=request.action_uuid){error="Zone retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),status.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            if(request.action==action::Action::SetBuildingStatus)return npc_probe::monster_action_runtime::building_status::retire(binding,request,error);
            if(request.action==action::Action::SetDungeonLighting)return npc_probe::monster_action_runtime::dungeon_light::retire(binding,request,error);
            if(request.action==action::Action::EditDungeonGizmo)return npc_probe::monster_action_runtime::dungeon_gizmo::retire(binding,request,error);
            if(request.action==action::Action::SetDungeonPalette)return npc_probe::monster_action_runtime::dungeon_palette::retire(binding,request,error);
            if(request.action==action::Action::DungeonInput)return npc_probe::monster_action_runtime::dungeon_input::retire(binding,request,error);
            if(request.action==action::Action::SetDungeonAtmosphere)return npc_probe::monster_action_runtime::dungeon_sound::retire(binding,request,error);
            if(request.action==action::Action::ConfigureDungeonInstances)return npc_probe::monster_action_runtime::dungeon_config::retire(binding,request,error);
            if(request.action==action::Action::EditCharacterCostume)return npc_probe::monster_action_runtime::costume_edit::retire(binding,request,error);
            if(request.action==action::Action::EditCharacterAbility)return npc_probe::monster_action_runtime::ability_edit::retire(binding,request,error);
            if(request.action==action::Action::EditAbilityMembership)return npc_probe::monster_action_runtime::ability_membership::retire(binding,request,error);
            if((request.action>=action::Action::RenameCharacter&&request.action<=action::Action::SetCharacterSpeed))return npc_probe::monster_action_runtime::character_rename::retire(binding,request,error);
            if((request.action==action::Action::SetNpcQuestList||request.action==action::Action::EditNpcQuest))return npc_probe::monster_action_runtime::quest_list::retire(binding,request,error);
        if(request.action==action::Action::SetNpcQuestDestination)return npc_probe::monster_action_runtime::quest_authoring::retire(binding,request,error);
            if(request.action==action::Action::RemoveMonster)return npc_probe::monster_action_runtime::remove_retire(binding,request,error);
            if(request.action==action::Action::MoveMonster)return npc_probe::monster_action_runtime::move_retire(binding,request,error);
            return request.action!=action::Action::PlaceMonsterZone?npc_probe::monster_action_runtime::retire(binding,request,error):npc_probe::zone_action_runtime::retire(binding,request,error);
        }
        if(request.action==action::Action::SetBugFix)return bug_authority::manual_action::retire(binding,request,error);
        if(request.action==action::Action::HireDeveloper||request.action==action::Action::FireDeveloper){
            if(!current(binding,request.snapshot_uuid,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){
                error="Developer retirement lacks exact Controller request";return false;}
            action::Evidence actual;if(!developer_staff::action_runtime::observe(binding,request,actual,error))return false;
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            return developer_staff::action_runtime::retire(binding,request,error);
        }
        if(research_action(request.action)||request.action==action::Action::CommitStartingpoint||request.action==action::Action::StartStartingpoint||request.action==action::Action::SetStartingpointWorkers){
            action::Evidence actual;if(!observe_commit(binding,request,actual,error))return false;
            const auto controller=typed_action_runtime::status();
            if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){
                error="startingpoint retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),controller.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
            commits_.at(request.action_uuid).evidence.retirement_confirmed=true;return true;
        }
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC||request.action==action::Action::EditNpcProperty) {
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
        if((request.action==action::Action::PlacePath||request.action==action::Action::RemovePath||request.action==action::Action::PlaceWall||request.action==action::Action::PlacePass||request.action==action::Action::RemoveWorldPath)) {
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
            if(!request.dungeon_building&&!found->second.scene_scenery&&world_.commit(world_.origin(),change,observed,error)!=world_state::Admission::New)return false;
            {
                action::Ledger after_ledger,expected;if(!read_ledger(after_ledger,error))return false;
                const int64_t refund=int64_t(after_ledger.cash)-found->second.before.cash;
                if(refund<0||refund>INT32_MAX){error="building history Remove refund outside native int32 domain";return false;}
                if(!building_history_slice::cash_effect(found->second.before,int32_t(refund),expected,error)||expected!=after_ledger) {
                    if(error.empty())error="building history Remove refund full Ledger/category differs";return false;
                }
                uintptr_t graph=0;
                if(!remove_diagnostics::confirmed_remove_graph(request.action_uuid,request.snapshot_uuid,request.canonical_record_key,
                    found->second.local_uid,graph,error)||!building_history_slice::remember_remove(request.action_uuid,request.canonical_record_key,graph,error))return false;
                found->second.refund=int32_t(refund);found->second.history_ready=true;
            }
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
    std::unique_ptr<live_host_state::PartyPort> parties;
    std::unique_ptr<scenario_vip_authority::FrameAdapter> vip_presentation;
    std::unique_ptr<management_runtime::LivePort> management;
    std::unique_ptr<release_runtime::LivePort> release;
    std::unique_ptr<marketing_authority::NativePort> marketing;
    std::unique_ptr<live_host_state::Bridge> live_state;
    std::unique_ptr<live_host_state::day::economy::NativePort> economy_port;
    std::unique_ptr<live_host_state::day::economy::Coupling> economy_state;
    std::unique_ptr<live_host_state::day::NativePort> day_port;
    std::unique_ptr<live_host_state::day::Sync> day_state;
    std::string live_error,live_publication_wait;
    bool live_failure_terminal=false;
    toon_weapon_visual::Receipt weapon_receipt;
    uintptr_t weapon_toon=0;
    toon_weapon_visual::Receipt weapon_hook_receipt;
    uintptr_t weapon_hook_toon=0;
    std::optional<action::Context> weapon_bootstrap_context;
    action::Binding weapon_bootstrap_binding;
    toon_time_fence::Receipt toon_time_receipt,toon_time_discarded;
    uintptr_t toon_time_receiver=0,toon_time_discarded_receiver=0;
    uint64_t toon_time_confirmed=0;
    scenario_vip_authority::Receipt vip_last,vip_failure;
    uint64_t vip_suppressed=0;
    Runtime():session(action::selected_session()),
        transport(Config::instance().local_test?static_cast<action::Transport&>(local_transport):static_cast<action::Transport&>(steam_transport)),
        controller(session,engine,transport,[](const char* level,const std::string& operation,const std::string& cause) {
        if(std::string_view(level)=="error")LOG_ERROR("[Action] {}: {}",operation,cause);
        else if(std::string_view(level)=="warn")LOG_WARN("[Action] {}: {}",operation,cause);
        else LOG_INFO("[Action] {}: {}",operation,cause);
    },60000,[]{return GetTickCount64();}){}
};
std::atomic<Runtime*> published_runtime{nullptr};
}
#include "toon_weapon_visual/runtime.inc"
#include "toon_weapon_visual/probe_runtime.inc"
#include "toon_time_fence/runtime.inc"
#include "scenario_vip_authority/runtime.inc"
#include "scenario_vip_authority/alert_runtime.inc"
namespace {
bool vip_initialized=false,vip_initialize_attempted=false;
std::string vip_initialize_error;
bool vip_initialize(std::string& error){
    if(vip_initialized)return scenario_vip_authority::verify_hook(error);
    if(vip_initialize_attempted){error=vip_initialize_error;return false;}
    vip_initialize_attempted=true;
    vip_initialized=scenario_vip_authority::initialize(weapon_image,
        {vip_client_active,vip_owner,vip_publish,scenario_vip_authority::host_update},vip_initialize_error);
    error=vip_initialize_error;return vip_initialized;
}
bool toon_time_initialized=false,toon_time_initialize_attempted=false;
std::string toon_time_initialize_error;
bool toon_time_initialize(std::string& error){
    if(toon_time_initialized)return toon_time_fence::verify_hook(error);
    if(toon_time_initialize_attempted){error=toon_time_initialize_error;return false;}
    toon_time_initialize_attempted=true;
    toon_time_initialized=toon_time_fence::initialize(weapon_image,
        {toon_time_owner,toon_time_publish,toon_time_client_active},toon_time_initialize_error);
    error=toon_time_initialize_error;return toon_time_initialized;
}
}
bool start(std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime){runtime=new Runtime;published_runtime.store(runtime,std::memory_order_release);}
    if(runtime->thread!=GetCurrentThreadId()){error="typed action start requires bound Mainthread";return false;}
    if(runtime->lobby_retiring){error="typed lobby owner is retiring; no start";return false;}
    if(runtime->attempted){error="typed action start already attempted; no restart";return false;}
    runtime->attempted=true;
    if(!weapon_initialize(error))return false;
    if(!toon_time_initialize(error))return false;
    if(!vip_initialize(error))return false;
    if(!vip_alert_initialize(error))return false;
    if(!release_runtime::enable(error)||!general_project_runtime::enable(error)||!logo_runtime::enable(error)||!management_runtime::enable(error)||!ui_price_runtime::enable(error)||!npc_probe::action_runtime::enable(error)||!developer_staff::action_runtime::enable(error)||!npc_probe::zone_action_runtime::enable(error)||!npc_probe::zone_edit_runtime::enable(error)||!npc_probe::monster_action_runtime::enable(error)||!npc_probe::monster_action_runtime::quest_authoring::enable(error)||!npc_probe::monster_action_runtime::quest_list::enable(error)||!npc_probe::monster_action_runtime::character_membership::enable(error)||!npc_probe::monster_action_runtime::character_rename::enable(error)||!npc_probe::monster_action_runtime::ability_membership::enable(error)||!npc_probe::monster_action_runtime::ability_edit::enable(error)||!npc_probe::monster_action_runtime::costume_edit::enable(error)||!npc_probe::monster_action_runtime::dungeon_config::enable(error)||!npc_probe::monster_action_runtime::dungeon_sound::enable(error)||!npc_probe::monster_action_runtime::dungeon_gizmo::enable(error)||!npc_probe::monster_action_runtime::dungeon_palette::enable(error)||!npc_probe::monster_action_runtime::dungeon_input::enable(error)||!npc_probe::monster_action_runtime::dungeon_light::enable(error)||!npc_probe::monster_action_runtime::building_status::enable(error)||!bug_authority::manual_action::enable(error))return false;
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
    if(context.role==action::Role::Client){
        if(!runtime->engine.current(binding,context.snapshot_uuid,error))return false;
        bool movement_attempted=false;
        if(!developer_staff::construction::settle_loaded_client(context,binding,movement_attempted,error))return false;
    }
    runtime->live_port=std::make_unique<live_host_state::NativePort>(context,binding,&fence);
    auto live_state=std::make_unique<live_host_state::Bridge>(context,binding,epoch,*runtime->live_port,runtime->transport,[context](uint64_t& revision,std::string& cause){return live_host_state::read_time_authority(context,revision,cause);});
    runtime->day_port=std::make_unique<live_host_state::day::NativePort>(context,binding,fence);runtime->day_state=std::make_unique<live_host_state::day::Sync>(*runtime->day_port,*runtime->live_port,context,binding);if(!runtime->day_state->bootstrap(error))return false;live_state->bind_day(*runtime->day_state);runtime->economy_port=std::make_unique<live_host_state::day::economy::NativePort>(*runtime->day_port,fence,live_host_state::day::Target{});runtime->economy_state=std::make_unique<live_host_state::day::economy::Coupling>(*runtime->day_state,*runtime->economy_port);live_state->bind_economy(*runtime->economy_state);
    runtime->parties=party_frame::make_native(*runtime->live_port);
    if(!live_state->bind_parties(*runtime->parties,error))return false;
    runtime->vip_presentation=std::make_unique<scenario_vip_authority::FrameAdapter>(&vip_alert_authorize);
    if(!live_state->bind_vip(*runtime->vip_presentation,error))return false;
    runtime->release=std::make_unique<release_runtime::LivePort>();
    if(!live_state->bind_release(*runtime->release,error))return false;
    runtime->management=std::make_unique<management_runtime::LivePort>();
    if(!live_state->bind_management(*runtime->management,error))return false;
    runtime->marketing=std::make_unique<marketing_authority::NativePort>();
    if(!live_state->bind_marketing(*runtime->marketing,error))return false;
    if(!live_state->bind_monsters(npc_probe::monster_action_runtime::live_port(),error))return false;
    runtime->live_state=std::move(live_state);
    if(context.role==action::Role::Host)live_host_state::day::bind_host_journal(runtime->day_state.get());runtime->engine.bind_live_state(*runtime->live_state,*runtime->live_port);return true;
}
bool read_character_membership_actors(const action::Binding& binding,const action::Request& request,json& economy,json& monsters,std::string& error){
    economy=monsters=nullptr;auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring||!runtime->economy_state||!runtime->live_state||!runtime->live_state->last()){
        error="Character membership current actor read requires active bound native providers and Frame lineage";return false;}
    const auto controller=runtime->controller.status();action::Context context;
    if(!controller.started||controller.locked||!controller.pending||controller.action_uuid!=request.action_uuid||(request.action!=action::Action::EditCharacterMembership&&request.action!=action::Action::EditAbilityMembership)){
        error="Character/Ability actor read requires its exact pending authoring action";return false;}
    if(!runtime->session.current(context,error))return false;
    if(context!=runtime->controller.bound_context()||binding!=runtime->controller.bound_binding()||context.snapshot_uuid!=request.snapshot_uuid){
        error="Character membership current actor Session/Binding/snapshot differs";return false;}
    if(!runtime->engine.current(binding,request.snapshot_uuid,error))return false;
    live_host_state::day::economy::State actual;std::vector<uint8_t> bytes;
    if(!runtime->economy_state->read_current(*runtime->live_state->last(),actual,error)||!live_host_state::day::economy::encode(actual,bytes,error))return false;
    live_host_state::monsters::State current;
    if(!npc_probe::monster_action_runtime::live_port().read_current(context,binding,net::current_world_epoch(),current,error))return false;
    try{economy=json::parse(bytes);monsters=live_host_state::monsters::encode(current);}
    catch(const std::exception& exception){error=std::string("Character membership actual actor serialization: ")+exception.what();return false;}
    return runtime->engine.current(binding,request.snapshot_uuid,error);
}
bool read_intervention_live(const action::Context& expected,const action::Binding& binding,json& economy,json& monsters,json& marketing,std::string& error){
    economy=monsters=marketing=nullptr;auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring||!runtime->economy_state||!runtime->marketing||!runtime->live_state||!runtime->live_state->last()){
        error="Intervention current values require bound native providers and actual Frame lineage";return false;}
    const auto controller=runtime->controller.status();action::Context context;
    if(!controller.started||controller.locked){error="Intervention current values require a started unlocked Controller";return false;}
    if(!runtime->session.current(context,error))return false;
    if(context!=expected||context!=runtime->controller.bound_context()||binding!=runtime->controller.bound_binding()){
        error="Intervention current Session/Controller Binding differs";return false;}
    if(!runtime->engine.current(binding,context.snapshot_uuid,error))return false;
    live_host_state::day::economy::State actual;std::vector<uint8_t> bytes;
    if(!runtime->economy_state->read_current(*runtime->live_state->last(),actual,error)||!live_host_state::day::economy::encode(actual,bytes,error))return false;
    live_host_state::monsters::State current;
    if(!npc_probe::monster_action_runtime::live_port().read_current(context,binding,net::current_world_epoch(),current,error))return false;
    marketing_authority::State current_marketing;
    if(!runtime->marketing->read_current(context,binding,*runtime->live_state->last(),current_marketing,error))return false;
    try{economy=json::parse(bytes);monsters=live_host_state::monsters::encode(current);marketing=current_marketing.values;}
    catch(const std::exception& exception){error=std::string("Intervention actual actor serialization: ")+exception.what();return false;}
    return runtime->engine.current(binding,context.snapshot_uuid,error);
}
bool live_state_active() {auto* runtime=published_runtime.load(std::memory_order_acquire);return runtime&&runtime->live_state!=nullptr;}
bool construction_jobs(const action::Context& expected,const action::Binding& binding,
                       std::vector<developer_staff::construction::LocalJob>& out,std::string& error){
    out.clear();auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Construction local Jobs require current bound Mainthread owner";return false;}
    action::Context context;if(!runtime->session.current(context,error))return false;
    if(context!=expected||context!=runtime->controller.bound_context()||binding!=runtime->controller.bound_binding()){
        error="Construction local Jobs current Session/Controller Binding differs";return false;}
    return runtime->engine.construction_jobs(binding,context.snapshot_uuid,out,error);
}
bool bug_parent_alive(const action::Context& expected,const action::Binding& binding,const std::string& key,
                      bool& known,bool& alive,std::string& error){
    known=alive=false;auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Bug parent requires current bound Mainthread owner";return false;}
    action::Context context;if(!runtime->session.current(context,error))return false;
    if(context!=expected||context!=runtime->controller.bound_context()||binding!=runtime->controller.bound_binding()){
        error="Bug parent current Session/Controller Binding differs";return false;}
    return runtime->engine.bug_parent_alive(binding,context.snapshot_uuid,key,known,alive,error);
}
bool bug_context(action::Context& context,action::Binding& binding,uint64_t& publication,bool& active,std::string& error){
    active=false;context={};binding={};publication=0;
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||!runtime->live_state||runtime->lobby_retiring)return true;
    if(runtime->thread!=GetCurrentThreadId()){error="Bug authority requires Controller Mainthread";return false;}
    if(!runtime->session.current(context,error))return false;
    if(context!=runtime->controller.bound_context()){error="Bug authority current Session differs from Controller";return false;}
    binding=runtime->controller.bound_binding();
    if(!runtime->engine.current(binding,context.snapshot_uuid,error))return false;
    publication=runtime->live_state->next_publication_revision();
    if(!publication){error="Bug authority publication revision overflow";return false;}
    active=true;return true;
}
bool read_binding(const std::string& snapshot,action::Binding& out,std::string& error){
    out={};auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="binding read requires bound Controller Mainthread";return false;}
    const auto state=runtime->controller.status();
    if(!state.started||state.locked||runtime->lobby_retiring){error="binding read requires started unlocked Controller";return false;}
    action::Context context;if(!runtime->session.current(context,error))return false;
    if(context!=runtime->controller.bound_context()||context.snapshot_uuid!=snapshot){error="binding read actual Session/snapshot differs from Controller";return false;}
    const auto binding=runtime->controller.bound_binding();
    if(!runtime->engine.current(binding,snapshot,error))return false;
    out=binding;error.clear();return true;
}
bool network_binding(const std::string& snapshot,action::Binding& out,std::string& error){
    out={};auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="network admission requires bound Controller Mainthread";return false;}
    const auto state=runtime->controller.status();
    if(!state.started||state.locked||state.pending||runtime->lobby_retiring){error="network admission requires idle unlocked Controller";return false;}
    return read_binding(snapshot,out,error);
}
bool observe_startingpoint_submission(const action::Context& expected_context,const action::Binding& expected_binding,
                                     const std::string& id,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="startingpoint submission observation requires bound Controller Mainthread";return false;}
    const auto state=runtime->controller.status();
    if(!state.started||state.locked||!state.pending||runtime->lobby_retiring||state.outcome!=action::Outcome::Pending||
       id.empty()||state.action_uuid!=id||!state.cause.empty()){
        error="startingpoint submission observation requires exact pending unlocked action: "+state.cause;return false;}
    if(!runtime->live_state){error="startingpoint submission observation live authority unavailable";return false;}
    const auto receipt=runtime->live_state->receipt();const auto& fence=live_host_state::economic_fence();
    if(runtime->live_state->locked()||receipt.unknown||fence.locked()||!runtime->live_error.empty()){
        error="startingpoint submission observation live authority locked/Unknown: "+runtime->live_error+
            " receipt="+receipt.cause+" fence="+fence.cause();return false;}
    action::Context actual;if(!runtime->session.current(actual,error))return false;
    if(expected_context.role!=action::Role::Client||actual!=expected_context||actual!=runtime->controller.bound_context()||
       expected_binding!=runtime->controller.bound_binding()){
        error="startingpoint submission observation immutable Controller Context/binding differs";return false;}
    return runtime->engine.current(expected_binding,expected_context.snapshot_uuid,error);
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
    if(runtime->live_state&&!net::current_world_epoch()){
        const auto failure=net::snapshot_progress().value("first_failure",std::string{});
        if(!failure.empty()&&!runtime->live_failure_terminal){
            runtime->live_failure_terminal=true;runtime->live_error=failure;
            LOG_ERROR("[LiveState] Shared world ended: {}",failure);
        }
    }
    if(runtime->live_failure_terminal){
        return;
    }
    if(runtime->live_state&&!runtime->controller.status().locked) {
        live_host_state::apply_timing::Scope measured_inner_TypedLiveState(live_host_state::apply_timing::Part::TypedLiveState);
        std::string cause;const auto state=runtime->controller.status();auto& fence=live_host_state::economic_fence();
        const bool applied=!fence.locked()&&runtime->live_state->tick(state.host_revision,state.pending,cause);
        if(!applied||fence.locked()){
            if(cause.empty())cause=fence.cause();
            if(cause.empty())cause="live authoritative state failed without cause";
            runtime->live_failure_terminal=true;runtime->live_error=cause;
            LOG_ERROR("[LiveState] Shared world failed: {}",cause);
            const auto context=runtime->controller.bound_context();
            if(context.role==action::Role::Host){
                std::string notice;
                if(!net::fail_host_world(context.snapshot_uuid,cause,notice))LOG_ERROR("[LiveState] Host failure notification not confirmed: {}; retained_failure={}",notice,cause);
            }
            return;
        }
    }
    if(runtime->live_state){
        const auto& wait=runtime->live_state->publication_wait();
        if(wait!=runtime->live_publication_wait){
            if(!wait.empty())LOG_INFO("[LiveState] {}",wait);
            else if(!runtime->live_publication_wait.empty())LOG_INFO("[LiveState] Native save publication wait ended");
            runtime->live_publication_wait=wait;
        }
    }
    {live_host_state::apply_timing::Scope measured_inner_TypedWeaponBootstrap(live_host_state::apply_timing::Part::TypedWeaponBootstrap);weapon_bootstrap(*runtime);}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiMove(live_host_state::apply_timing::Part::TypedUiMove);ui_move_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiRegion(live_host_state::apply_timing::Part::TypedUiRegion);ui_region_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiPrice(live_host_state::apply_timing::Part::TypedUiPrice);ui_price_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiRelease(live_host_state::apply_timing::Part::TypedUiRelease);release_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiGeneralProject(live_host_state::apply_timing::Part::TypedUiGeneralProject);general_project_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiLogo(live_host_state::apply_timing::Part::TypedUiLogo);logo_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiManagement(live_host_state::apply_timing::Part::TypedUiManagement);management_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiDeveloper(live_host_state::apply_timing::Part::TypedUiDeveloper);developer_staff::action_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiBug(live_host_state::apply_timing::Part::TypedUiBug);bug_authority::manual_action::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiNpc(live_host_state::apply_timing::Part::TypedUiNpc);npc_probe::action_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiMonsterZone(live_host_state::apply_timing::Part::TypedUiMonsterZone);npc_probe::zone_action_runtime::tick();npc_probe::zone_edit_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiMonster(live_host_state::apply_timing::Part::TypedUiMonster);npc_probe::monster_action_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiQuestAuthoring(live_host_state::apply_timing::Part::TypedUiQuestAuthoring);npc_probe::monster_action_runtime::quest_authoring::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiQuestList(live_host_state::apply_timing::Part::TypedUiQuestList);npc_probe::monster_action_runtime::quest_list::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiCharacterMembership(live_host_state::apply_timing::Part::TypedUiCharacterMembership);npc_probe::monster_action_runtime::character_membership::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiCharacterRename(live_host_state::apply_timing::Part::TypedUiCharacterRename);npc_probe::monster_action_runtime::character_rename::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiAbilityMembership(live_host_state::apply_timing::Part::TypedUiAbilityMembership);npc_probe::monster_action_runtime::ability_membership::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiAbilityEdit(live_host_state::apply_timing::Part::TypedUiAbilityEdit);npc_probe::monster_action_runtime::ability_edit::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiCostumeEdit(live_host_state::apply_timing::Part::TypedUiCostumeEdit);npc_probe::monster_action_runtime::costume_edit::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiDungeonConfig(live_host_state::apply_timing::Part::TypedUiDungeonConfig);npc_probe::monster_action_runtime::dungeon_config::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiDungeonSound(live_host_state::apply_timing::Part::TypedUiDungeonSound);npc_probe::monster_action_runtime::dungeon_sound::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiDungeonGizmo(live_host_state::apply_timing::Part::TypedUiDungeonGizmo);npc_probe::monster_action_runtime::dungeon_gizmo::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiDungeonPalette(live_host_state::apply_timing::Part::TypedUiDungeonPalette);npc_probe::monster_action_runtime::dungeon_palette::tick();}
    npc_probe::monster_action_runtime::dungeon_input::tick();
    {live_host_state::apply_timing::Scope measured_inner_TypedUiDungeonLight(live_host_state::apply_timing::Part::TypedUiDungeonLight);npc_probe::monster_action_runtime::dungeon_light::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiBuildingStatus(live_host_state::apply_timing::Part::TypedUiBuildingStatus);npc_probe::monster_action_runtime::building_status::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiTerrain(live_host_state::apply_timing::Part::TypedUiTerrain);ui_terrain_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiPath(live_host_state::apply_timing::Part::TypedUiPath);ui_path_runtime::tick();}
    {live_host_state::apply_timing::Scope measured_inner_TypedUiNetwork(live_host_state::apply_timing::Part::TypedUiNetwork);ui_network_runtime::tick();}
}
void tick_after_transport() {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->lobby_retiring||!runtime->controller.status().started)return;
    std::string error;
    live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::TypedController);
    if(!runtime->controller.tick(GetTickCount64(),error)&&error.empty())LOG_ERROR("[Action] Tick failed without cause");
}
bool resolve_held_dungeon_destination(uintptr_t pointer,std::string& key,bool& matched,std::string& error,std::string* scene){
    if(scene)scene->clear();
    key.clear();matched=false;
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||!runtime->live_state){error.clear();return true;}
    if(runtime->thread!=GetCurrentThreadId()){
        error="Held Dungeon destination requires the bound Mainthread";return false;
    }
    return runtime->live_state->resolve_held_dungeon_destination(pointer,key,matched,error,scene);
}
json controller_status() {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime)return {{"initialized",false}};
    if(runtime->thread!=GetCurrentThreadId())return {{"initialized",false},{"error","typed action status requires bound Mainthread"}};
    const auto s=runtime->controller.status();
    // Frame guards need Controller receipts, not a diagnostic rescan of the whole economy.
    return {{"initialized",true},{"lobby_retiring",runtime->lobby_retiring},{"live_failure_terminal",runtime->live_failure_terminal},{"started",s.started},{"locked",s.locked},
        {"pending",s.pending||runtime->lobby_retiring},{"host_revision",s.host_revision},{"action_uuid",s.action_uuid},
        {"outcome",int(s.outcome)},{"cause",s.cause},{"live_error",runtime->live_error}};
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
    return {{"initialized",true},{"weapon_visual",weapon_status(*runtime)},{"bug_manual",bug_authority::manual_action::status()},{"ui_price",ui_price_runtime::status()},{"ui_release",release_runtime::status()},{"general_project",general_project_runtime::status()},{"ui_logo",logo_runtime::status()},{"ui_management",management_runtime::status()},{"developer_staff",developer_staff::action_runtime::status()},{"ui_npc",npc_probe::action_runtime::status()},{"ui_monster_zone",npc_probe::zone_action_runtime::status()},{"ui_zone_edit",npc_probe::zone_edit_runtime::status()},{"ui_monster",npc_probe::monster_action_runtime::status()},{"ui_npc_quest",npc_probe::monster_action_runtime::quest_authoring::status()},{"ui_npc_quest_list",npc_probe::monster_action_runtime::quest_list::status()},{"ui_character_membership",npc_probe::monster_action_runtime::character_membership::status()},{"ui_character_rename",npc_probe::monster_action_runtime::character_rename::status()},{"ui_ability_membership",npc_probe::monster_action_runtime::ability_membership::status()},{"ui_ability_edit",npc_probe::monster_action_runtime::ability_edit::status()},{"ui_costume_edit",npc_probe::monster_action_runtime::costume_edit::status()},{"ui_dungeon_sound",npc_probe::monster_action_runtime::dungeon_sound::status()},{"ui_dungeon_gizmo",npc_probe::monster_action_runtime::dungeon_gizmo::status()},{"ui_dungeon_palette",npc_probe::monster_action_runtime::dungeon_palette::status()},{"ui_dungeon_input",npc_probe::monster_action_runtime::dungeon_input::status()},{"ui_dungeon_light",npc_probe::monster_action_runtime::dungeon_light::status()},{"ui_dungeon_config",npc_probe::monster_action_runtime::dungeon_config::status()},{"ui_building_status",npc_probe::monster_action_runtime::building_status::status()},{"lobby_retiring",runtime->lobby_retiring},{"live_failure_terminal",runtime->live_failure_terminal},{"started",s.started},{"locked",s.locked},{"pending",s.pending||runtime->lobby_retiring},
        {"host_revision",s.host_revision},{"attempted",s.attempted},{"confirmed",s.confirmed},{"failed",s.failed},
        {"duplicates",s.duplicates},{"suppressed_errors",s.suppressed_errors},{"required_mask",s.required_mask},{"ack_mask",s.ack_mask},
        {"outcome",int(s.outcome)},{"action_uuid",s.action_uuid},{"cause",s.cause},{"last_packet_accepted",s.last_packet_accepted},
        {"subscriber_additions",runtime->economy_state?runtime->economy_state->addition_receipt():nlohmann::json(nullptr)},
        {"frame_transport",runtime->live_state?runtime->live_state->frame_transport_status():nlohmann::json(nullptr)},
        {"toon_time",toon_time_status(*runtime)},
        {"vip_authority",vip_status(*runtime)},
        {"release_frame",runtime->live_state?runtime->live_state->release_status():nlohmann::json(nullptr)},
        {"management_frame",runtime->live_state?runtime->live_state->management_status():nlohmann::json(nullptr)},
        {"marketing_frame",runtime->live_state?runtime->live_state->marketing_status():nlohmann::json(nullptr)},
        {"monster_frame",runtime->live_state?runtime->live_state->monster_status():nlohmann::json(nullptr)},
        {"vip_frame",runtime->live_state?runtime->live_state->vip_status():nlohmann::json(nullptr)},
        {"vip_presentation",runtime->vip_presentation?runtime->vip_presentation->status():nlohmann::json(nullptr)},
        {"network_receipt",runtime->live_state?live_host_state::network::encode_receipt(runtime->live_state->network_receipt()):nlohmann::json::object()},
        {"network_activation_receipt",runtime->live_state?runtime->live_state->network_activation_receipt().status():nlohmann::json(nullptr)},
        {"network_frame_revision",runtime->live_state&&runtime->live_state->last()?runtime->live_state->last()->revision:0},
        {"network_target",runtime->live_state&&runtime->live_state->last()&&runtime->live_state->last()->network?live_host_state::network::encode(*runtime->live_state->last()->network):nlohmann::json(nullptr)},
        {"native_economy",native_economy},{"native_economy_read_error",economy_read_error},
        {"quest_completion",live_host_state::day::economy::quest_completion_status()},
        {"economy_receipt",runtime->economy_state?nlohmann::json{{"attempted",runtime->economy_state->receipt().attempted},{"confirmed",runtime->economy_state->receipt().confirmed},{"unknown",runtime->economy_state->receipt().unknown},{"attempted_stores",runtime->economy_state->receipt().attempted_stores},{"completed_stores",runtime->economy_state->receipt().stores},{"cause",runtime->economy_state->receipt().cause}}:nlohmann::json::object()},
        {"live_state",{{"requested",runtime->live_attempted},{"active",runtime->live_state!=nullptr},{"locked",(runtime->live_state&&runtime->live_state->locked())||live_host_state::economic_fence().locked()},
            {"publication_wait",runtime->live_state?runtime->live_state->publication_wait():std::string{}},
            {"publication_wait_receipt",runtime->live_state?runtime->live_state->publication_wait_status():nlohmann::json(nullptr)},
            {"day_record_sequence",runtime->day_state?runtime->day_state->sequence():0},{"day_receipt",runtime->day_state?nlohmann::json{{"attempted",runtime->day_state->day_receipt().attempted},{"confirmed",runtime->day_state->day_receipt().confirmed},{"unknown",runtime->day_state->day_receipt().unknown},{"created",runtime->day_state->day_receipt().created_records},{"appended",runtime->day_state->day_receipt().appended_records},{"pruned",runtime->day_state->day_receipt().pruned_records},{"cause",runtime->day_state->day_receipt().cause}}:nlohmann::json::object()},
            {"economic_fence_locked",live_host_state::economic_fence().locked()},{"economic_fence_cause",live_host_state::economic_fence().cause()},{"economic_retired_cause",live_host_state::economic_fence().retired_cause()},{"economic_suppressed",live_host_state::economic_fence().suppressed()},
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
    const auto id=args.at("action_uuid").get<std::string>(),key=args.at("record_key").get<std::string>();
    remove_diagnostics::LocalTarget target;std::string scene;bool dungeon=false;
    if(!remove_diagnostics::resolve_record_target(net::current_snapshot_id(),key,target,error)||
       !remove_diagnostics::dungeon_building_scene(target,scene,dungeon,error))return false;
    const bool accepted=dungeon?
        runtime->controller.submit_dungeon_building(id,action::Action::RemoveSnapshotBuilding,key,scene,{}, {},GetTickCount64(),error):
        runtime->controller.submit_remove(id,key,GetTickCount64(),error);
    out=status();return accepted;
}
bool submit_dungeon_building(const std::string& id,action::Action kind,const std::string& key,
    const std::string& scene,const action::BuildingState& value,const std::string& target_action,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Dungeon Building requires the bound game-thread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Dungeon Building not attempted";return false;}
    return runtime->controller.submit_dungeon_building(id,kind,key,scene,value,target_action,GetTickCount64(),error);
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
    remove_diagnostics::LocalTarget target;std::string scene;bool dungeon=false;
    if(!remove_diagnostics::resolve_record_target(net::current_snapshot_id(),key,target,error)||
       !remove_diagnostics::dungeon_building_scene(target,scene,dungeon,error))return false;
    return dungeon?runtime->controller.submit_dungeon_building(id,action::Action::MoveBuilding,key,scene,state,{},GetTickCount64(),error):
        runtime->controller.submit_move(id,key,state,GetTickCount64(),error);
}
bool submit_building_history(action::Action action,const std::string& id,const std::string& target,std::string& error) {return submit_building_history(action,id,target,"action:"+target,error);}
bool submit_building_history(action::Action action,const std::string& id,const std::string& target,const std::string& key,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="building history requires started game-thread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; building history not attempted";return false;}
    std::string scene;bool dungeon=false;
    if(!runtime->engine.dungeon_history_scene(target,key,scene,dungeon,error))return false;
    return dungeon?runtime->controller.submit_dungeon_building(id,action,key,scene,{},target,GetTickCount64(),error):
        runtime->controller.submit_building_history(action,id,target,key,GetTickCount64(),error);
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
bool submit_world_path(const std::string& id,const action::WorldPathIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){
        error="Wall/Pass requires active Mainthread Controller";return false;}
    return runtime->controller.submit_world_path(id,intent,GetTickCount64(),error);
}
bool submit_building_status(const std::string& id,const action::BuildingStatusIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Building status requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Building status not attempted";return false;}
    return runtime->controller.submit_building_status(id,intent,GetTickCount64(),error);
}
bool submit_dungeon_light(const std::string& id,const action::DungeonLightIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Dungeon light edit requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Dungeon light edit not attempted";return false;}
    return runtime->controller.submit_dungeon_light(id,intent,GetTickCount64(),error);
}
bool submit_dungeon_gizmo(const std::string& id,const action::DungeonGizmoIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Gizmo edit requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Gizmo edit not attempted";return false;}
    return runtime->controller.submit_dungeon_gizmo(id,intent,GetTickCount64(),error);
}
bool submit_dungeon_input(const std::string& id,const action::DungeonInputIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Dungeon input requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Dungeon input not attempted";return false;}
    return runtime->controller.submit_dungeon_input(id,intent,GetTickCount64(),error);
}
bool submit_dungeon_palette(const std::string& id,const action::DungeonPaletteIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Dungeon palette edit requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Dungeon palette edit not attempted";return false;}
    return runtime->controller.submit_dungeon_palette(id,intent,GetTickCount64(),error);
}
bool submit_dungeon_sound(const std::string& id,const action::DungeonSoundIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Dungeon sound edit requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Dungeon sound edit not attempted";return false;}
    return runtime->controller.submit_dungeon_sound(id,intent,GetTickCount64(),error);
}
bool submit_dungeon_config(const std::string& id,const action::DungeonConfigIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Dungeon config edit requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Dungeon config edit not attempted";return false;}
    return runtime->controller.submit_dungeon_config(id,intent,GetTickCount64(),error);
}
bool submit_costume_edit(const std::string& id,const action::CostumeEditIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Costume edit requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Costume edit not attempted";return false;}
    return runtime->controller.submit_costume_edit(id,intent,GetTickCount64(),error);
}
bool submit_ability_membership(const std::string& id,const action::AbilityMembershipIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Ability membership requires active Mainthread Controller";return false;}
    return runtime->controller.submit_ability_membership(id,intent,GetTickCount64(),error);
}
bool submit_ability_edit(const std::string& id,const action::AbilityEditIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Ability edit requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Ability edit not attempted";return false;}
    return runtime->controller.submit_ability_edit(id,intent,GetTickCount64(),error);
}
bool submit_character_membership(const std::string& id,const action::CharacterMembershipIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Character membership requires active Mainthread Controller";return false;}
    return runtime->controller.submit_character_membership(id,intent,GetTickCount64(),error);
}
bool submit_character_rename(const std::string& id,const action::CharacterRenameIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Character rename requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Character rename not attempted";return false;}
    return runtime->controller.submit_character_rename(id,intent,GetTickCount64(),error);
}
bool submit_npc_quest_list(const std::string& id,const std::string& owner,const action::NpcQuestListIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Quest list requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Quest list not attempted";return false;}
    return runtime->controller.submit_npc_quest_list(id,owner,intent,GetTickCount64(),error);
}
bool submit_npc_quest(const std::string& id,const std::string& owner,const action::NpcQuestIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Quest edit requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Quest edit not attempted";return false;}
    return runtime->controller.submit_npc_quest(id,owner,intent,GetTickCount64(),error);
}
bool submit_remove_monster(const std::string& id,const std::string& target,const action::MonsterRemoveIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Monster Remove requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Monster Remove not attempted";return false;}
    return runtime->controller.submit_remove_monster(id,target,intent,GetTickCount64(),error);
}
bool submit_move_monster(const std::string& id,const std::string& target,const action::MonsterMoveIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Monster Move requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Monster Move not attempted";return false;}
    return runtime->controller.submit_move_monster(id,target,intent,GetTickCount64(),error);
}
bool submit_monster(const std::string& id,const action::MonsterIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Monster requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Monster not attempted";return false;}
    return runtime->controller.submit_monster(id,intent,GetTickCount64(),error);
}
bool submit_zone(const std::string& id,const action::ZoneIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Zone requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Zone not attempted";return false;}
    return runtime->controller.submit_zone(id,intent,GetTickCount64(),error);
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
bool submit_bug_fix(const std::string& id,const action::BugFixIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Bug Fix requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Bug Fix not attempted";return false;}
    return runtime->controller.submit_bug_fix(id,intent,GetTickCount64(),error);
}
bool submit_developer(bool hire,const std::string& id,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Developer Hire/Fire requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Developer Hire/Fire not attempted";return false;}
    return runtime->controller.submit_developer(hire,id,GetTickCount64(),error);
}
bool submit_start_research(const std::string& id,const std::string& key,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Research requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Research not attempted";return false;}
    return runtime->controller.submit_start_research(id,key,GetTickCount64(),error);
}
bool submit_research_workers(const std::string& id,const std::string& key,int32_t desired,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Research requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Research not attempted";return false;}
    return runtime->controller.submit_research_workers(id,key,desired,GetTickCount64(),error);
}
bool submit_commit_research(const std::string& id,const std::string& key,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="Research requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Research not attempted";return false;}
    return runtime->controller.submit_commit_research(id,key,GetTickCount64(),error);
}
bool submit_startingpoint_workers(const std::string& id,int32_t desired,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="startingpoint staff requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Start not attempted";return false;}
    return runtime->controller.submit_startingpoint_workers(id,desired,GetTickCount64(),error);
}
bool submit_start_startingpoint(const std::string& id,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="startingpoint Start requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Start not attempted";return false;}
    return runtime->controller.submit_start_startingpoint(id,GetTickCount64(),error);
}
bool submit_commit_startingpoint(const std::string& id,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="startingpoint Commit requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; Commit not attempted";return false;}
    return runtime->controller.submit_commit_startingpoint(id,GetTickCount64(),error);
}
bool submit_intervention(const std::string& id,const action::InterventionIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Intervention submit requires current bound Mainthread";return false;}
    return runtime->controller.submit_intervention(id,intent,GetTickCount64(),error);
}
bool submit_game_rules(const std::string& id,const action::GameRulesIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Design rule submit requires active Mainthread Controller";return false;}
    return runtime->controller.submit_game_rules(id,intent,GetTickCount64(),error);
}
bool submit_gm_policy(const std::string& id,const action::GmPolicyIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="GM policy submit requires active Mainthread Controller";return false;}
    return runtime->controller.submit_gm_policy(id,intent,GetTickCount64(),error);
}
bool submit_release_cycle(const std::string& id,const action::ReleaseCycleIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Release cycle submit requires active Mainthread Controller";return false;}
    return runtime->controller.submit_release_cycle(id,intent,GetTickCount64(),error);
}
bool submit_release_edit(const std::string& id,const action::ReleaseEditIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="General project submit requires active Mainthread Controller";return false;}
    return runtime->controller.submit_release_edit(id,intent,GetTickCount64(),error);
}
bool submit_general_project(const std::string& id,const action::GeneralProjectIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="General project submit requires active Mainthread Controller";return false;}
    return runtime->controller.submit_general_project(id,intent,GetTickCount64(),error);
}
bool submit_logo(const std::string& id,const action::LogoIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Logo submit requires active Mainthread Controller";return false;}
    return runtime->controller.submit_logo(id,intent,GetTickCount64(),error);
}
bool submit_network_remove(const std::string& id,const std::string& key,const action::NetworkRemoveIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Network Remove submit requires active Mainthread Controller";return false;}
    return runtime->controller.submit_network_remove(id,key,intent,GetTickCount64(),error);
}
bool submit_management(const std::string& id,const action::ManagementIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Management action requires current Mainthread Controller";return false;}
    return runtime->controller.submit_management(id,intent,GetTickCount64(),error);
}
bool submit_prices(const std::string& id,const action::PriceIntent& intent,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="price action requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; price action not attempted";return false;}
    return runtime->controller.submit_prices(id,intent,GetTickCount64(),error);
}
bool submit_duplicate(const std::string& id,const action::DuplicateIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Duplicate requires active Mainthread Controller";return false;}
    return runtime->controller.submit_duplicate(id,intent,GetTickCount64(),error);
}
bool submit_boss_placement(const std::string& id,const action::BossPlacementIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring){error="Boss placement requires an active Mainthread Controller";return false;}
    return runtime->controller.submit_boss_placement(id,intent,GetTickCount64(),error);
}
bool submit_group_object(bool moving,const std::string& id,const action::GroupObjectIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Group action requires an active Mainthread Controller";return false;}
    return runtime->controller.submit_group_object(moving,id,intent,GetTickCount64(),error);
}
bool submit_modular(const std::string& id,const action::ModularIntent& intent,std::string& error){auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Modular action requires the active Mainthread Controller";return false;}return runtime->controller.submit_modular(id,intent,GetTickCount64(),error);}
bool submit_scenery_stroke(const std::string& id,const action::SceneryStrokeIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Scenery stroke requires an active Mainthread Controller";return false;}
    return runtime->controller.submit_scenery_stroke(id,intent,GetTickCount64(),error);
}
bool submit_selection_move(const std::string& id,const action::SelectionMoveIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Selection Move requires active Mainthread Controller";return false;}
    return runtime->controller.submit_selection_move(id,intent,GetTickCount64(),error);
}
bool submit_service_membership(const std::string& id,const std::string& key,const action::ServiceMembershipIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Service membership requires active Mainthread Controller";return false;}
    return runtime->controller.submit_service_membership(id,key,intent,GetTickCount64(),error);
}
bool submit_boss_phase(const std::string& id,const action::BossPhaseIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Boss phase requires active Mainthread Controller";return false;}
    return runtime->controller.submit_boss_phase(id,intent,GetTickCount64(),error);
}
bool submit_group(const std::string& id,const action::GroupIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Group requires active Mainthread Controller";return false;}
    return runtime->controller.submit_group(id,intent,GetTickCount64(),error);
}
bool submit_zone_edit(const std::string& id,const action::ZoneEditIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Zone edit requires active Mainthread Controller";return false;}
    return runtime->controller.submit_zone_edit(id,intent,GetTickCount64(),error);
}
bool submit_district_stroke(const std::string& id,const action::DistrictStrokeIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="District stroke requires active Mainthread Controller";return false;}
    return runtime->controller.submit_district_stroke(id,intent,GetTickCount64(),error);
}
bool submit_district_property(const std::string& id,const std::string& key,const game::district_property::Value& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="District title requires active Mainthread Controller";return false;}
    return runtime->controller.submit_district_property(id,key,intent,GetTickCount64(),error);
}
bool submit_district_title(const std::string& id,const std::string& key,const action::DistrictTitleIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="District title requires active Mainthread Controller";return false;}
    return runtime->controller.submit_district_title(id,key,intent,GetTickCount64(),error);
}
bool submit_district_text(const std::string& id,const std::string& key,const action::DistrictTextIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="District text requires active Mainthread Controller";return false;}
    return runtime->controller.submit_district_text(id,key,intent,GetTickCount64(),error);
}
bool submit_prop_text(const std::string& id,const std::string& key,const action::PropTextIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||GetCurrentThreadId()!=runtime->thread||runtime->lobby_retiring){error="Prop text requires active Mainthread Controller";return false;}
    return runtime->controller.submit_prop_text(id,key,intent,GetTickCount64(),error);
}
bool submit_npc_property(const std::string& id,const std::string& key,const action::NpcPropertyIntent& intent,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="NPC property requires started Mainthread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; NPC property not attempted";return false;}
    return runtime->controller.submit_npc_property(id,key,intent,GetTickCount64(),error);
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
static bool begin_retirement(bool leave_lobby,std::string& e){
    const auto current=status();
    const auto build=ui_build_runtime::status(),remove=ui_remove_runtime::status(),move=ui_move_runtime::status(),region=ui_region_runtime::status(),price=ui_price_runtime::status();
    if(build.value("deferred_build_capture",false)||remove.value("replay_active",false)||remove.value("inflight",uint64_t{})||move.value("pending_ui",false)||region.value("pending_ui",false)||price.value("pending_ui",false)||ui_path_runtime::world_path_pending()||release_runtime::pending_ui()||general_project_runtime::pending_ui()||logo_runtime::pending_ui()||management_runtime::pending_ui()||npc_probe::action_runtime::pending_ui()||developer_staff::action_runtime::pending_ui()||npc_probe::zone_action_runtime::pending_ui()||npc_probe::zone_edit_runtime::pending()||npc_probe::monster_action_runtime::pending_ui()||npc_probe::monster_action_runtime::quest_authoring::pending_ui()||npc_probe::monster_action_runtime::quest_list::pending_ui()||npc_probe::monster_action_runtime::character_membership::pending_ui()||npc_probe::monster_action_runtime::character_rename::pending_ui()||npc_probe::monster_action_runtime::ability_membership::pending_ui()||npc_probe::monster_action_runtime::ability_edit::pending_ui()||npc_probe::monster_action_runtime::costume_edit::pending_ui()||npc_probe::monster_action_runtime::dungeon_config::pending_ui()||npc_probe::monster_action_runtime::dungeon_sound::pending_ui()||npc_probe::monster_action_runtime::dungeon_gizmo::pending_ui()||npc_probe::monster_action_runtime::dungeon_palette::pending_ui()||npc_probe::monster_action_runtime::dungeon_input::pending_ui()||npc_probe::monster_action_runtime::dungeon_light::pending_ui()||npc_probe::monster_action_runtime::building_status::pending_ui()||bug_authority::manual_action::pending_ui()){e="native lobby Leave blocked by retained active/pending UI capture intent";return false;}
    if(current.value("snapshot_handoff",false)||!current.value("snapshot_error",std::string{}).empty()){e="native lobby Leave blocked by current snapshot handoff: "+current.value("snapshot_error",std::string{});return false;}
    auto* r=published_runtime.load(std::memory_order_acquire);
    if(r){const auto s=r->controller.status();if(r->thread!=GetCurrentThreadId()||!r->controller.can_retire_lobby()||r->lobby_retiring){e="lobby Leave requires idle actual Controller; pending/Unknown/retiring cause="+s.cause;return false;}
        r->lobby_retiring=true;net::session_wire::suspend_actions();
        if(r->live_state){
            if(!r->live_state->suspend_lobby(e)){if(r->live_error.empty())r->live_error=e;return false;}
            if(!e.empty()){LOG_WARN("[VIPPresentation] {}",e);e.clear();}
        }
        if(r->live_port&&!live_host_state::economic_fence().suspend_lobby(e)){if(r->live_error.empty())r->live_error=e;return false;}
    }else net::session_wire::suspend_actions();
    time_integration::Status time;
    if(time_integration::instance().status(time,e)){
        if(!time_integration::instance().suspend("native lobby Leave",e))return false;
    }else if(e!="time_integration_not_initialized")return false;
    if(leave_lobby)net::chat::Runtime::instance().set_lobby_admitted(false);
    net::invalidate_snapshot_world("native lobby Leave");e.clear();return true;
}
static bool finish_retirement(bool leave_lobby,bool& ready,std::string& e){
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
    if(leave_lobby){
        net::SteamMatchmaking::instance().leave_lobby();
        if(net::SteamMatchmaking::instance().in_lobby()){e="native lobby Leave local membership readback still present";return false;}
        if(!net::session_wire::retire_lobby(e))return false;
    }else if(!net::session_wire::retire_world(e))return false;
    tutorial_replication::retire("confirmed native Title after lobby Leave");
    if(!release_runtime::retire_lobby(e)||!general_project_runtime::retire_lobby(e)||!logo_runtime::retire_lobby(e)||!management_runtime::retire_lobby(e))return false;
    if(!developer_staff::action_runtime::retire_lobby(e)||!bug_authority::manual_action::retire_lobby(e)||!npc_probe::zone_edit_runtime::retire_session(e))return false;
    if(!npc_probe::monster_action_runtime::quest_list::retire_lobby(e)||
       !npc_probe::monster_action_runtime::character_membership::retire_lobby(e)||
       !npc_probe::monster_action_runtime::character_rename::retire_lobby(e)||
       !npc_probe::monster_action_runtime::ability_membership::retire_lobby(e)||
       !npc_probe::monster_action_runtime::ability_edit::retire_lobby(e)||
       !npc_probe::monster_action_runtime::building_status::retire_lobby(e)||
       !npc_probe::monster_action_runtime::costume_edit::retire_lobby(e)||
       !npc_probe::monster_action_runtime::dungeon_config::retire_lobby(e)||
       !npc_probe::monster_action_runtime::dungeon_light::retire_lobby(e)||
       !npc_probe::monster_action_runtime::dungeon_sound::retire_lobby(e)||
       !npc_probe::monster_action_runtime::dungeon_palette::retire_lobby(e)||
       !npc_probe::monster_action_runtime::dungeon_input::retire_lobby(e)||
       !npc_probe::monster_action_runtime::dungeon_gizmo::retire_lobby(e))return false;
    dungeon_scene_identity_runtime::invalidate();
    ui_path_runtime::retire_world();
    if(r&&r->vip_presentation)r->vip_presentation->retire_lobby();
    live_host_state::day::bind_host_journal(nullptr);published_runtime.store(nullptr,std::memory_order_release);delete r;
    ready=true;e.clear();return true;
}
}

namespace mt2mp::game::typed_action_runtime {
bool begin_lobby_leave(std::string& error){return begin_retirement(true,error);}
bool finish_lobby_leave(bool& ready,std::string& error){return finish_retirement(true,ready,error);}
bool begin_world_reload(std::string& error){return begin_retirement(false,error);}
bool finish_world_reload(bool& ready,std::string& error){return finish_retirement(false,ready,error);}
}

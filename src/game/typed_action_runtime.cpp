#include "mt2mp/net/chat_runtime.h"
#include "marketing_authority/native_port.h"
#include "bug_authority/runtime.h"
#include "bug_authority/manual_action.h"
#include "bug_authority/codec.h"
#include "typed_action_runtime.h"
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
#include "tutorial_replication/start_ledger.h"
#include "tutorial_replication/staff_action.h"
#include "ui_remove_runtime.h"
#include "ui_build_runtime.h"
#include "building_history_slice/native_bridge.h"
#include "building_history_slice/cash_effect.h"
#include "ui_move_runtime.h"
#include "ui_region_runtime.h"
#include "ui_price_runtime.h"
#include "developer_staff/action_native.h"
#include "developer_staff/construction_native.h"
#include "npc_probe.h"
#include "monster_zone_action_runtime.h"
#include "monster_action_runtime.h"
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
    struct Operation {action::Request request;action::Binding binding;action::Ledger before;json removal,retirement;action::BuildingState object_before;uint64_t local_uid=0;bool normal_scenery=false,history_ready=false;int32_t refund=0;};
    std::map<std::string,Operation> operations_;
    struct BuildOperation {action::Request request;action::Binding binding;action::Ledger before;action::BuildingState state;json receipt;bool retired=false;action::BuildingState object_before;bool normal_scenery=false;developer_staff::construction::LocalJob construction_job;bool construction_confirmed=false;};
    std::map<std::string,BuildOperation> buildings_;
    struct HistoryOperation {action::Request request;action::Binding binding;action::Ledger before,after;
        action::BuildingState object_before,object_after;uint64_t uid=0;bool confirmed=false,retired=false;};
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
    action::Outcome perform_remove(const action::Binding& binding,const action::Request& request,
        action_session::ui_capture::ReplayPurpose purpose,const action::Evidence* authority,std::string& error) {
        if(!current(binding,request.snapshot_uuid,error))return action::Outcome::RejectedNoEffect;
        if(operations_.contains(request.action_uuid)){error="typed native action token already attempted; no replay";return action::Outcome::Unknown;}
        action::Ledger before;if(!read_ledger(before,error))return action::Outcome::RejectedNoEffect;
        action::BuildingState object_before;uint64_t local_uid=0;bool normal_scenery=false;
        {
            remove_diagnostics::LocalTarget target;native::BuildingState actual;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
               !scenery_kiss::read_action_prop(target.uid,actual,error))return action::Outcome::RejectedNoEffect;
            object_before=world_state::building_state(actual);local_uid=target.uid;normal_scenery=target.normal_scenery;
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
        operation.normal_scenery=normal_scenery;
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
    bool bug_parent_alive(const action::Binding& binding,const std::string& snapshot,const std::string& key,
                          bool& known,bool& alive,std::string& error){
        known=alive=false;if(!current(binding,snapshot,error))return false;
        bug_authority::Row parent;parent.parent_kind=bug_authority::ParentKind::Building;parent.parent_key=key;
        if(!bug_authority::codec_detail::parent(parent)){error="Bug parent requires canonical Building key";return false;}
        const auto row=world_.rows().find(key);
        if(row!=world_.rows().end()){known=true;alive=row->second.state.alive;}
        error.clear();return true;
    }
    bool construction_jobs(const action::Binding& binding,const std::string& snapshot,
                           std::vector<developer_staff::construction::LocalJob>& out,std::string& error){
        out.clear();if(!current(binding,snapshot,error))return false;
        std::vector<developer_staff::construction::LocalJob> result;
        for(const auto& [id,op]:buildings_){
            if(!op.construction_job.job)continue;
            if(op.binding!=binding||op.request.snapshot_uuid!=snapshot||op.request.action!=action::Action::PlaceBuilding||
               op.request.action_uuid!=id||op.request.canonical_record_key!=op.construction_job.building_key||!op.construction_confirmed){
                error="Construction local Job does not belong to confirmed current PlaceBuilding";return false;}
            const auto row=world_.rows().find(op.construction_job.building_key);
            if(row==world_.rows().end()){error="Construction local Job has no current ordered Building row";return false;}
            auto job=op.construction_job;job.target_alive=row->second.state.alive;
            job.target_restored=false;
            if(job.target_alive)for(const auto& [history_id,history]:history_){
                const auto removed=operations_.find(history.request.target_action_uuid);
                const bool original_place=history.request.action==action::Action::RedoBuilding&&history.request.target_action_uuid==id;
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
            result.push_back(std::move(job));
        }
        out=std::move(result);error.clear();return true;
    }
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
        developer_staff::construction::PlacementBefore construction_before;
        if(!developer_staff::construction::capture_placement(construction_before,error))return action::Outcome::RejectedNoEffect;
        for(auto& [prior_id,prior]:buildings_){
            (void)prior_id;
            if(!prior.construction_confirmed||!prior.construction_job.job||prior.construction_job.retired)continue;
            bool present=false;for(const auto& job:construction_before.queue.jobs)if(job.pointer==prior.construction_job.job){present=true;break;}
            if(!present)prior.construction_job.retired=true;
        }
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
        if(!developer_staff::construction::finish_placement(construction_before,request.snapshot_uuid,
                request.canonical_record_key,uid,operation.construction_job,error))return action::Outcome::Unknown;
        operation.normal_scenery=target.normal_scenery;
        world_state::LocalBinding observed;
        if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error))return action::Outcome::Unknown;
        if(observed.state!=operation.state){error="native building state changed before ID binding";return action::Outcome::Unknown;}
        if(authority&&operation.state!=authority->object_after){error="native client building differs from Host result";return action::Outcome::Unknown;}
        change.after=operation.state;
        if(world_.commit(world_.origin(),change,observed,error)!=world_state::Admission::New)return action::Outcome::Unknown;
        operation.construction_confirmed=true;
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
        action::BuildingState* source_state=nullptr;uint64_t uid=0;int32_t amount=0;bool source_remove=false;
        const auto placed=buildings_.find(request.target_action_uuid);
        const auto removed=operations_.find(request.target_action_uuid);
        if(placed!=buildings_.end()&&removed!=operations_.end()) {
            error="building history target mutation is ambiguous";return action::Outcome::RejectedNoEffect;
        }
        if(placed!=buildings_.end()) {
            auto& source=placed->second;
            if(source.request.action!=action::Action::PlaceBuilding||source.request.action_uuid!=request.target_action_uuid||source.binding!=binding||
               source.request.snapshot_uuid!=request.snapshot_uuid||source.request.canonical_record_key!=request.canonical_record_key||
               !source.receipt.value("confirmed",false)||!source.retired||source.normal_scenery) {
                error="building history target has no confirmed current normal building placement";return action::Outcome::RejectedNoEffect;
            }
            source_state=&source.state;uid=source.receipt.at("native_uid").get<uint64_t>();amount=source.receipt.at("cost").get<int32_t>();
        } else if(removed!=operations_.end()) {
            auto& source=removed->second;
            if(source.request.action!=action::Action::RemoveSnapshotBuilding||source.request.action_uuid!=request.target_action_uuid||source.binding!=binding||
               source.request.snapshot_uuid!=request.snapshot_uuid||source.request.canonical_record_key!=request.canonical_record_key||
               !source.history_ready||source.normal_scenery||!source.removal.value("confirmed",false)||
               !source.retirement.value("confirmed",false)) {
                error="building history target has no confirmed retired normal building removal";return action::Outcome::RejectedNoEffect;
            }
            source_remove=true;source_state=&source.object_before;uid=source.local_uid;amount=source.refund;
        } else {
            error="building history target confirmed mutation missing";return action::Outcome::RejectedNoEffect;
        }
        if(!uid||amount<0||!source_state->alive){error="building history original UID/amount/state invalid";return action::Outcome::RejectedNoEffect;}
        const bool destroying=(request.action==action::Action::UndoBuilding)!=source_remove;
        auto alive=*source_state;auto dead=alive;dead.alive=false;
        const auto row=world_.rows().find(request.canonical_record_key);
        if(row==world_.rows().end()||row->second.state!=(destroying?alive:dead)) {
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
            if(target.uid!=uid||!target.normal_building||target.normal_scenery){error="building history destructive target UID/family differs";return action::Outcome::RejectedNoEffect;}
            if(!world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,settled,error)||
               !world_.refresh_current(world_.origin(),request.canonical_record_key,settled,error))return action::Outcome::RejectedNoEffect;
            alive=settled.state;dead=alive;dead.alive=false;*source_state=alive;
        } else if(!remove_diagnostics::history_target_absent(request.snapshot_uuid,request.canonical_record_key,uid,error))
            return action::Outcome::RejectedNoEffect;
        action::Ledger expected;
        if(!building_history_slice::cash_effect(before,destroying?amount:-amount,expected,error))return action::Outcome::RejectedNoEffect;
        if(authority&&(authority->before!=before||authority->object_before!=(destroying?alive:dead)||
                       authority->object_after!=(destroying?dead:alive)||authority->after!=expected)) {
            error="building history Client before/effect/full Ledger differs from Host";return action::Outcome::RejectedNoEffect;
        }
        auto& operation=history_.emplace(request.action_uuid,HistoryOperation{request,binding,before,{},
            destroying?alive:dead,destroying?dead:alive,uid,false}).first->second;
        const auto outcome=building_history_slice::perform(request.action,request.target_action_uuid,error);
        if(outcome!=action::Outcome::Confirmed)return outcome;
        if(!read_ledger(operation.after,error))return action::Outcome::Unknown;
        if(operation.after!=expected){error="building history actual full Ledger differs from original cash inverse";return action::Outcome::Unknown;}
        if(destroying) {
            if(!remove_diagnostics::retire_undone_building(request.snapshot_uuid,request.canonical_record_key,uid,error)||
               !world_.retire_native_effects(world_.origin(),revision,request.action_uuid,{request.canonical_record_key},error))return action::Outcome::Unknown;
        } else {
            if(!remove_diagnostics::restore_redone_building(request.snapshot_uuid,request.canonical_record_key,uid,error))return action::Outcome::Unknown;
            remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding observed;
            if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
               target.uid!=uid||!target.normal_building||target.normal_scenery||!scenery_kiss::read_action_prop(uid,actual,error)||
               world_state::building_state(actual)!=alive||
               !world_state::bind_root_readback(world_.origin(),request.canonical_record_key,target,actual,observed,error)||
               !world_.restore_native_building_key(world_.origin(),revision,request.action_uuid,request.canonical_record_key,observed,error)) {
                if(error.empty())error="building history restored native UID/family/full state differs";return action::Outcome::Unknown;
            }
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
        if(research_action(request.action))return perform_research(binding,request,nullptr,error);
        if(request.action==action::Action::SetBugFix)return bug_authority::manual_action::perform(binding,request,nullptr,error);
        if(request.action==action::Action::RemoveMonster)return npc_probe::monster_action_runtime::remove_perform(binding,request,nullptr,error);
        if(request.action==action::Action::MoveMonster)return npc_probe::monster_action_runtime::move_perform(binding,request,nullptr,error);
        if(request.action==action::Action::PlaceMonster)return npc_probe::monster_action_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::PlaceMonsterZone)return npc_probe::zone_action_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetStartingpointWorkers)return perform_staff(binding,request,nullptr,error);
        if(request.action==action::Action::StartStartingpoint)return perform_start(binding,request,nullptr,error);
        if(request.action==action::Action::CommitStartingpoint)return perform_commit(binding,request,nullptr,error);
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC)
            return npc_probe::action_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::SetPrices)return ui_price_runtime::perform(binding,request,nullptr,error);
        if(request.action==action::Action::HireDeveloper||request.action==action::Action::FireDeveloper)
            return developer_staff::action_runtime::perform(binding,request,nullptr,error);
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
        if(research_action(request.action))return perform_research(binding,request,&host,error);
        if(request.action==action::Action::SetBugFix)return bug_authority::manual_action::perform(binding,request,&host,error);
        if(request.action==action::Action::RemoveMonster)return npc_probe::monster_action_runtime::remove_perform(binding,request,&host,error);
        if(request.action==action::Action::MoveMonster)return npc_probe::monster_action_runtime::move_perform(binding,request,&host,error);
        if(request.action==action::Action::PlaceMonster)return npc_probe::monster_action_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::PlaceMonsterZone)return npc_probe::zone_action_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::SetStartingpointWorkers)return perform_staff(binding,request,&host,error);
        if(request.action==action::Action::StartStartingpoint)return perform_start(binding,request,&host,error);
        if(request.action==action::Action::CommitStartingpoint)return perform_commit(binding,request,&host,error);
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC)
            return npc_probe::action_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::SetPrices)return ui_price_runtime::perform(binding,request,&host,error);
        if(request.action==action::Action::HireDeveloper||request.action==action::Action::FireDeveloper)
            return developer_staff::action_runtime::perform(binding,request,&host,error);
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
        action::Context bug_context;if(live_state_&&(!action::selected_session().current(bug_context,error)||!bug_authority::begin_action(bug_context,binding,request.action_uuid,error)))return action::Outcome::RejectedNoEffect;
        action::Outcome outcome=action::Outcome::Unknown;
        try{outcome=execute_action_native(binding,request,error);}
        catch(const std::exception& e){error=std::string("Original ordered action: ")+e.what();}
        catch(...){error="Original ordered action nonstandard exception";}
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
        action::Context bug_context;bool bug_scope=false;
        if(action::selected_session().current(bug_context,error))bug_scope=bug_authority::begin_action(bug_context,binding,request.action_uuid,error);
        try {if(bug_scope)outcome=apply_action_native(binding,request,host,error);}
        catch(const std::exception& failure){error="live native action original exception: "+std::string(failure.what());}
        catch(...){error="live native action original nonstandard exception";}
        if(bug_scope){std::string bug_error;if(!bug_authority::end_action(outcome==action::Outcome::Confirmed,bug_error)){error=bug_error+" original="+error;outcome=action::Outcome::Unknown;}}
        const auto original=error;std::string readback;
        if(!fence.end_action(readback)){error="live native cash scope: "+readback+" original="+original;return action::Outcome::Unknown;}
        if(outcome==action::Outcome::Confirmed&&!live_state_->after_action(authority,readback)){error="live actual Day after-action receipt: "+readback+" original="+original;return action::Outcome::Unknown;}
        error=original;return outcome;
    }
    bool observe_action(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error)override {
        if(request.action==action::Action::SetBugFix)return bug_authority::manual_action::observe(binding,request,out,error);
        if(request.action==action::Action::RemoveMonster)return npc_probe::monster_action_runtime::remove_observe(binding,request,out,error);
        if(request.action==action::Action::MoveMonster)return npc_probe::monster_action_runtime::move_observe(binding,request,out,error);
        if(request.action==action::Action::PlaceMonster)return npc_probe::monster_action_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::PlaceMonsterZone)return npc_probe::zone_action_runtime::observe(binding,request,out,error);
        if(research_action(request.action)||request.action==action::Action::CommitStartingpoint||request.action==action::Action::StartStartingpoint||request.action==action::Action::SetStartingpointWorkers)return observe_commit(binding,request,out,error);
        if(request.action==action::Action::PlaceNPC||request.action==action::Action::RemoveNPC||
           request.action==action::Action::MoveNPC)
            return npc_probe::action_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::SetPrices)return ui_price_runtime::observe(binding,request,out,error);
        if(request.action==action::Action::HireDeveloper||request.action==action::Action::FireDeveloper)
            return developer_staff::action_runtime::observe(binding,request,out,error);
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
            if(operation.object_after.alive) {
                remove_diagnostics::LocalTarget target;native::BuildingState native_state;
                if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,error)||
                   target.uid!=operation.uid||!scenery_kiss::read_action_prop(target.uid,native_state,error)||
                   world_state::building_state(native_state)!=operation.object_after)return false;
            }
            out={};out.before=operation.before;out.after=operation.after;
            out.object_before=operation.object_before;out.object_after=operation.object_after;
            if(!operation.object_after.alive&&!remove_diagnostics::history_target_absent(request.snapshot_uuid,request.canonical_record_key,operation.uid,error))return false;
            out.undo_confirmed=true;out.target_retired=!operation.object_after.alive;
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
        if(request.action==action::Action::PlaceMonsterZone||request.action==action::Action::PlaceMonster||request.action==action::Action::MoveMonster||request.action==action::Action::RemoveMonster){
            if(!current(binding,request.snapshot_uuid,error))return false;
            const auto status=typed_action_runtime::controller_status();
            if(!status.value("pending",false)||status.value("action_uuid",std::string{})!=request.action_uuid){error="Zone retirement lacks exact Controller request";return false;}
            if(!world_.retire_native_effects(world_.origin(),status.at("host_revision").get<uint64_t>(),request.action_uuid,{},error))return false;
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
            if(!found->second.normal_scenery) {
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
    std::unique_ptr<scenario_vip_authority::FrameAdapter> vip_presentation;
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
    }){}
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
    if(!ui_price_runtime::enable(error)||!npc_probe::action_runtime::enable(error)||!developer_staff::action_runtime::enable(error)||!npc_probe::zone_action_runtime::enable(error)||!npc_probe::monster_action_runtime::enable(error)||!bug_authority::manual_action::enable(error))return false;
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
    runtime->live_state=std::make_unique<live_host_state::Bridge>(context,binding,epoch,*runtime->live_port,runtime->transport,[context](uint64_t& revision,std::string& cause){return live_host_state::read_time_authority(context,revision,cause);});
    runtime->day_port=std::make_unique<live_host_state::day::NativePort>(context,binding,fence);runtime->day_state=std::make_unique<live_host_state::day::Sync>(*runtime->day_port,*runtime->live_port,context,binding);if(!runtime->day_state->bootstrap(error))return false;runtime->live_state->bind_day(*runtime->day_state);runtime->economy_port=std::make_unique<live_host_state::day::economy::NativePort>(*runtime->day_port,fence,live_host_state::day::Target{});runtime->economy_state=std::make_unique<live_host_state::day::economy::Coupling>(*runtime->day_state,*runtime->economy_port);runtime->live_state->bind_economy(*runtime->economy_state);
    runtime->vip_presentation=std::make_unique<scenario_vip_authority::FrameAdapter>(&vip_alert_authorize);
    if(!runtime->live_state->bind_vip(*runtime->vip_presentation,error))return false;
    runtime->marketing=std::make_unique<marketing_authority::NativePort>();
    if(!runtime->live_state->bind_marketing(*runtime->marketing,error))return false;
    if(context.role==action::Role::Host)live_host_state::day::bind_host_journal(runtime->day_state.get());runtime->engine.bind_live_state(*runtime->live_state,*runtime->live_port);return true;
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
        std::string stopped;
        if(!runtime->controller.tick(GetTickCount64(),stopped)&&stopped.empty())LOG_ERROR("[Action] Terminal session observation failed without cause");
        return;
    }
    if(runtime->live_state&&!runtime->controller.status().locked) {
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
            std::string stopped;
            if(!runtime->controller.tick(GetTickCount64(),stopped)&&stopped.empty())LOG_ERROR("[Action] Failed session observation lacks cause");
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
    weapon_bootstrap(*runtime);
    ui_move_runtime::tick();
    ui_region_runtime::tick();
    ui_price_runtime::tick();
    developer_staff::action_runtime::tick();
    bug_authority::manual_action::tick();
    npc_probe::action_runtime::tick();
    npc_probe::zone_action_runtime::tick();
    npc_probe::monster_action_runtime::tick();
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
    return {{"initialized",true},{"weapon_visual",weapon_status(*runtime)},{"bug_manual",bug_authority::manual_action::status()},{"ui_price",ui_price_runtime::status()},{"developer_staff",developer_staff::action_runtime::status()},{"ui_npc",npc_probe::action_runtime::status()},{"ui_monster_zone",npc_probe::zone_action_runtime::status()},{"ui_monster",npc_probe::monster_action_runtime::status()},{"lobby_retiring",runtime->lobby_retiring},{"live_failure_terminal",runtime->live_failure_terminal},{"started",s.started},{"locked",s.locked},{"pending",s.pending||runtime->lobby_retiring},
        {"host_revision",s.host_revision},{"attempted",s.attempted},{"confirmed",s.confirmed},{"failed",s.failed},
        {"duplicates",s.duplicates},{"suppressed_errors",s.suppressed_errors},{"required_mask",s.required_mask},{"ack_mask",s.ack_mask},
        {"outcome",int(s.outcome)},{"action_uuid",s.action_uuid},{"cause",s.cause},{"last_packet_accepted",s.last_packet_accepted},
        {"subscriber_additions",runtime->economy_state?runtime->economy_state->addition_receipt():nlohmann::json(nullptr)},
        {"frame_transport",runtime->live_state?runtime->live_state->frame_transport_status():nlohmann::json(nullptr)},
        {"toon_time",toon_time_status(*runtime)},
        {"vip_authority",vip_status(*runtime)},
        {"marketing_frame",runtime->live_state?runtime->live_state->marketing_status():nlohmann::json(nullptr)},
        {"vip_frame",runtime->live_state?runtime->live_state->vip_status():nlohmann::json(nullptr)},
        {"vip_presentation",runtime->vip_presentation?runtime->vip_presentation->status():nlohmann::json(nullptr)},
        {"network_receipt",runtime->live_state?live_host_state::network::encode_receipt(runtime->live_state->network_receipt()):nlohmann::json::object()},
        {"network_activation_receipt",runtime->live_state?runtime->live_state->network_activation_receipt().status():nlohmann::json(nullptr)},
        {"network_frame_revision",runtime->live_state&&runtime->live_state->last()?runtime->live_state->last()->revision:0},
        {"network_target",runtime->live_state&&runtime->live_state->last()&&runtime->live_state->last()->network?live_host_state::network::encode(*runtime->live_state->last()->network):nlohmann::json(nullptr)},
        {"native_economy",native_economy},{"native_economy_read_error",economy_read_error},
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
bool submit_building_history(action::Action action,const std::string& id,const std::string& target,std::string& error) {return submit_building_history(action,id,target,"action:"+target,error);}
bool submit_building_history(action::Action action,const std::string& id,const std::string& target,const std::string& key,std::string& error) {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()){error="building history requires started game-thread Controller";return false;}
    if(runtime->lobby_retiring){error="native lobby owner retiring; building history not attempted";return false;}
    return runtime->controller.submit_building_history(action,id,target,key,GetTickCount64(),error);
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
    if(build.value("deferred_build_capture",false)||remove.value("replay_active",false)||remove.value("inflight",uint64_t{})||move.value("pending_ui",false)||region.value("pending_ui",false)||price.value("pending_ui",false)||npc_probe::action_runtime::pending_ui()||developer_staff::action_runtime::pending_ui()||npc_probe::zone_action_runtime::pending_ui()||npc_probe::monster_action_runtime::pending_ui()||bug_authority::manual_action::pending_ui()){e="native lobby Leave blocked by retained active/pending UI capture intent";return false;}
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
    net::chat::Runtime::instance().set_lobby_admitted(false);
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
    if(!developer_staff::action_runtime::retire_lobby(e)||!bug_authority::manual_action::retire_lobby(e))return false;
    if(r&&r->vip_presentation)r->vip_presentation->retire_lobby();
    live_host_state::day::bind_host_journal(nullptr);published_runtime.store(nullptr,std::memory_order_release);delete r;
    ready=true;e.clear();return true;
}
}

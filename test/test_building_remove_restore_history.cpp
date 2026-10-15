#include "multiplayer_world/native_binding_adapter.h"
#include "building_history_slice/native_bridge.h"
#include "building_history_slice/cash_effect.h"
#include "scenery_kiss/native_catalog.h"
#include "typed_action_runtime.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using json=nlohmann::json;
namespace action=mt2mp::action_session;
namespace world_state=mt2mp::world_state;
namespace native=mt2mp::game::native;
namespace remove_diagnostics=mt2mp::game::remove_diagnostics;
namespace scenery_kiss=mt2mp::scenery_kiss;
namespace building_history_slice=mt2mp::game::building_history_slice;
namespace typed_action_runtime=mt2mp::game::typed_action_runtime;
namespace {
const std::string snapshot="17541a49-b8cb-424d-9788-8268fcb36ccf",fixture_placed="ea442a72-e563-4d78-9757-05f2688016c0";
const std::string fixture_removed="22222222-2222-4222-8222-222222222222",undo_id="9672387e-d58d-409b-8c79-939df9bccbd5",redo_id="11111111-1111-4111-8111-111111111111";
int calls=0,checks=0,restore_calls=0,absence_calls=0;bool fail_resolve=false,fail_read=false,fail_original=false,fail_restore=false,fail_absence=false,bad_cash=false,restore_pose=false,is_remove=true;
uint64_t fixture_revision=3;std::string pending=undo_id;int32_t fixture_amount=400;
action::Ledger ledger;native::BuildingState fixture_observed;remove_diagnostics::LocalTarget local;
bool read_ledger(action::Ledger& out,std::string& error){out=ledger;error.clear();return true;}
void check(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);++checks;}
struct Harness {
    struct BuildOperation {action::Request request;action::Binding binding;action::Ledger before;action::BuildingState state;json receipt;bool retired=false;action::BuildingState object_before;bool normal_scenery=false;};
    struct Operation {action::Request request;action::Binding binding;action::Ledger before;json removal,retirement;action::BuildingState object_before;uint64_t local_uid=0;bool normal_scenery=false,history_ready=false;int32_t refund=0;};
    struct HistoryOperation {action::Request request;action::Binding binding;action::Ledger before,after;action::BuildingState object_before,object_after;uint64_t uid=0;bool confirmed=false,retired=false;};
    std::map<std::string,BuildOperation> buildings_;std::map<std::string,Operation> operations_;std::map<std::string,HistoryOperation> history_;world_state::Table world_;
    std::string key,fixture_source;bool current(const action::Binding&,const std::string&,std::string& error){error.clear();return true;}
#include "history_method.inc"
    Harness(bool removal=true,bool baseline=false,int32_t price=400){
        is_remove=removal;fixture_amount=price;calls=restore_calls=absence_calls=0;fail_resolve=fail_read=fail_original=fail_restore=fail_absence=bad_cash=restore_pose=false;fixture_revision=3;pending=undo_id;
        ledger={};ledger.cash=88040;ledger.record_cash=88040;ledger.record_present=1;ledger.flat_day=ledger.record_day=2;
        action::BuildingState state;state.type="SmallInn";state.variant="base";state.alive=true;state.x=8020;state.y=70.83831787109375f;state.z=8550;
        world_state::Origin origin{snapshot,std::string(64,'a'),2,1};std::string error;
        key=baseline?origin.sha256+":world:building:44":"action:"+fixture_placed;fixture_source=removal?fixture_removed:fixture_placed;
        check(world_.bind_snapshot(origin,{{key,origin.sha256+":world:building:44",state}},1,error),"world bootstrap");
        action::Request request;request.action=removal?action::Action::RemoveSnapshotBuilding:action::Action::PlaceBuilding;request.action_uuid=fixture_source;request.snapshot_uuid=snapshot;request.canonical_record_key=key;
        if(removal){check(world_.retire_native_effects(origin,2,fixture_removed,{key},error),"real table Remove");operations_[fixture_removed]={request,{2,1},{},{{"confirmed",true}},{{"confirmed",true}},state,7328,false,true,price};}
        else buildings_[fixture_placed]={request,{2,1},{},state,{{"native_uid",7328u},{"cost",price},{"confirmed",true}},true,{},false};
        fixture_observed={};fixture_observed.prop={7328,state.x,state.y,state.z};fixture_observed.type=state.type;fixture_observed.variant=state.variant;fixture_observed.rotation=state.rotation;fixture_observed.scale=state.scale;
        local={};local.uid=7328;local.world_epoch=2;local.world_generation=1;local.snapshot_id=snapshot;local.record_key=key;local.type=state.type;local.variant=state.variant;local.normal_building=true;
    }
    action::Request request(bool undo=true){action::Request r;r.action=undo?action::Action::UndoBuilding:action::Action::RedoBuilding;r.action_uuid=undo?undo_id:redo_id;r.target_action_uuid=fixture_source;r.snapshot_uuid=snapshot;r.canonical_record_key=key;return r;}
    action::Outcome run(std::string& error,const action::Evidence* authority=nullptr,bool undo=true){return perform_building_history({2,1},request(undo),authority,error);}
    action::Evidence authority(bool undo=true){action::Evidence e;e.before=ledger;const bool destroy=undo!=is_remove;e.object_before=world_state::building_state(fixture_observed);e.object_before.alive=destroy;e.object_after=e.object_before;e.object_after.alive=!destroy;std::string error;check(building_history_slice::cash_effect(e.before,destroy?fixture_amount:-fixture_amount,e.after,error),error);return e;}
};
}
namespace mt2mp::game::typed_action_runtime {json status(){return {{"pending",true},{"action_uuid",pending},{"host_revision",fixture_revision}};}}
namespace mt2mp::game::remove_diagnostics {
bool resolve_record_target(const std::string&,const std::string&,LocalTarget& out,std::string& error){if(fail_resolve){error="fixture original resolver failure";return false;}out=local;error.clear();return true;}
bool retire_undone_building(const std::string&,const std::string&,uint64_t,std::string& error){error.clear();return true;}
bool restore_redone_building(const std::string&,const std::string&,uint64_t,std::string& error){++restore_calls;if(fail_restore){error="fixture original rebind failure";return false;}if(restore_pose)fixture_observed.prop.y+=1;error.clear();return true;}
bool history_target_absent(const std::string&,const std::string&,uint64_t,std::string& error){++absence_calls;if(fail_absence){error="fixture old holders not retired";return false;}error.clear();return true;}
}
namespace mt2mp::scenery_kiss {bool read_action_prop(uint64_t,game::native::BuildingState& out,std::string& error){if(fail_read){error="fixture original read failure";return false;}out=fixture_observed;error.clear();return true;}}
namespace mt2mp::game::building_history_slice {action::Outcome perform(action::Action direction,const std::string&,std::string& error){++calls;if(fail_original){error="fixture partial native failure";return action::Outcome::Unknown;}const bool destroy=(direction==action::Action::UndoBuilding)!=is_remove;action::Ledger after;if(!cash_effect(ledger,destroy?fixture_amount:-fixture_amount,after,error))return action::Outcome::Unknown;ledger=after;if(bad_cash)++ledger.line_income[19];error.clear();return action::Outcome::Confirmed;}}
int main(){try{
    std::string error;
    for(bool baseline:{false,true}){Harness h(true,baseline);const auto original_key=h.key;check(h.run(error)==action::Outcome::Confirmed,"Remove Undo restore: "+error);check(calls==1&&restore_calls==1&&h.world_.rows().at(h.key).state.alive&&ledger.cash==87640,"restores same key with full inverse debit");fixture_revision=4;pending=redo_id;check(h.run(error,nullptr,false)==action::Outcome::Confirmed,"Remove Redo: "+error);check(calls==2&&!h.world_.rows().at(h.key).state.alive&&ledger.cash==88040&&h.world_.rows().size()==1&&h.key==original_key,"Redo removes same canonical key");check(h.run(error,nullptr,false)==action::Outcome::Unknown&&calls==2,"confirmed action UUID never replayed");}
    {Harness h(false);check(h.run(error)==action::Outcome::Confirmed,"placement Undo compatibility: "+error);fixture_revision=4;pending=redo_id;check(h.run(error,nullptr,false)==action::Outcome::Confirmed&&calls==2,"placement Redo compatibility: "+error);}
    {Harness h;auto e=h.authority();check(h.run(error,&e)==action::Outcome::Confirmed&&calls==1,"Client applies exact Host full ledger before ACK: "+error);}
    for(int field=0;field<4;++field){Harness h;auto e=h.authority();if(field==0)++e.before.line_counts[19];if(field==1)++e.after.record_income;if(field==2)e.object_before.x+=1;if(field==3)e.object_after.rotation[0]=1;check(h.run(error,&e)==action::Outcome::RejectedNoEffect&&calls==0,"Client mismatched Host evidence blocked before original");}
    {Harness h;fail_absence=true;check(h.run(error)==action::Outcome::RejectedNoEffect&&calls==0&&error=="fixture old holders not retired","old holder retirement guard preserves cause");}
    {Harness h;fail_original=true;check(h.run(error)==action::Outcome::Unknown&&calls==1&&error=="fixture partial native failure","partial Original cause");check(h.run(error)==action::Outcome::Unknown&&calls==1,"partial never replays");}
    {Harness h;bad_cash=true;check(h.run(error)==action::Outcome::Unknown&&calls==1&&restore_calls==0,"noncash full ledger drift remains Unknown");}
    {Harness h;fail_restore=true;check(h.run(error)==action::Outcome::Unknown&&calls==1&&error=="fixture original rebind failure","fresh rebind cause stays visible");}
    {Harness h;restore_pose=true;check(h.run(error)==action::Outcome::Unknown&&calls==1&&!h.world_.rows().at(h.key).state.alive,"native restore pose mismatch is not committed");}
    {Harness h;local.uid++;check(h.run(error)==action::Outcome::Unknown&&calls==1,"fresh UID mismatch after original remains Unknown");}
    {Harness h;h.operations_.at(fixture_removed).history_ready=false;check(h.run(error)==action::Outcome::RejectedNoEffect&&calls==0,"unconfirmed fixture_source cannot Restore");}
    {Harness h;h.operations_.at(fixture_removed).binding.native_generation++;check(h.run(error)==action::Outcome::RejectedNoEffect&&calls==0,"old world fixture_source rejected");}
    {Harness h;h.operations_.at(fixture_removed).request.action_uuid=fixture_placed;check(h.run(error)==action::Outcome::RejectedNoEffect&&calls==0,"retained mutation UUID differs from requested source");}
    {Harness h;h.operations_.at(fixture_removed).normal_scenery=true;check(h.run(error)==action::Outcome::RejectedNoEffect&&calls==0,"separate Scenery fixture_source rejected");}
    {Harness h;pending=redo_id;check(h.run(error)==action::Outcome::RejectedNoEffect&&calls==0,"wrong pending action rejected");}
    {Harness h;ledger.record_cash=INT32_MIN;check(h.run(error)==action::Outcome::RejectedNoEffect&&calls==0&&error.find("overflow")!=std::string::npos,"inverse int32 overflow rejected preeffect");}
    {Harness h(true,false,0);const auto before=ledger;check(h.run(error)==action::Outcome::Confirmed&&ledger==before,"zero-price inverse has no cash call or category increment");}
    {Harness h(false);fixture_observed.prop.y+=0.2f;check(h.run(error)==action::Outcome::Confirmed&&h.history_.at(undo_id).object_before.y==fixture_observed.prop.y,"existing settled height capture preserved");}
    {Harness h(false);fail_resolve=true;check(h.run(error)==action::Outcome::RejectedNoEffect&&calls==0&&error=="fixture original resolver failure","destructive resolve preserves cause");}
    std::cout<<"PASS "<<checks<<" actual history method/Table/cash assertions; native calls are controlled test doubles\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

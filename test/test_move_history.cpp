#include "multiplayer_world/native_binding_adapter.h"
#include "building_history_slice/native_bridge.h"
#include "scenery_kiss/native_catalog.h"
#include "typed_action_runtime.h"
#include <iostream>
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
const std::string snapshot="17541a49-b8cb-424d-9788-8268fcb36ccf", moved="11111111-1111-4111-8111-111111111111";
const std::string undo_id="22222222-2222-4222-8222-222222222222",redo_id="33333333-3333-4333-8333-333333333333";
unsigned checks=0,effects=0,refreshes=0;bool partial=false,bad_money=false,bad_pose=false,bad_uid=false;
std::string pending;uint64_t fixture_revision=2;
action::Ledger money;action::BuildingState old_pose,new_pose;
native::BuildingState fixture_actual;remove_diagnostics::LocalTarget local;
void need(bool value,const std::string& error){++checks;if(!value)throw std::runtime_error(error);}
void set_pose(const action::BuildingState& state){
    fixture_actual={};fixture_actual.prop={7328,state.x,state.y,state.z};fixture_actual.type=state.type;fixture_actual.variant=state.variant;
    fixture_actual.rotation=state.rotation;fixture_actual.scale=state.scale;
}
bool read_ledger(action::Ledger& out,std::string& error){out=money;error.clear();return true;}
struct Harness {
    const action::Request* active_dungeon_building_=nullptr;
#include "dungeon_original_scope.inc"
    struct BuildOperation {action::Request request;action::Binding binding;action::Ledger before;action::BuildingState state;json receipt;bool retired=false;action::BuildingState object_before;bool normal_scenery=false;};
    struct HistoryOperation {action::Request request;action::Binding binding;action::Ledger before,after;action::BuildingState object_before,object_after;uint64_t uid=0;bool confirmed=false,retired=false;};
    std::map<std::string,BuildOperation> buildings_;std::map<std::string,HistoryOperation> history_;world_state::Table world_;
    std::string key;
    bool current(const action::Binding&,const std::string&,std::string& error){error.clear();return true;}
#include "move_history_method.inc"
    explicit Harness(bool scenery=false,bool baseline=false){
        effects=refreshes=0;partial=bad_money=bad_pose=bad_uid=false;pending=undo_id;fixture_revision=2;
        money={};money.cash=money.record_cash=94000;money.record_present=1;money.flat_day=money.record_day=2;
        old_pose={};old_pose.type=scenery?"Cactus":"SmallInn";old_pose.variant="base";old_pose.alive=true;
        old_pose.x=8000;old_pose.y=70;old_pose.z=8500;new_pose=old_pose;new_pose.x+=15;new_pose.rotation={0,1,0,0};new_pose.scale={2,2,2};
        const world_state::Origin origin{snapshot,std::string(64,'a'),2,1};
        key=baseline?origin.sha256+":world:building:44":"action:44444444-4444-4444-8444-444444444444";
        std::string error;need(world_.bind_snapshot(origin,{{key,origin.sha256+":world:building:44",new_pose}},1,error),error);
        action::Request source;source.action=action::Action::MoveBuilding;source.action_uuid=moved;source.snapshot_uuid=snapshot;source.canonical_record_key=key;
        buildings_[moved]={source,{2,1},money,new_pose,{{"native_uid",7328u},{"confirmed",true}},true,old_pose,scenery};
        set_pose(new_pose);local={};local.uid=7328;local.world_epoch=2;local.world_generation=1;local.snapshot_id=snapshot;local.record_key=key;
        local.type=new_pose.type;local.variant="base";local.normal_building=!scenery;local.normal_scenery=scenery;
    }
    action::Request request(bool undo=true){action::Request r;r.action=undo?action::Action::UndoBuilding:action::Action::RedoBuilding;
        r.action_uuid=undo?undo_id:redo_id;r.snapshot_uuid=snapshot;r.target_action_uuid=moved;r.canonical_record_key=key;return r;}
    action::Outcome run(std::string& error,const action::Evidence* authority=nullptr,bool undo=true){return perform_move_history({2,1},request(undo),authority,error);}
    action::Evidence authority(){action::Evidence e;e.before=e.after=money;e.object_before=new_pose;e.object_after=old_pose;return e;}
};
}
namespace mt2mp::game::typed_action_runtime {json status(){return {{"pending",true},{"action_uuid",pending},{"host_revision",fixture_revision}};}}
namespace mt2mp::game::remove_diagnostics {
bool is_dungeon_building_target(const LocalTarget&,const std::string&,std::string&){
    throw std::runtime_error("Ordinary history fixture entered Dungeon target validation");
}
bool restore_redone_dungeon_building(const std::string&,const std::string&,uint64_t,const std::string&,std::string&){
    throw std::runtime_error("Ordinary history fixture entered Dungeon restoration");
}
bool resolve_record_target(const std::string&,const std::string&,LocalTarget& out,std::string& error){out=local;error.clear();return true;}
bool refresh_moved_building(const std::string&,const std::string&,uint64_t,std::string& error,const std::string& scene){if(!scene.empty())throw std::runtime_error("Ordinary history fixture entered Dungeon refresh");++refreshes;if(bad_uid)++local.uid;error.clear();return true;}
}
namespace mt2mp::scenery_kiss {bool read_action_prop(uint64_t,game::native::BuildingState& out,std::string& error){out=fixture_actual;error.clear();return true;}}
namespace mt2mp::game::building_history_slice {
action::Outcome perform(action::Action direction,const std::string&,std::string& error){
    ++effects;if(partial){error="retained original Move inverse failure";return action::Outcome::Unknown;}
    set_pose(direction==action::Action::UndoBuilding?old_pose:new_pose);if(bad_pose)fixture_actual.prop.z+=1;if(bad_money)++money.line_income[19];
    error.clear();return action::Outcome::Confirmed;
}}
int main()try{
    {Harness harness;auto dungeon=harness.request();dungeon.dungeon_building=true;std::string cause;
     need(harness.perform_move_history({2,1},dungeon,nullptr,cause)==action::Outcome::RejectedNoEffect&&effects==0&&
          cause=="Dungeon Building original requires its exact prepared native scope","Dungeon original bypassed prepared full scene scope");}
    std::string error;
    for(bool scenery:{false,true})for(bool baseline:{false,true}){
        Harness h(scenery,baseline);const auto expected_money=money;auto e=h.authority();
        need(h.run(error,&e)==action::Outcome::Confirmed,"Undo Move: "+error);
        need(h.world_.rows().at(h.key).state==old_pose&&effects==1&&refreshes==1&&money==expected_money,"Undo changed identity, economy or missed original");
        pending=redo_id;fixture_revision=3;need(h.run(error,nullptr,false)==action::Outcome::Confirmed,"Redo Move: "+error);
        need(h.world_.rows().at(h.key).state==new_pose&&h.world_.rows().size()==1&&effects==2&&money==expected_money,"Redo lost pose or key");
        need(h.run(error,nullptr,false)==action::Outcome::Unknown&&effects==2,"confirmed inverse replayed");
    }
    for(unsigned field=0;field<4;++field){Harness h;auto e=h.authority();
        if(field==0)++e.before.line_counts[19];if(field==1)++e.after.cash;if(field==2)e.object_before.x+=1;if(field==3)e.object_after.scale[0]+=1;
        need(h.run(error,&e)==action::Outcome::RejectedNoEffect&&effects==0,"Host mismatch executed inverse");}
    for(unsigned field=0;field<4;++field){Harness h;
        if(field==0)h.buildings_.at(moved).binding.native_generation++;if(field==1)h.buildings_.at(moved).retired=false;
        if(field==2)++local.uid;if(field==3)local.normal_scenery=true;
        need(h.run(error)==action::Outcome::RejectedNoEffect&&effects==0,"invalid original binding executed inverse");}
    {Harness h;partial=true;need(h.run(error)==action::Outcome::Unknown&&error=="retained original Move inverse failure","lost partial cause");
        need(h.run(error)==action::Outcome::Unknown&&effects==1,"partial inverse replayed");}
    for(unsigned field=0;field<3;++field){Harness h;if(field==0)bad_money=true;if(field==1)bad_pose=true;if(field==2)bad_uid=true;
        need(h.run(error)==action::Outcome::Unknown&&effects==1&&h.world_.rows().at(h.key).state==new_pose,"bad native result was committed");}
    std::cout<<checks<<" fixture_actual Move history/Table assertions passed; native calls use controlled doubles\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}

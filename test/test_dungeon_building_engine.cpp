#include "dungeon_building_fixture.h"
#include "multiplayer_world/world_state.h"
#include "ui_remove/ui_capture.h"
#include <iostream>
#include <map>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
namespace action=mt2mp::action_session;
namespace world_state=mt2mp::world_state;
namespace fixture {
namespace dto=dungeon_building_fixture;
unsigned checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
std::string fail_at;
std::vector<std::string> calls;
unsigned originals=0,retirements=0,phase=0;
bool client=false,bad_after=false;
bool contained=false,target_alive=false,bad_census=false;
uintptr_t target_pointer=0x2000,current_graph=0x6000;
uint64_t requested_revision=1;
const void* expected_consumed=nullptr;
const void* expected_full_seed=nullptr;
unsigned history_seed_calls=0,child_publications=0;
const std::string contained_key=mt2mp::game::scene_clone::birth_key(dto::source_id,mt2mp::game::scene_clone::Family::Building,0);
struct GraphExpectation {
    action::DungeonBuildingState value;action::BuildingState destination;
    std::string sha;int32_t cash=0;bool destroying=false;
};
std::map<uintptr_t,GraphExpectation> graphs;
action::Request request;
action::Ledger ledger;
action::DungeonBuildingEffect effect;
bool step(const std::string& name,std::string& error){
    calls.push_back(name);
    if(name==fail_at){error="original fixture failure: "+name;return false;}
    return true;
}
}
namespace mt2mp::action_session {
class FixtureSession final:public Session {
public:
    bool current(Context& out,std::string& error)override{
        if(!fixture::step("session",error))return false;
        out.role=fixture::client?Role::Client:Role::Host;out.snapshot_uuid=fixture::request.snapshot_uuid;return true;
    }
};
Session& selected_session(){static FixtureSession value;return value;}
}
namespace dungeon_scene_identity_runtime {struct Census {};}
namespace npc_probe::dungeon_building {
struct BuildingOwner {std::string key,scene;uintptr_t object=0;uint64_t uid=0;action::BuildingState state;};
struct Local {std::vector<BuildingOwner> owners;const void* history_source=nullptr;};
struct Snapshot {action::DungeonBuildingState value;std::shared_ptr<Local> local;};
Snapshot current(){
    Snapshot out;out.value=fixture::phase?fixture::effect.after:fixture::effect.before;out.local=std::make_shared<Local>();
    const auto root_key=std::string("action:")+fixture::dto::source_id;
    out.local->owners.push_back({root_key,"world",0x1000,0x100000007ULL,fixture::dto::object(true)});
    if(fixture::contained){
        if(fixture::bad_census&&fixture::phase)out.local->owners.front().state.x+=1;
        if(fixture::target_alive)out.local->owners.push_back({fixture::contained_key,root_key+"/template",
            fixture::target_pointer,0x200000007ULL,fixture::dto::object(false)});
    }else if(fixture::phase)out.local->owners.push_back({fixture::request.canonical_record_key,"world",0x2000,0x200000007ULL,fixture::request.building});
    return out;
}
bool capture(const action::Binding&,const std::string&,Snapshot& out,std::string& error,const Snapshot* previous=nullptr){
    if(!fixture::step(fixture::phase?"capture-after":"capture-before",error))return false;
    fixture::need(previous&&previous->local,"Engine lost complete Quest seed before full sample");
    if(fixture::contained&&fixture::expected_full_seed){
        fixture::need(previous->local.get()==fixture::expected_full_seed,"Contained Engine did not pass the helper's exact Quest seed into full capture");
        fixture::expected_full_seed=nullptr;
    }
    out=current();if(fixture::bad_after)out.value.references=std::string(64,'f');return true;
}
bool prepare_children(const action::Binding&,const std::string&,const action::Request&,Snapshot& out,std::string& error,const Snapshot* previous=nullptr){
    if(fixture::contained){
        const bool history=fixture::request.action==action::Action::UndoBuilding||fixture::request.action==action::Action::RedoBuilding;
        fixture::need((previous!=nullptr)==history,"Contained Engine skipped or invented consumed inverse seed");
        if(history)fixture::need(previous->local->history_source==fixture::expected_consumed,"Contained Engine prepared children from a different consumed inverse");
    }
    if(!fixture::step("prepare-children",error))return false;out=current();
    if(fixture::contained){out.value={};fixture::expected_full_seed=out.local.get();}return true;
}
bool prepare_history_seed(const action::Binding&,const std::string&,const Snapshot& consumed,Snapshot& out,std::string& error){
    if(fixture::contained){
        fixture::need(consumed.local&&consumed.local.get()==fixture::expected_consumed,"History seed lost exact consumed retained snapshot");
        ++fixture::history_seed_calls;if(!fixture::step("history-seed",error))return false;
        out=consumed;out.local=std::make_shared<Local>(*consumed.local);out.local->history_source=consumed.local.get();return true;
    }
    throw std::runtime_error("Placement fixture unexpectedly entered history seed");
}
bool publish_children(const action::Binding&,const std::string&,const action::Request&,const Snapshot& before,
                      const Snapshot* consumed,const std::vector<uintptr_t>& nodes,Snapshot& out,std::string& error){
    if(fixture::contained){
        const bool history=fixture::request.action==action::Action::UndoBuilding||fixture::request.action==action::Action::RedoBuilding;
        fixture::need((consumed!=nullptr)==history&&(!consumed||consumed->local.get()==fixture::expected_consumed),
            "Child publication lost consumed inverse lifetime");
        fixture::need(nodes==std::vector<uintptr_t>{fixture::current_graph+0x1000},"Contained child publication lost exact current original operation array");
        fixture::need(before.local&&before.value==fixture::effect.before,"Contained child publication lost complete original BEFORE witness");
        ++fixture::child_publications;
    }else fixture::need(!consumed&&nodes==std::vector<uintptr_t>{0x7000},"Engine lost actual child inverse operation list");
    if(!fixture::step("publish-children",error))return false;out=current();
    if(fixture::contained){out.value={};fixture::expected_full_seed=out.local.get();}return true;
}
bool seed(const action::Binding&,const std::string&,const std::array<uint64_t,4>& random,std::string& error){
    if(!fixture::step("seed",error))return false;
    (fixture::phase?fixture::effect.after:fixture::effect.before).random=random;return true;
}
bool read_owners(const Snapshot& sample,dungeon_scene_identity_runtime::Census&,std::vector<BuildingOwner>& out,std::string& error){
    if(!sample.local){error="fixture local owners absent";return false;}out=sample.local->owners;return true;
}
}
namespace dungeon_building_graph {
struct Cash {int32_t amount=0;};
struct Proof {std::string sha;uintptr_t held_node=0;std::vector<Cash> cash;};
bool owner(const npc_probe::dungeon_building::Snapshot& sample,const std::string& key,
           npc_probe::dungeon_building::BuildingOwner& out,std::string& error){
    for(const auto& row:sample.local->owners)if(row.key==key){out=row;return true;}
    error="fixture exact target absent";return false;
}
bool held(const npc_probe::dungeon_building::Snapshot&,const std::string&,uintptr_t,uintptr_t&,std::string&){
    throw std::runtime_error("Placement fixture unexpectedly entered held Dungeon destruction");
}
bool operations(uintptr_t graph,std::vector<uintptr_t>& out,std::string& error){
    fixture::need(graph==(fixture::contained?fixture::current_graph:0x6000),"Engine lost original current inverse graph");
    if(!fixture::step("inverse-operations",error))return false;out={graph+0x1000};return true;
}
bool read(const npc_probe::dungeon_building::Snapshot& sample,const std::string& key,uintptr_t graph,bool destroying,bool moving,
          const action::BuildingState& destination,Proof& out,std::string& error){
    if(fixture::contained){
        const auto expected=fixture::graphs.find(graph);
        fixture::need(expected!=fixture::graphs.end()&&key==fixture::contained_key&&!moving&&
            destroying==expected->second.destroying&&destination==expected->second.destination&&
            sample.local&&sample.local->owners.size()==2&&sample.value==expected->second.value,
            "Contained Engine lost complete retained inverse owner/value/direction/destination");
        fixture::need(sample.local->owners.back().key==fixture::contained_key&&
            sample.local->owners.back().scene==std::string("action:")+fixture::dto::source_id+"/template"&&
            sample.local->owners.back().uid==0x200000007ULL,"Contained inverse proof lost saved local full UID/scene/key");
        if(!fixture::step("inverse",error))return false;out={expected->second.sha,0,{{expected->second.cash}}};return true;
    }
    fixture::need(graph==0x6000&&destroying&&!moving&&sample.local&&sample.local->owners.size()==2,
                  "Engine did not prove the original complete inverse against new current owners");
    if(!fixture::step("inverse",error))return false;out={fixture::effect.inverse,0,{{50}}};return true;
}
}
namespace typed_action_runtime {nlohmann::json status(){return {};}}
namespace ui_move_runtime::contract {
bool pending(const nlohmann::json&,const action::Request&,uint64_t& revision,std::string& error){
    if(!fixture::step("pending",error))return false;revision=fixture::requested_revision;return true;
}
}
namespace building_history_slice {
bool graph_for(action::Action direction,const std::string& source,uintptr_t& graph,std::string& error){
    if(fixture::contained){
        const bool history=fixture::request.action==action::Action::UndoBuilding||fixture::request.action==action::Action::RedoBuilding;
        fixture::need(source==(history?fixture::request.target_action_uuid:fixture::request.action_uuid),"Contained history used object key instead of source action UUID");
        const auto expected_direction=fixture::phase?(fixture::request.action==action::Action::UndoBuilding? action::Action::RedoBuilding:action::Action::UndoBuilding):fixture::request.action;
        fixture::need(direction==expected_direction,"Contained history selected wrong consumed/current graph direction");
        if(!fixture::step("graph",error))return false;
        graph=fixture::phase?fixture::current_graph:(fixture::request.action==action::Action::UndoBuilding?0x6000:0x6100);return true;
    }
    fixture::need(direction==action::Action::UndoBuilding&&source==fixture::request.action_uuid,
                  "Engine asked for a different native history source");
    if(!fixture::step("graph",error))return false;graph=0x6000;return true;
}
}
namespace ui_build_runtime {
bool complete_dungeon_local_ui(const action::Request&,uintptr_t object,uint64_t uid,std::string& error){
    fixture::need(object==0x2000&&uid==0x200000007ULL,"Engine completed local UI for a different actual owner");
    return fixture::step("local-ui",error);
}
}
struct Party {
    bool begin_dungeon_building(const action::Request&,std::string& error){return fixture::step("begin",error);}
    bool cancel_dungeon_building(const action::Request&,std::string& error){return fixture::step("cancel",error);}
    bool finish_dungeon_building(const action::Request&,const mt2mp::game::scene_clone::Mapping* authority,
         mt2mp::game::scene_clone::Mapping& out,bool& attempted,std::string& error,uintptr_t held){
        fixture::need(held==0&&((authority!=nullptr)==fixture::client),"Engine lost exact Party authority");
        attempted=true;if(!fixture::step("mapping",error))return false;out=fixture::effect.mapping;return true;
    }
};
struct NativeEngine {
    struct DungeonOperation {
        action::Request request;action::Binding binding;action::Ledger ledger_before,ledger_after;
        npc_probe::dungeon_building::Snapshot before,after;
        action::DungeonBuildingEffect effect;npc_probe::dungeon_building::Snapshot inverse_sample;
        action::BuildingState inverse_destination;uintptr_t inverse_graph=0;
        bool inverse_destroying=false,inverse_moving=false,confirmed=false,retired=false,removal_finalized=false;
    };
    struct Building {action::BuildingState object_before,state;};
    struct Removed {action::BuildingState object_before;};
    struct History {action::BuildingState object_before,object_after;};
    std::map<std::string,std::unique_ptr<DungeonOperation>> dungeon_buildings_;
    std::map<std::string,Building> buildings_;
    std::map<std::string,Removed> operations_;
    std::map<std::string,History> history_;
    world_state::Table world_;Party party;Party* live_state_=&party;
    const action::Request* active_dungeon_building_=nullptr;
    bool current(const action::Binding&,const std::string&,std::string& error){return fixture::step("current",error);}
    bool read_ledger(action::Ledger& value,std::string& error){
        if(!fixture::step(fixture::phase?"ledger-after":"ledger-before",error))return false;value=fixture::ledger;return true;
    }
    action::Outcome perform_place(const action::Binding&,const action::Request& request,const action::Evidence*,std::string& error){
        fixture::need(active_dungeon_building_&&*active_dungeon_building_==request,"Original ran outside exact prepared scope");
        if(fixture::fail_at=="reject-original"){error="original rejected before effect";return action::Outcome::RejectedNoEffect;}
        ++fixture::originals;fixture::calls.push_back("original");fixture::phase=1;
        buildings_[request.action_uuid]={{},request.building};
        fixture::ledger.cash-=50;fixture::ledger.record_cash-=50;
        if(fixture::fail_at=="original"){error="original partial effect";return action::Outcome::Unknown;}
        return action::Outcome::Confirmed;
    }
    action::Outcome perform_move(const action::Binding&,const action::Request&,const action::Evidence*,std::string&){throw std::runtime_error("unexpected Move");}
    action::Outcome contained_original(const action::Request& request,bool restoring,std::string& error){
        fixture::need(fixture::contained&&active_dungeon_building_&&*active_dungeon_building_==request,
            "Contained original executed outside exact prepared request scope");
        if(fixture::fail_at=="reject-original"){error="original rejected before effect";return action::Outcome::RejectedNoEffect;}
        fixture::need(fixture::target_alive!=restoring,"Contained original target lifetime did not alternate");
        ++fixture::originals;fixture::calls.push_back("original");fixture::phase=1;
        fixture::target_alive=restoring;if(restoring)fixture::target_pointer=0x3000;
        const int32_t delta=restoring?-25:25;fixture::ledger.cash+=delta;fixture::ledger.record_cash+=delta;
        fixture::ledger.record_income+=delta;++fixture::ledger.line_counts[6];fixture::ledger.line_income[6]+=delta;
        if(request.action==action::Action::RemoveSnapshotBuilding)operations_[request.action_uuid]={fixture::dto::object(false)};
        else {auto before=fixture::dto::object(false);before.alive=!restoring;auto after=before;after.alive=restoring;
            history_[request.action_uuid]={before,after};}
        fixture::GraphExpectation proof;proof.value=restoring?fixture::effect.after:fixture::effect.before;
        proof.destination=fixture::dto::object(false);proof.destination.alive=!restoring;
        proof.sha=fixture::effect.inverse;proof.cash=-delta;proof.destroying=restoring;fixture::graphs[fixture::current_graph]=proof;
        if(fixture::fail_at=="original"){error="original partial contained effect";return action::Outcome::Unknown;}
        return action::Outcome::Confirmed;
    }
    action::Outcome perform_remove(const action::Binding&,const action::Request& request,action::ui_capture::ReplayPurpose purpose,
        const action::Evidence* authority,std::string& error){
        if(!fixture::contained)throw std::runtime_error("unexpected Remove");
        fixture::need(purpose==(fixture::client?action::ui_capture::ReplayPurpose::ConfirmedHostApplication:action::ui_capture::ReplayPurpose::HostExecution)&&
            ((authority!=nullptr)==fixture::client),"Contained Remove lost actual Host/Client original purpose");
        return contained_original(request,false,error);
    }
    action::Outcome perform_building_history(const action::Binding&,const action::Request& request,const action::Evidence* authority,std::string& error){
        if(!fixture::contained)throw std::runtime_error("unexpected history");
        fixture::need(((authority!=nullptr)==fixture::client)&&request.target_action_uuid==fixture::dto::action_id,
            "Contained history lost original Remove action source");
        return contained_original(request,request.action==action::Action::UndoBuilding,error);
    }
    bool retire_base(const action::Binding&,const action::Request&,std::string& error){
        ++fixture::retirements;return fixture::step("retire-original",error);
    }
    bool observe_action_base(const action::Binding&,const action::Request& request,action::Evidence& out,std::string& error){
        if(!fixture::step("observe-original",error))return false;
        const auto& saved=*dungeon_buildings_.at(request.action_uuid);out.before=saved.ledger_before;out.after=fixture::ledger;return true;
    }
#include "dungeon_building_engine.inc"
};
namespace fixture {
std::unique_ptr<NativeEngine> setup(bool peer){
    fail_at.clear();calls.clear();originals=retirements=phase=0;client=peer;bad_after=false;
    contained=target_alive=bad_census=false;target_pointer=0x2000;current_graph=0x6000;requested_revision=1;
    expected_consumed=expected_full_seed=nullptr;history_seed_calls=child_publications=0;graphs.clear();
    request=dto::request(action::Action::PlaceBuilding,true);ledger=dto::money();effect=dto::effect(request);
    auto engine=std::make_unique<NativeEngine>();std::string error;
    const world_state::Origin origin{dto::snapshot_id,std::string(64,'a'),1,2};
    need(engine->world_.bind_snapshot(origin,{{std::string("action:")+dto::source_id,origin.sha256+":world:building:1",dto::object(true)}},0,error),error);
    return engine;
}
std::unique_ptr<action::Evidence> authority(){
    auto value=std::make_unique<action::Evidence>();value->before=dto::money();value->after=value->before;
    value->after.cash-=50;value->after.record_cash-=50;value->dungeon_building_effect=effect;return value;
}
void contained_request(action::Action kind){
    request=dto::request(kind,false);request.canonical_record_key=contained_key;
    request.action_uuid=kind==action::Action::RemoveSnapshotBuilding?dto::action_id:
        kind==action::Action::UndoBuilding?"55555555-5555-4555-8555-555555555555":"66666666-6666-4666-8666-666666666666";
    if(kind==action::Action::UndoBuilding||kind==action::Action::RedoBuilding)request.target_action_uuid=dto::action_id;
    phase=0;calls.clear();fail_at.clear();expected_consumed=expected_full_seed=nullptr;requested_revision=kind==action::Action::RemoveSnapshotBuilding?1:
        kind==action::Action::UndoBuilding?2:3;
    current_graph=kind==action::Action::RemoveSnapshotBuilding?0x6000:kind==action::Action::UndoBuilding?0x6100:0x6200;
    effect=dto::effect(request);const auto exterior=std::string("action:")+dto::source_id;
    const bool restoring=kind==action::Action::UndoBuilding;
    effect.before.authored.at(exterior)=std::string(64,target_alive?'a':'b');
    effect.after.authored.at(exterior)=std::string(64,restoring?'a':'b');
    effect.before.quests=std::string(64,target_alive?'c':'d');effect.after.quests=std::string(64,restoring?'c':'d');
    effect.before.references=std::string(64,target_alive?'e':'f');effect.after.references=std::string(64,restoring?'e':'f');
    effect.inverse=std::string(64,kind==action::Action::RemoveSnapshotBuilding?'e':restoring?'d':'c');
    effect.mapping={};effect.mapping.operation=request.action_uuid;
    mt2mp::game::scene_clone::SceneCount count;count.scene=exterior+"/template";count.buildings=restoring?1:0;
    effect.mapping.scenes.push_back(count);
    if(restoring)effect.mapping.members.push_back({count.scene,contained_key,mt2mp::game::scene_clone::Family::Building,0,0,dto::source_id});
    std::string error;need(mt2mp::game::scene_clone::valid(effect.mapping,error),error);
}
std::unique_ptr<NativeEngine> setup_contained(bool peer){
    auto engine=setup(peer);contained=true;target_alive=true;contained_request(action::Action::RemoveSnapshotBuilding);
    engine=std::make_unique<NativeEngine>();std::string error;
    const world_state::Origin origin{dto::snapshot_id,std::string(64,'a'),1,2};
    need(engine->world_.bind_snapshot(origin,{
        {std::string("action:")+dto::source_id,origin.sha256+":world:building:1",dto::object(true)},
        {contained_key,origin.sha256+":world:building:inline:region{1}/buildingManager/building[0]/template/buildingManager/building[0]",dto::object(false)}},0,error),error);
    return engine;
}
std::unique_ptr<action::Evidence> contained_authority(){
    auto out=std::make_unique<action::Evidence>();out->before=out->after=ledger;
    const int32_t delta=request.action==action::Action::UndoBuilding?-25:25;
    out->after.cash+=delta;out->after.record_cash+=delta;out->after.record_income+=delta;
    ++out->after.line_counts[6];out->after.line_income[6]+=delta;out->dungeon_building_effect=effect;return out;
}
void consume(NativeEngine& engine,const std::string& source){expected_consumed=engine.dungeon_buildings_.at(source)->inverse_sample.local.get();}
void contained_cases(){
    for(bool peer:{false,true}){
        auto engine=setup_contained(peer);std::string error;
        const auto initial_ledger=ledger;
        for(auto kind:{action::Action::RemoveSnapshotBuilding,action::Action::UndoBuilding,action::Action::RedoBuilding}){
            if(kind!=action::Action::RemoveSnapshotBuilding){contained_request(kind);
                consume(*engine,kind==action::Action::UndoBuilding?dto::action_id:"55555555-5555-4555-8555-555555555555");}
            const auto original_count=originals,seed_count=history_seed_calls,publication_count=child_publications;
            auto host=contained_authority();error.clear();
            need(engine->perform_dungeon_building({1,2},request,peer?host.get():nullptr,error)==action::Outcome::Confirmed,error);
            need(originals==original_count+1&&child_publications==publication_count+1&&!engine->active_dungeon_building_,
                "contained original or child publication did not occur exactly once");
            const bool restoring=kind==action::Action::UndoBuilding;
            need(engine->world_.revision()==requested_revision&&engine->world_.rows().size()==2&&
                 engine->world_.rows().at(contained_key).state.alive==restoring&&
                 engine->world_.rows().at(std::string("action:")+dto::source_id).state==dto::object(true),
                 "complete World census lost target tombstone/restoration or unrelated exterior");
            need(ledger==host->after,"full signed category6 and all other Ledger values differ");
            need(history_seed_calls==seed_count+(kind==action::Action::RemoveSnapshotBuilding?0:1),
                "history did not consume exactly one retained child/Quest seed");
            const auto at=[&](const char* name){return std::find(calls.begin(),calls.end(),name)-calls.begin();};
            need(at("prepare-children")<at("capture-before")&&at("capture-before")<at("begin")&&
                 at("begin")<at("original")&&at("original")<at("mapping")&&at("mapping")<at("publish-children")&&
                 at("publish-children")<at("capture-after"),"contained complete original/alias/Quest ordering differs");
            if(kind!=action::Action::RemoveSnapshotBuilding)need(at("inverse")<at("history-seed")&&at("history-seed")<at("prepare-children"),
                "consumed graph was not proved before preparing its current Quest seed");
            if(restoring)need(target_pointer==0x3000&&engine->dungeon_buildings_.at(request.action_uuid)->after.local->owners.back().object==0x3000,
                "restoration reused old receiver instead of original new receiver/same saved UID");
            auto observed=std::make_unique<action::Evidence>();error.clear();
            need(engine->observe_dungeon_building({1,2},request,*observed,error)&&observed->dungeon_building_effect==effect&&
                 observed->before==host->before&&observed->after==host->after,error);
            const auto retired_before=retirements;
            need(engine->retire_dungeon_building({1,2},request,error)&&retirements==retired_before+(kind==action::Action::RemoveSnapshotBuilding?0:1),
                 "normal Remove base retirement replayed or history retirement missing");
            need(engine->observe_dungeon_building({1,2},request,*observed,error)&&observed->retirement_confirmed,error);
            const auto retired_once=retirements;
            need(!engine->retire_dungeon_building({1,2},request,error)&&retirements==retired_once,"contained original retirement replayed");
            need(engine->perform_dungeon_building({1,2},request,peer?host.get():nullptr,error)==action::Outcome::Unknown&&originals==original_count+1,
                 "confirmed contained original replayed");
            if(kind==action::Action::UndoBuilding)need(engine->dungeon_buildings_.at(dto::action_id)->inverse_graph==0,"Undo retained consumed stale original graph");
            if(kind==action::Action::RedoBuilding)need(engine->dungeon_buildings_.at("55555555-5555-4555-8555-555555555555")->inverse_graph==0,
                "Redo retained consumed stale Undo graph");
        }
        need(originals==3&&retirements==3&&ledger.cash==initial_ledger.cash+25&&ledger.line_counts[6]==3&&ledger.line_income[6]==25,
            "Remove/Undo/Redo did not retain exact original count and signed full accounting");
    }
    for(const std::string failure:{"history-seed","prepare-children","publish-children"}){
        auto engine=setup_contained(false);std::string error;
        need(engine->perform_dungeon_building({1,2},request,nullptr,error)==action::Outcome::Confirmed,error);
        need(engine->retire_dungeon_building({1,2},request,error),error);
        contained_request(action::Action::UndoBuilding);consume(*engine,dto::action_id);fail_at=failure;error.clear();
        const bool partial=failure=="publish-children";
        need(engine->perform_dungeon_building({1,2},request,nullptr,error)==(partial?action::Outcome::Unknown:action::Outcome::RejectedNoEffect)&&
             originals==(partial?2u:1u)&&engine->world_.revision()==1&&error=="original fixture failure: "+failure,
             "history child failure lost exact cause or published wrong outcome");
        if(partial){fail_at.clear();need(engine->perform_dungeon_building({1,2},request,nullptr,error)==action::Outcome::Unknown&&originals==2,
            "partially restored contained original retried");}
    }
    {
        auto engine=setup_contained(true);auto host=contained_authority();++host->before.line_counts[19];std::string error;
        need(engine->perform_dungeon_building({1,2},request,host.get(),error)==action::Outcome::RejectedNoEffect&&originals==0,
            "contained Client ignored untouched complete BEFORE Ledger field");
    }
    {
        auto engine=setup_contained(true);auto host=contained_authority();++host->after.line_income[19];std::string error;
        need(engine->perform_dungeon_building({1,2},request,host.get(),error)==action::Outcome::Unknown&&originals==1&&engine->world_.revision()==0,
            "contained Client published mismatching complete AFTER Ledger");
    }
    {
        auto engine=setup_contained(false);bad_census=true;std::string error;
        need(engine->perform_dungeon_building({1,2},request,nullptr,error)==action::Outcome::Unknown&&originals==1&&engine->world_.revision()==0,
            "contained action published changed unrelated World object");
    }
    {
        auto engine=setup_contained(true);auto host=contained_authority();host->dungeon_building_effect->after.references=std::string(64,'0');std::string error;
        need(engine->perform_dungeon_building({1,2},request,host.get(),error)==action::Outcome::Unknown&&originals==1&&engine->world_.revision()==0,
            "contained Client published different complete active Quest references");
    }
}
void cases(){
    for(bool peer:{false,true}){
        auto engine=setup(peer);auto host=authority();std::string error;
        need(engine->perform_dungeon_building({1,2},request,peer?host.get():nullptr,error)==action::Outcome::Confirmed,error);
        need(originals==1&&engine->world_.revision()==1&&engine->world_.rows().size()==2&&!engine->active_dungeon_building_,"complete native action did not publish exactly once");
        const auto at=[&](const char* name){return std::find(calls.begin(),calls.end(),name)-calls.begin();};
        need(at("prepare-children")<at("capture-before")&&at("capture-before")<at("begin")&&at("begin")<at("original")&&
             at("original")<at("mapping")&&at("mapping")<at("publish-children")&&at("publish-children")<at("capture-after")&&
             at("capture-after")<at("inverse"),"complete original/aliases/child/Quest/inverse ordering differs");
        need(engine->perform_dungeon_building({1,2},request,peer?host.get():nullptr,error)==action::Outcome::Unknown&&originals==1,"confirmed original replayed");
        auto observed=std::make_unique<action::Evidence>();
        need(engine->observe_dungeon_building({1,2},request,*observed,error)&&observed->dungeon_building_effect==effect&&!observed->retirement_confirmed,error);
        need(engine->retire_dungeon_building({1,2},request,error)&&retirements==1,error);
        need(engine->observe_dungeon_building({1,2},request,*observed,error)&&observed->retirement_confirmed,error);
        need(!engine->retire_dungeon_building({1,2},request,error)&&retirements==1,"retirement original replayed");
    }
    for(const std::string failure:{"session","pending","ledger-before","prepare-children","capture-before","begin"}){
        auto engine=setup(false);fail_at=failure;std::string error;
        need(engine->perform_dungeon_building({1,2},request,nullptr,error)==action::Outcome::RejectedNoEffect&&originals==0&&
             engine->world_.revision()==0&&error=="original fixture failure: "+failure,"pre-effect failure changed world or lost cause: "+failure);
    }
    for(const std::string failure:{"original","graph","mapping","inverse-operations","publish-children","capture-after","inverse","ledger-after"}){
        auto engine=setup(false);fail_at=failure;std::string error;
        need(engine->perform_dungeon_building({1,2},request,nullptr,error)==action::Outcome::Unknown&&originals==1&&
             engine->world_.revision()==0&&!engine->active_dungeon_building_&&!error.empty(),"partial native effect published or lost scope: "+failure);
        fail_at.clear();need(engine->perform_dungeon_building({1,2},request,nullptr,error)==action::Outcome::Unknown&&originals==1,
                            "partial original retried: "+failure);
    }
    {
        auto engine=setup(true);auto host=authority();host->dungeon_building_effect->before.references=std::string(64,'f');std::string error;
        need(engine->perform_dungeon_building({1,2},request,host.get(),error)==action::Outcome::RejectedNoEffect&&originals==0,
             "Client accepted different complete Quest reference BEFORE");
    }
    {
        auto engine=setup(true);auto host=authority();host->dungeon_building_effect->after.quests=std::string(64,'f');std::string error;
        need(engine->perform_dungeon_building({1,2},request,host.get(),error)==action::Outcome::Unknown&&originals==1&&engine->world_.revision()==0,
             "Client published a different complete original result");
    }
    {
        auto engine=setup(true);auto host=authority();fail_at="reject-original";std::string error;
        need(engine->perform_dungeon_building({1,2},request,host.get(),error)==action::Outcome::RejectedNoEffect&&originals==0&&
             std::count(calls.begin(),calls.end(),"cancel")==1&&std::count(calls.begin(),calls.end(),"seed")==2,
             "No-effect rejection lost prepared scope or Client RNG restoration");
    }
    {
        auto engine=setup(false);std::string error;
        need(engine->perform_dungeon_building({1,2},request,nullptr,error)==action::Outcome::Confirmed,error);
        bad_after=true;auto observed=std::make_unique<action::Evidence>();
        need(!engine->observe_dungeon_building({1,2},request,*observed,error)&&originals==1,"stale complete after state ACKed");
    }
}
}
int main()try{fixture::cases();fixture::contained_cases();std::cout<<fixture::checks<<" actual Dungeon Engine boundary checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}

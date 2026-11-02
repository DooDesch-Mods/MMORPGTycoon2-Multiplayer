#include "typed_action/action_session.h"
#include "typed_action/ordered_building_family.h"
#include "ordered/wall_owner_cases.h"
#include "release_edit_state.h"
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace release_edit=mt2mp::game::release_edit;
using json=nlohmann::json;
bool fixture_ordered=true,scope_ok=true,controller_ordered=true;
action::Context fixture_context;
action::Request fixture_request;
unsigned native_calls=0,checks=0;
void need(bool value,const std::string& error){++checks;if(!value)throw std::runtime_error(error);}
namespace mt2mp::action_session {
struct FixtureSession {bool current(Context& out,std::string&){out=::fixture_context;return true;}};
FixtureSession& selected_session(){static FixtureSession session;return session;}
namespace tick_commands::authority {
bool active(){return ::fixture_ordered;}
bool matches(const Context& actual,const Binding& binding,const Request& request,std::string& error){
    if(!scope_ok||actual.role!=fixture_context.role||binding!=Binding{2,3}||request!=fixture_request){error="Actual consumed authority scope differs";return false;}return true;
}
bool matches(const Binding& binding,const Request& request,std::string& error){return matches(::fixture_context,binding,request,error);}
}
bool network_history(const Request&){return false;}
bool valid_world_path_effect(const Request&,const Evidence&);
}
struct Config {bool is_host=true;static Config& instance(){static Config config;return config;}};
unsigned GetCurrentThreadId(){return 7;}
bool fail(std::string& error,const std::string& why){error=why;return false;}
namespace typed_action_runtime {json controller_status(){return {{"ordered",controller_ordered}};}}
#include "ordered_path_scope_actual.inc"
unsigned unrelated_history_calls=0;
namespace ui_move_runtime {
namespace selection_move {bool history_intent(const action::Binding&,const action::Request&,action::SelectionMoveIntent&,std::string& error){++unrelated_history_calls;error="Unexpected selection route";return false;}}
namespace duplicate_edit {bool history_targets(const action::Binding&,const action::Request&,std::map<uint64_t,std::string>&,std::string& error){++unrelated_history_calls;error="Unexpected duplicate route";return false;}}
namespace group_edit {bool history_admitted(const action::Binding&,const action::Request&,std::string& error){++unrelated_history_calls;error="Unexpected group route";return false;}}
}
struct EngineFixture {
    struct Source{action::Request request;};std::map<std::string,Source> buildings_,operations_;
    bool live_state_=false,live_port_=false,live_result_=false;
    unsigned routed=0;
    action::Outcome execute_action_native(const action::Binding&,const action::Request&,std::string&){++routed;return action::Outcome::Confirmed;}
    action::Outcome execute(const action::Binding& binding,const action::Request& request,std::string& error){
        #include "ordered_path_route_actual.inc"
        error="Nonordered test route";return action::Outcome::RejectedNoEffect;
    }
};
namespace region {
struct Runtime {unsigned thread=7;uintptr_t image=100;int memory=1;};
struct LocalView {struct World{uintptr_t root=200;}world;};
}
namespace mixed_path_native {
struct Census{};
bool read_world(int,uintptr_t,uintptr_t,uintptr_t,Census&,std::string&){return true;}
}
region::Runtime native_runtime;
region::Runtime* runtime(std::string&){return &native_runtime;}
namespace mixed {
struct Snapshot {int graph=0,records=0;friend bool operator==(const Snapshot&,const Snapshot&)=default;};
bool canonical(const Snapshot& value,const Snapshot&,json& out,std::string&){out={{"graph",value.graph},{"records",value.records}};return true;}
bool same(const Snapshot& a,const Snapshot& b,std::string& error){return a==b||fail(error,"Mixed topology/Records differ");}
}
namespace world_path_runtime {
struct Sample {mixed::Snapshot mixed;action::WorldPathState wire;};
struct Operation {mt2mp::game::wall_history_owner::Receipt wall_owners;action::Binding binding;action::Request request;action::Evidence evidence;Sample before,after;bool confirmed=true,retired=true,ordered=true;};
std::map<std::string,Operation> operations;
struct Owner {struct Keys{bool restore(int,const mixed_path_native::Census&,std::string&){return true;}} keys;} owner;
struct Capture{bool ready=true;uintptr_t local_wall=0;std::optional<int> before,captured,pending;std::string last_action,pending_action,cause;} capture;
Sample fixture_current,fixture_result;
}
action::Ledger money,expected_money;
bool native_reject=false,bad_result=false,bad_money=false,graph_ok=true,effect_ok=true;
namespace region {
bool read(Runtime&,int32_t,uintptr_t,LocalView&,std::string&){return true;}
bool bound(const LocalView&,const action::Binding& binding,const std::string& snapshot,std::string& error){
    return (binding==action::Binding{2,3}&&snapshot==fixture_request.snapshot_uuid)||fail(error,"Snapshot/binding differs");
}
bool ledger(action::Ledger& out,std::string&){out=money;return true;}
bool at(Runtime&,uintptr_t,size_t,uintptr_t& out,std::string&){out=300;return true;}
}
namespace ui_move_runtime::contract {
bool pending(const json&,const action::Request& request,uint64_t& revision,std::string& error){
    revision=3;return request==fixture_request||fail(error,"Pending action differs");
}
}
namespace building_history_slice {
bool graph_for(action::Action,const std::string& target,uintptr_t& out,std::string& error){out=400;return (graph_ok&&target==fixture_request.target_action_uuid)||fail(error,"Original exact graph missing");}
bool cash_effect(const action::Ledger& before,int32_t amount,action::Ledger& after,std::string&){after=before;after.cash+=amount;return true;}
action::Outcome perform(action::Action,const std::string&,std::string& error){
    ++native_calls;if(native_reject){error="Original graph rejected";return action::Outcome::RejectedNoEffect;}
    world_path_runtime::fixture_current=world_path_runtime::fixture_result;money=expected_money;
    if(bad_result)++world_path_runtime::fixture_current.mixed.records;
    if(bad_money)++money.cash;return action::Outcome::Confirmed;
}
}
namespace world_path_runtime {
bool world_path_sample(region::Runtime&,const std::string&,int32_t,int32_t,Sample& out,std::string&){out=fixture_current;return true;}
}
namespace mt2mp::action_session {
bool valid_world_path_effect(const Request&,const Evidence&){return effect_ok;}
}
bool owner_restore_ok=true;unsigned owner_restore_calls=0,diagnostic_calls=0;
namespace world_path_runtime {
bool restore_wall_owners(region::Runtime&,uintptr_t,const std::string&,const Sample&,const mixed_path_native::Census&,const mt2mp::game::wall_history_owner::Receipt&,std::string& e){
    ++owner_restore_calls;if(!owner_restore_ok)e="original owner restore failure";return owner_restore_ok;
}
json changed_wall_record(region::Runtime&,uintptr_t,const std::string&,const Sample&,const Sample&){
    ++diagnostic_calls;return {{"state","controlled_record_diagnostic"}};
}
}
#include "world_path_history.inc"

const std::string source_id="0627b09b-72d6-4639-a0dd-9f6baf849cba";
action::Request reset(action::Action direction,bool host=true,bool deterministic=true){
    owner_restore_ok=true;owner_restore_calls=diagnostic_calls=0;fixture_ordered=controller_ordered=deterministic;scope_ok=true;Config::instance().is_host=host;
    fixture_context.role=host?action::Role::Host:action::Role::Client;
    native_runtime={};native_calls=0;native_reject=bad_result=bad_money=false;graph_ok=effect_ok=true;
    world_path_runtime::operations.clear();world_path_runtime::history.clear();world_path_runtime::capture={};
    action::Request request;request.action=direction;request.target_action_uuid=source_id;
    request.action_uuid="33333333-3333-4333-8333-333333333333";
    request.snapshot_uuid="11111111-1111-4111-8111-111111111111";
    request.canonical_record_key="path:"+source_id;fixture_request=request;
    world_path_runtime::Operation source;source.binding={2,3};source.request.action=action::Action::PlaceWall;
    source.request.world_path.kind=action::WorldPathKind::Wall;source.request.action_uuid=source_id;source.request.snapshot_uuid=request.snapshot_uuid;
    source.request.world_path.start_region=source.request.world_path.end_region=67;source.ordered=deterministic;
    source.before.mixed={0,0};source.after.mixed={1,1};source.before.wire.paths=0;source.after.wire.paths=1;
    source.evidence.before.cash=100;source.evidence.after.cash=90;
    const bool undo=direction==action::Action::UndoBuilding;
    world_path_runtime::fixture_current=undo?source.after:source.before;
    world_path_runtime::fixture_result=undo?source.before:source.after;
    money={};money.cash=undo?90:100;expected_money=money;expected_money.cash=undo?100:90;
    world_path_runtime::operations.emplace(source_id,std::move(source));return request;
}
int main(){try{
    std::string error;
    for(auto direction:{action::Action::UndoBuilding,action::Action::RedoBuilding})for(bool host:{true,false}){
        auto request=reset(direction,host);
        EngineFixture engine;need(engine.execute({2,3},request,error)==action::Outcome::Confirmed&&engine.routed==1,error);
        need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Confirmed&&native_calls==1,error);
        action::Evidence evidence;need(world_path_runtime::observe_history({2,3},request,evidence,error),error);
        need(world_path_runtime::retire_history({2,3},request,error),error);
        need(world_path_runtime::observe_history({2,3},request,evidence,error)&&evidence.retirement_confirmed,error);
        need(!world_path_runtime::retire_history({2,3},request,error),"Duplicate retirement accepted");
        need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&native_calls==1,"Native replayed");
    }
    auto reject=[&](auto change){auto request=reset(action::Action::UndoBuilding,false);change(request);
        need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::RejectedNoEffect&&native_calls==0&&!error.empty(),"Invalid history admission accepted: "+error);};
    reject([](auto&){scope_ok=false;});reject([](auto&){fixture_ordered=false;});
    reject([](auto&){fixture_context.role=action::Role::Host;});
    reject([](auto&){native_runtime.thread=8;});reject([](auto&){world_path_runtime::capture.ready=false;});
    reject([](auto&){world_path_runtime::operations.clear();});
    reject([](auto&){world_path_runtime::operations.at(source_id).confirmed=false;});
    reject([](auto&){world_path_runtime::operations.at(source_id).retired=false;});
    reject([](auto&){world_path_runtime::operations.at(source_id).ordered=false;});
    reject([](auto&){world_path_runtime::operations.at(source_id).binding={9,3};});
    reject([](auto&){world_path_runtime::operations.at(source_id).request.snapshot_uuid="wrong";});
    reject([](auto&){world_path_runtime::fixture_current.mixed.graph=99;});
    reject([](auto&){world_path_runtime::fixture_current.mixed.records=99;});
    reject([](auto&){graph_ok=false;});
    auto request=reset(action::Action::UndoBuilding,false);action::Evidence injected;
    need(world_path_runtime::perform_history({2,3},request,&injected,error)==action::Outcome::RejectedNoEffect&&native_calls==0&&error=="Ordered path execution forbids Host result and RNG adoption",error);
    for(int change=0;change<3;++change){request=reset(action::Action::UndoBuilding);bad_result=change==0;bad_money=change==1;effect_ok=change!=2;
        need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&native_calls==1,error);}
    for(auto direction:{action::Action::UndoBuilding,action::Action::RedoBuilding}){
        request=reset(direction,true,false);need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Confirmed,error);
        action::Evidence authority;need(world_path_runtime::observe_history({2,3},request,authority,error),error);
        request=reset(direction,false,false);need(world_path_runtime::perform_history({2,3},request,&authority,error)==action::Outcome::Confirmed,error);
        request=reset(direction,false,false);need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::RejectedNoEffect&&native_calls==0,error);
    }
    request=reset(action::Action::UndoBuilding);EngineFixture engine;
    request.canonical_record_key="path:wrong";fixture_request=request;
    need(engine.execute({2,3},request,error)==action::Outcome::RejectedNoEffect&&engine.routed==0&&error=="Ordered Building history requires one exact original mutation",error);
    request.target_action_uuid.clear();request.canonical_record_key="path:";fixture_request=request;
    need(engine.execute({2,3},request,error)==action::Outcome::RejectedNoEffect&&engine.routed==0,error);
    request=reset(action::Action::UndoBuilding);request.canonical_record_key="building:existing";fixture_request=request;
    need(engine.execute({2,3},request,error)==action::Outcome::RejectedNoEffect&&engine.routed==0,error);
    auto source=request;source.action=action::Action::PlaceBuilding;source.action_uuid=request.target_action_uuid;
    engine.buildings_[source.action_uuid]={source};need(engine.execute({2,3},request,error)==action::Outcome::Confirmed&&engine.routed==1,error);
    engine.operations_[source.action_uuid]={source};need(engine.execute({2,3},request,error)==action::Outcome::RejectedNoEffect&&engine.routed==1,error);
    request=reset(action::Action::RedoBuilding);bad_result=true;
    need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&native_calls==1,error);
    need(error.find("Mixed topology/Records differ")!=std::string::npos&&error.find("/records")!=std::string::npos&&error.find("difference_count")!=std::string::npos,error);
    request=reset(action::Action::RedoBuilding);bad_money=true;
    need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&native_calls==1,error);
    need(error.find("native ledger differs")!=std::string::npos&&error.find("ledger_matches")!=std::string::npos,error);
    request=reset(action::Action::RedoBuilding);bad_result=true;
    need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&diagnostic_calls==1,error);
    request=reset(action::Action::UndoBuilding);bad_result=true;
    need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&diagnostic_calls==0,error);
    request=reset(action::Action::RedoBuilding);bad_result=true;
    world_path_runtime::operations.at(source_id).request.world_path.kind=action::WorldPathKind::Pass;
    need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&diagnostic_calls==0,error);
    request=reset(action::Action::RedoBuilding);owner_restore_ok=false;
    need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&native_calls==1&&owner_restore_calls==1&&error=="original owner restore failure",error);
    need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&native_calls==1&&owner_restore_calls==1,"Owner completion replayed failed original history");
    request=reset(action::Action::UndoBuilding);
    need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Confirmed&&owner_restore_calls==0,error);
    request=reset(action::Action::RedoBuilding,true,false);
    need(world_path_runtime::perform_history({2,3},request,nullptr,error)==action::Outcome::Confirmed&&owner_restore_calls==0,error);
    need(unrelated_history_calls==0,"Path history invoked an unrelated edit route");
    const auto owner_checks=wall_owner_fixture::run();
    std::cout<<checks<<" actual Wall history checks and "<<owner_checks<<" owner cases passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

#include "typed_action/action_session.h"
#include "path_history_graph.h"
#include "move_contract.h"
#include "live_host_state/economic_fence.h"
#include <cstdio>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace path_history_graph=mt2mp::game::path_history_graph;
namespace ui_move_runtime {namespace contract=mt2mp::game::ui_move_runtime::contract;}
using json=nlohmann::json;
namespace {
const std::string snapshot_id="11111111-1111-4111-8111-111111111111",source_id="22222222-2222-4222-8222-222222222222",history_id="33333333-3333-4333-8333-333333333333";
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
unsigned GetCurrentThreadId(){return 7;}
struct Graph {action::PathState state;std::vector<uint64_t> path_uids,node_uids;json rows;};
struct Operation {action::Binding binding;action::Request request;action::Evidence evidence;
    bool confirmed=true,retired=true;std::optional<path_history_graph::Snapshot> history_before,history_after;uintptr_t history_graph=100;};
struct Path {bool ready=true;std::map<std::string,Operation> operations;} path;
namespace region {
struct Runtime {unsigned thread=7;};
struct LocalView {uintptr_t region=10,random=20;action::RegionState state;};
}
region::Runtime native_runtime;
Graph current_graph,result_graph;
action::Ledger fixture_money,fixture_expected_money;
action::RegionState current_region;
action::TerrainState current_terrain;
json controller;
unsigned calls=0,seeds=0;bool native_reject=false,bad_effect=false,bad_cash=false,bad_inverse=false;
void need(bool value,const std::string& error){if(!value)throw std::runtime_error(error);}
Graph graph_from(const path_history_graph::Snapshot& s){return {{s.roads.size(),s.nodes.size(),std::string(64,'a'),0},s.roads,s.nodes,s.rows};}
region::Runtime* runtime(std::string&){return &native_runtime;}
bool graph(region::Runtime&,const region::LocalView&,Graph& out,std::string&){out=current_graph;return true;}
bool native_error(uint32_t status,const char* name,std::string& error){if(status){error=name;return false;}return true;}
namespace region {
bool read(Runtime&,int32_t id,uintptr_t,LocalView& out,std::string& error){
    if(id!=56){error="native Region missing";return false;}out={10,20,current_region};return true;
}
bool bound(const LocalView&,const action::Binding& binding,const std::string& snapshot,std::string& error){
    if(binding!=action::Binding{2,3}||snapshot!=snapshot_id){error="native snapshot/binding differs";return false;}return true;
}
bool ledger(action::Ledger& out,std::string&){out=fixture_money;return true;}
bool sha256(const std::string& value,std::string& out,std::string&){
    uint64_t sum=1469598103934665603ULL;for(unsigned char c:value){sum^=c;sum*=1099511628211ULL;}
    char body[65];std::snprintf(body,sizeof body,"%064llx",static_cast<unsigned long long>(sum));out=body;return true;
}
}
namespace typed_action_runtime {json controller_status(){return controller;}}
namespace terrain {
bool terrain_state(region::Runtime&,const region::LocalView&,action::TerrainState& out,uintptr_t& global,std::string&){out=current_terrain;global=30;return true;}
uint32_t seed(uintptr_t,uintptr_t,const action::Evidence& authority){++seeds;current_region.random=authority.region_before.random;current_terrain.global_random=authority.terrain_before.global_random;return 0;}
}
namespace building_history_slice {
bool graph_for(action::Action,const std::string&,uintptr_t& out,std::string& error){
    if(calls&&bad_inverse){error="native inverse missing";return false;}out=calls?200:100;return true;
}
bool cash_effect(const action::Ledger& before,int32_t amount,action::Ledger& out,std::string& error){
    if(!amount){out=before;error.clear();return true;}
    return mt2mp::game::live_host_state::predicted_cash(before,false,amount,6,out,error);
}
action::Outcome perform(action::Action,const std::string&,std::string& error){
    ++calls;if(native_reject){error="native exact graph refused";return action::Outcome::RejectedNoEffect;}
    current_graph=result_graph;fixture_money=fixture_expected_money;
    if(bad_effect){if(current_graph.path_uids.empty())current_graph.node_uids.push_back(99);else current_graph.rows["paths"][0][2][0][0]=99;}
    if(bad_cash)++fixture_money.cash;return action::Outcome::Confirmed;
}
}
namespace world_path_runtime {
bool owns_history(const std::string&){return false;}
action::Outcome perform_history(const action::Binding&,const action::Request&,const action::Evidence*,std::string& e){e="unexpected World Path history";return action::Outcome::RejectedNoEffect;}
bool observe_history(const action::Binding&,const action::Request&,action::Evidence&,std::string& e){e="unexpected World Path history observation";return false;}
bool retire_history(const action::Binding&,const action::Request&,std::string& e){e="unexpected World Path history retirement";return false;}
}
#include "path_history_runtime.inc"

action::Request reset(action::Action direction,bool source_remove=false){
    native_runtime={};path={};history_operations.clear();Config::instance().is_host=true;
    calls=seeds=0;native_reject=bad_effect=bad_cash=bad_inverse=false;
    const path_history_graph::Snapshot empty{{},{},{{"paths",json::array()},{"nodes",json::array()}}};
    const path_history_graph::Snapshot road{{11},{21,22},{{"paths",json::array({json::array({0,1,json::array({{0,0,0},{1,0,0}}),json::array({0,1}),json::array(),json::array()})})},
        {"nodes",json::array({json::array({{0,0,0},{0}}),json::array({{1,0,0},{0}})})}}};
    Operation source;source.binding={2,3};source.request.action=source_remove?action::Action::RemovePath:action::Action::PlacePath;
    source.request.action_uuid=source_id;source.request.snapshot_uuid=snapshot_id;source.request.path.region=source.request.path_delete.region=56;
    source.history_before=source_remove?road:empty;source.history_after=source_remove?empty:road;source.evidence.path_after.cost=100;
    const bool undo_direction=direction==action::Action::UndoBuilding;
    current_graph=graph_from(undo_direction?*source.history_after:*source.history_before);
    result_graph=graph_from(undo_direction?*source.history_before:*source.history_after);
    path.operations.emplace(source_id,std::move(source));
    current_region={};current_region.id=56;current_region.preset="Forest";current_region.styles={"Forest","Forest","Forest","Forest"};
    current_region.scenery_sha256=current_region.grid_sha256=std::string(64,'a');
    current_terrain={};current_terrain.contours_sha256=current_terrain.bugs_sha256=std::string(64,'a');
    fixture_money={};fixture_money.cash=fixture_money.record_cash=1000;fixture_money.record_present=1;fixture_money.flat_day=fixture_money.record_day=1;fixture_expected_money=fixture_money;
    const int32_t amount=((!source_remove)==undo_direction)?100:-100;
    fixture_expected_money.cash+=amount;fixture_expected_money.record_cash+=amount;fixture_expected_money.line_counts[6]=1;fixture_expected_money.line_income[6]=amount;
    fixture_expected_money.record_income=amount;
    action::Request request;request.action=direction;request.action_uuid=history_id;request.target_action_uuid=source_id;
    request.snapshot_uuid=snapshot_id;request.canonical_record_key="path:"+source_id;
    controller={{"started",true},{"pending",true},{"locked",false},{"action_uuid",history_id},{"host_revision",uint64_t{5}},{"snapshot_uuid",snapshot_id}};
    return request;
}
}
int main(){try{
    unsigned cases=0;std::string error;
    for(auto direction:{action::Action::UndoBuilding,action::Action::RedoBuilding})for(bool removed:{false,true}){
        auto request=reset(direction,removed);
        need(perform_history({2,3},request,nullptr,error)==action::Outcome::Confirmed&&calls==1,error);++cases;
        action::Evidence authority;need(observe_history({2,3},request,authority,error)&&authority.after==fixture_expected_money,error);++cases;
        need(retire_history({2,3},request,error)&&!retire_history({2,3},request,error),"Path history duplicate retirement accepted");++cases;
        need(perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&calls==1,"Path history replayed");++cases;
        request=reset(direction,removed);Config::instance().is_host=false;current_region.random.fill(91);current_terrain.global_random.fill(92);
        need(perform_history({2,3},request,&authority,error)==action::Outcome::Confirmed&&calls==1&&seeds==1,error);++cases;
        need(observe_history({2,3},request,authority,error),error);current_terrain.global_random.fill(99);
        need(observe_history({2,3},request,authority,error),"Later observational RNG invalidated Path history");++cases;
        ++fixture_money.cash;need(!observe_history({2,3},request,authority,error),"Later cash drift hidden");++cases;
    }
    auto reject=[&](auto change){auto request=reset(action::Action::UndoBuilding);change(request);
        need(perform_history({2,3},request,nullptr,error)==action::Outcome::RejectedNoEffect&&calls==0&&!error.empty(),"Invalid Path history admission accepted");++cases;};
    reject([](auto&){native_runtime.thread=8;});reject([](auto&){path.ready=false;});
    reject([](auto&){controller["pending"]=false;});reject([](auto& r){r.snapshot_uuid="wrong";});
    reject([](auto& r){r.canonical_record_key="path:wrong";});reject([](auto&){path.operations.at(source_id).confirmed=false;});
    reject([](auto&){path.operations.at(source_id).retired=false;});reject([](auto&){current_graph.path_uids[0]=99;});
    reject([](auto&){current_graph.rows["paths"][0][2][0][0]=99;});
    auto request=reset(action::Action::UndoBuilding);bad_effect=true;
    need(perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&calls==1,"Bad native topology accepted");++cases;
    request=reset(action::Action::UndoBuilding);bad_cash=true;
    need(perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&calls==1,"Bad native cash accepted");++cases;
    request=reset(action::Action::UndoBuilding);bad_inverse=true;
    need(perform_history({2,3},request,nullptr,error)==action::Outcome::Unknown&&calls==1,"Missing native inverse accepted");++cases;
    request=reset(action::Action::UndoBuilding);need(perform_history({2,3},request,nullptr,error)==action::Outcome::Confirmed,error);
    action::Evidence authority;need(observe_history({2,3},request,authority,error),error);
    request=reset(action::Action::UndoBuilding);Config::instance().is_host=false;native_reject=true;
    need(perform_history({2,3},request,&authority,error)==action::Outcome::Unknown&&calls==1&&seeds==1,"Client seed then native reject was disguised as no effect");++cases;
    std::cout<<cases<<" actual Path history orchestration comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

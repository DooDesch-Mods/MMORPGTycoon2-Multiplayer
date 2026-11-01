#include "vendor/nlohmann/json.hpp"
#include <iostream>
#include <map>
#include <cmath>
using json=nlohmann::json;using DWORD=unsigned long;
int checks=0;void check(bool v){++checks;if(!v)throw std::runtime_error("check "+std::to_string(checks));}
std::string bad;bool ordered=true,deployed=false,complete=true,host=false;int calls=0,legacy_reads=0;std::vector<std::string> research_keys;
struct Request {std::string operation="client_bridges_commit",token="one",origin_uuid="origin";uint64_t epoch=1,generation=1;json expected_development={{"same",1}};};
struct Progress {bool locked=false,shown=false,committed=false;std::string cause;};
struct Pin {bool ordered=true;uintptr_t image=1,lounge=2,project=3,window=4,button=5,world=6;int items=7;std::string tech_key="bridges";json development={{"same",1}},tutorial={{"same",1}};};
std::map<std::string,Progress> worlds;std::map<std::string,json> receipts;
bool fail(std::string&e,const std::string&c){e=c;return false;}
struct Config {bool is_host=false;static Config& instance(){static Config c;c.is_host=host;return c;}};
namespace action_session {
enum class Role{Client,Host};
struct Context{Role role=Role::Client;std::string snapshot_uuid="session";bool operator==(const Context&)const=default;};
struct Binding{int generation=1;bool operator==(const Binding&)const=default;};
struct Session{bool current(Context&c,std::string&e){c.role=host?Role::Host:Role::Client;return bad!="session"||fail(e,"session lost");}};
Session& selected_session(){static Session s;return s;}
}
namespace typed_action_runtime {bool network_binding(const auto&,action_session::Binding&b,std::string&e){if(calls&&bad=="rebind")b.generation=2;return bad!="binding"||fail(e,"binding lost");}}
bool pin(const Request&,Pin&p,std::string&e,bool=false,const std::string& ={},const action_session::Context* =nullptr,const action_session::Binding* =nullptr){p.ordered=ordered;p.project=deployed?0:3;if(calls&&bad=="mutation")p.items=8;return bad!="pin"||fail(e,"pin rejected");}
bool before(const Pin&,std::string&e){return bad!="before"||fail(e,"wrong project");}
bool same_expected_development(const json&a,const json&b){return a==b;}
namespace live_host_state {struct Pin{int cash=10;bool operator==(const Pin&)const=default;};struct NativePort{NativePort(const auto&,const auto&,void*){}bool read(Pin&p,std::string&e){if(calls&&bad=="ledger")p.cash=9;return bad!="ledger-read"||fail(e,"ledger unavailable");}};}
namespace native {struct SimulationState{int cash=10;};bool read_simulation_state(SimulationState&s,std::string&e){if(calls&&bad=="full-ledger")s.cash=9;return bad!="simulation"||fail(e,"simulation read unavailable");}}
json begin_ledger(const native::SimulationState&s){return {{"cash",s.cash}};}
namespace tutorial_replication {bool read_progress(json&p,std::string&e){++legacy_reads;p={{"legacy",true}};return bad!="legacy-progress"||fail(e,"legacy joined owner differs");}}
namespace general_project {enum class Command{Commit};struct Intent{int source=1;};}
namespace tutorial_development {
struct Row{bool purchased=true,custom=false,project_present=true,deployed=false;};
struct StartingpointSnapshot{uintptr_t origin=6,lounge=2,project=3;std::optional<Row> row=Row{};};
bool read_research(const std::string& key,StartingpointSnapshot&s,json&d,std::string&e,bool){research_keys.push_back(key);s.project=deployed?0:3;s.row->project_present=!deployed;s.row->deployed=deployed;d={{"project",s.project}};if(bad=="owner")s.lounge=99;if(bad=="foreign")s.row->project_present=false;return bad!="research"||fail(e,"research unavailable");}
}
bool begin_snapshot(const Pin&,tutorial_development::StartingpointSnapshot&,json&,std::string&e){return bad!="snapshot"||fail(e,"snapshot unavailable");}
bool expected_project_intent(const auto&,const auto&,general_project::Command,general_project::Intent&,std::string&e){return bad!="source"||fail(e,"source rejected");}
namespace general_project_runtime {json status(){return {{"captures",0}};}bool observe_pending_ui(const auto&,const auto&,const auto&,uint64_t,std::string&id,std::string&e){id="ordered-id";return bad!="capture"||fail(e,"capture absent");}}
bool client_submission(const json&,const json&,std::string&id,std::string&e){id="legacy-id";return bad!="legacy-submit"||fail(e,"legacy submit absent");}
bool development_window(Pin&,std::string&e){return bad!="window"||fail(e,"window differs");}
bool commit_button(Pin&,std::string&e){return bad!="button"||fail(e,"button disabled");}
DWORD complete_state(uintptr_t,uintptr_t,bool&out){out=complete;return bad=="getter"?77:0;}
bool same(uintptr_t a,uintptr_t b){return a==b;}
bool same_inventory_owner(const Pin&,const Pin&){return bad!="inventory-owner";}
bool same_research_native(const Pin&,const Pin&,const auto&,const auto&){return bad!="unstable";}
template<class T>bool at(uintptr_t,size_t,T&v,std::string&){v=1;return true;}
DWORD show(uintptr_t,uintptr_t){++calls;return bad=="native"?88:0;}
DWORD activate(uintptr_t,uintptr_t){++calls;return bad=="native"?88:0;}
#include "client_ui_body.inc"
#include "client_commit_state.inc"
struct Clock {float time_scale=0,pause_scale=1;};
bool clock_admitted(const Request& request,const Pin&p,const Clock&clock,std::string&e){
#include "clock_guard.inc"
return true;
}
void reset(){bad.clear();ordered=true;deployed=false;complete=true;host=false;calls=legacy_reads=0;worlds.clear();receipts.clear();research_keys.clear();}
int main(){try{
 for(const auto* failure:{"","pin","before","session","binding","ledger-read","simulation","snapshot","source","getter","button","inventory-owner","native","capture","mutation","ledger","full-ledger"}){
    reset();bad=failure;Request r;json out;Progress*p=nullptr;std::string key,e;bool ok=client_ui(r,out,p,key,e);check(ok==bad.empty());check(legacy_reads==0);
    if(ok){check(calls==1&&out["queued"]==true&&out["route"]=="general_project"&&out["confirmation"]=="intent_queued"&&out["ledger_before"]==out["ledger_after"]);check(!client_ui(r,out,p,key,e)&&calls==1);}
    else {check(!e.empty()&&calls<=1);if(calls){check(p&&p->locked);std::string next;check(!client_ui(r,out,p,key,next)&&calls==1);}}
 }
 reset();complete=false;{Request r;json out;Progress*p=nullptr;std::string k,e;check(!client_ui(r,out,p,k,e)&&calls==0&&e.find("not natively complete")!=std::string::npos);}
 reset();ordered=false;{Request r;json out;Progress*p=nullptr;std::string k,e;check(client_ui(r,out,p,k,e)&&legacy_reads==2&&out["submitted"]==true&&out["queued"]==false&&out["action_uuid"]=="legacy-id");}
 reset();ordered=false;bad="legacy-progress";{Request r;json out;Progress*p=nullptr;std::string k,e;check(!client_ui(r,out,p,k,e)&&calls==0&&e=="legacy joined owner differs");}
 for(bool is_host:{false,true})for(bool is_deployed:{false,true}){reset();host=is_host;deployed=is_deployed;Request r;r.operation="client_bridges_commit_state";json out;std::string e;check(client_commit_state(r,out,e));check(out["read_only"]==true&&out["attempted"]==false&&out["deployed"]==is_deployed&&calls==0&&legacy_reads==0);}
 for(const auto* failure:{"pin","session","binding","research","owner","foreign","getter","unstable"}){reset();bad=failure;Request r;r.operation="client_bridges_commit_state";json out;std::string e;check(!client_commit_state(r,out,e)&&!e.empty()&&calls==0);}
 reset();ordered=false;{Request r;json out;std::string e;check(!client_commit_state(r,out,e)&&calls==0);}
 reset();{Request r;Pin p;Clock c{1,1};std::string e;check(!clock_admitted(r,p,c,e));r.operation="client_bridges_commit_state";check(clock_admitted(r,p,c,e));p.ordered=false;check(!clock_admitted(r,p,c,e));p.ordered=true;c.time_scale=NAN;check(!clock_admitted(r,p,c,e));c.time_scale=0;check(clock_admitted(r,p,c,e));}
 for(bool is_host:{false,true})for(bool is_deployed:{false,true})for(bool done:{false,true}){
    reset();host=is_host;deployed=is_deployed;complete=done;Request r;r.operation="client_startingpoint_commit_state";json out;std::string e;
    check(client_commit_state(r,out,e));check(research_keys==std::vector<std::string>{"startingpoint","startingpoint"});
    check(out["read_only"]==true&&out["attempted"]==false&&out["implementation_complete"]==(done&&!deployed)&&out["deployed"]==deployed&&calls==0);
 }
 for(const auto* failure:{"pin","session","binding","research","owner","foreign","getter","unstable"}){
    reset();bad=failure;Request r;r.operation="client_startingpoint_commit_state";json out;std::string e;
    check(!client_commit_state(r,out,e)&&!e.empty()&&calls==0);
 }
 reset();{Request r;Pin p;Clock c{9,1};std::string e;r.operation="client_startingpoint_commit_state";
    check(clock_admitted(r,p,c,e));r.operation="client_commit";check(!clock_admitted(r,p,c,e));
    r.operation="complete";json out;check(!client_commit_state(r,out,e)&&research_keys.empty()&&calls==0);}
 std::cout<<checks<<" completion fixture checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

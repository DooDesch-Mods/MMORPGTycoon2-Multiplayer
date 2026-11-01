#include <atomic>
#include <map>
#include <set>
#include <optional>
#include <limits>
#include <iostream>
#include <nlohmann/json.hpp>
#include "scenery_gesture.h"
using json=nlohmann::json;
using namespace mt2mp::game;
#define LOG_ERROR(...) ((void)0)
using uintptr_t=std::uintptr_t;
unsigned GetCurrentThreadId(){return 1;}
struct Config {bool local_test=true;static Config& instance(){static Config c;return c;}};
namespace action {struct Context{};struct Binding {uint64_t native_epoch=2,native_generation=3;bool operator==(const Binding&)const=default;};enum class SceneryStrokeKind{Paint,Erase};struct SceneryStrokeIntent{SceneryStrokeKind kind;int region=7;std::vector<int> stamps;};}
namespace host {struct WorldState{int origin=1;uintptr_t root=50,selection=51;};bool same_world(int a,int b){return a==b;}}
struct Runtime{unsigned thread=1;int memory=0;uintptr_t image=1000;bool ready=true;};
Runtime runtime;std::atomic<Runtime*> published{&runtime};
action::Binding fixture_binding;host::WorldState fixture_world;
std::string fixture_snapshot="10000000-0000-0000-0000-000000000001",fixture_action="20000000-0000-0000-0000-000000000001";
bool connected=true,binding_ok=true,owner_ok=true,world_ok=true,simulation_ok=true,routes_ok=true,body_ok=true,source_ok=true,region_ok=true;
int fixture_calls=0,fail_call=0,reject_call=0,change_world_call=0;bool omit_queue=false;uintptr_t point_region=70,fixture_default=30;float timescale=0;
json fixture_controller={{"started",true},{"locked",false},{"pending",false}};
namespace q {bool fail(std::string&e,const std::string&s){e=s;return false;}bool at(int,uintptr_t base,uintptr_t off,uintptr_t& out,std::string&){out=base==1000?10:off==0x190?20:off==0x198?fixture_default:0;return true;}bool typed(int,uintptr_t,uintptr_t,uintptr_t,std::string&e){return owner_ok||fail(e,"owner failed");}}
namespace net {namespace session_wire {bool connected(){return ::connected;}}std::string current_snapshot_id(){return fixture_snapshot;}}
namespace typed_action_runtime {bool network_binding(const std::string&,action::Binding& out,std::string&e){out=fixture_binding;return binding_ok||q::fail(e,"binding failed");}json controller_status(){return fixture_controller;}}
namespace native {struct SimulationState{struct{float time_scale;}clock;};bool read_simulation_state(SimulationState& out,std::string&e){out.clock.time_scale=timescale;return simulation_ok||q::fail(e,"simulation failed");}}
namespace mt2mp::game::scenery_gesture {inline constexpr int guards[]={1,2};}
namespace scenery_stroke {namespace {
using Kind=action::SceneryStrokeKind;
struct Capture{action::Binding binding;action::SceneryStrokeIntent intent;};
struct Pending{action::Binding binding;action::SceneryStrokeIntent intent;std::string snapshot,id;};
std::optional<Capture> capture;std::optional<Pending> queued;uint64_t captures=0,rejections=0;std::string cause;bool installed=true;
bool source_world(Runtime&,uintptr_t,Kind,host::WorldState& out,std::string&e){out=fixture_world;return world_ok||q::fail(e,"world failed");}
bool region(Runtime&,const host::WorldState&,int32_t id,uintptr_t expected,uintptr_t& out,std::string&e){out=70;return (region_ok&&id==7&&(!expected||expected==70))||q::fail(e,"region failed");}
bool read_source(Runtime&,uintptr_t,Kind,Capture& out,std::string&e){out.intent.region=7;return source_ok||q::fail(e,"source failed");}
bool verify_routes(Runtime&,bool,std::string&e){return routes_ok||q::fail(e,"route failed");}
bool original_bytes(Runtime&,int,std::string&e){return body_ok||q::fail(e,"body failed");}
}}
namespace scenery_stroke {
bool capture_binding(const std::string& snapshot,action::Binding& binding,std::optional<action::Context>& context,std::string& e){
    context.reset();return typed_action_runtime::network_binding(snapshot,binding,e);
}
bool current_capture(const std::optional<action::Context>& context,const std::string& snapshot,const action::Binding& expected,std::string& e){
    if(context)return q::fail(e,"unexpected ordered capture in legacy gesture fixture");
    action::Binding actual;return typed_action_runtime::network_binding(snapshot,actual,e)&&(actual==expected||q::fail(e,"Scenery gesture World binding changed"));
}
bool input_admission(std::string& e){return (fixture_controller.value("started",false)&&!fixture_controller.value("locked",true)&&!fixture_controller.value("pending",true))||q::fail(e,"Scenery gesture Controller unavailable");}
}
#include "adapter_actual.inc"
int assertions=0;void check(bool b,const char*s){++assertions;if(!b)throw std::runtime_error(s);}
json args(){return {{"token","30000000-0000-0000-0000-000000000001"},{"snapshot_uuid",fixture_snapshot},{"expected_world_epoch",uint64_t(2)},{"expected_generation",uint64_t(3)},{"kind","paint"},{"region_id",7},{"points",{{1,2,3},{4,2,3},{7,2,3}}}};}
void reset(){using namespace scenery_stroke;gesture_jobs.clear();capture.reset();queued.reset();captures=rejections=0;cause.clear();fixture_calls=fail_call=reject_call=change_world_call=0;omit_queue=false;fixture_world={};fixture_binding={};point_region=70;fixture_default=30;timescale=0;runtime={};published=&runtime;Config::instance().local_test=true;connected=binding_ok=owner_ok=world_ok=simulation_ok=routes_ok=body_ok=source_ok=region_ok=installed=true;fixture_controller={{"started",true},{"locked",false},{"pending",false}};}
int main(){using namespace scenery_stroke;try{
  json out;std::string e;reset();auto a=args();check(execute_native_gesture(a,out,e),"normal capture");check(fixture_calls==5&&out["stamps"]==3&&out["captured"]==true&&!out["host_ordered_action_confirmed"].get<bool>(),"capture is not confirmation");check(read_native_gesture({{"token",a["token"]}},out,e)&&out["action_uuid"]==fixture_action,"read retained UUID");queued.reset();check(!execute_native_gesture(a,out,e)&&fixture_calls==5,"same token not replayed");
  for(bool* flag:{&binding_ok,&owner_ok,&world_ok,&simulation_ok,&routes_ok,&body_ok,&region_ok,&connected}){reset();*flag=false;check(!execute_native_gesture(args(),out,e)&&fixture_calls==0&&!e.empty(),"fresh guard rejects before native input");}
  reset();Config::instance().local_test=false;check(!execute_native_gesture(args(),out,e)&&!fixture_calls,"non-owned rejected");reset();timescale=1;check(!execute_native_gesture(args(),out,e)&&!fixture_calls,"running simulation rejected");reset();point_region=71;check(!execute_native_gesture(args(),out,e)&&!fixture_calls,"point owner rejected");
  reset();a=args();a["expected_generation"]=uint64_t(4);check(!execute_native_gesture(a,out,e)&&!fixture_calls,"binding mismatch rejected");
  for(int i=1;i<=5;++i){reset();fail_call=i;check(!execute_native_gesture(args(),out,e)&&fixture_calls==i,"SEH stops later callbacks");check(read_native_gesture({{"token",args()["token"]}},out,e)&&out["attempted"]==i&&out["returned"]==i-1,"SEH attempted retained");capture.reset();queued.reset();a=args();a["token"]="30000000-0000-0000-0000-000000000002";check(!execute_native_gesture(a,out,e)&&fixture_calls==i,"uncertain receipt prevents new token replay");}
  for(int i=1;i<=5;++i){reset();change_world_call=i;check(!execute_native_gesture(args(),out,e)&&fixture_calls==i,"World changes stop remaining callbacks");reset();reject_call=i;check(!execute_native_gesture(args(),out,e)&&fixture_calls==i&&e.find("original capture rejected")!=std::string::npos,"capture error preserved");}
  reset();omit_queue=true;check(!execute_native_gesture(args(),out,e)&&fixture_calls==5&&out["captured"]==false,"no queue not success");reset();source_ok=false;check(!execute_native_gesture(args(),out,e)&&fixture_calls==1,"hover source validated before press");
  reset();a=args();a["kind"]="erase";check(execute_native_gesture(a,out,e)&&queued->intent.kind==Kind::Erase,"erase uses same guarded lifecycle");
  reset();GestureJob pinned;pinned.snapshot=fixture_snapshot;pinned.binding=fixture_binding;pinned.world=fixture_world;pinned.cursor=10;pinned.receiver=20;pinned.default_receiver=30;pinned.region_id=7;pinned.region_owner=70;
  check(gesture_current(runtime,pinned,e),"selected eraser and independent Default owner accepted");
  fixture_default=31;check(!gesture_current(runtime,pinned,e)&&e=="Scenery gesture Default behaviour owner changed","Default replacement rejected distinctly");fixture_default=30;
  pinned.receiver=21;check(!gesture_current(runtime,pinned,e)&&e=="Scenery gesture selected behaviour changed","selected replacement rejected distinctly");pinned.receiver=20;
  fixture_world.selection++;check(!gesture_current(runtime,pinned,e)&&e=="Scenery gesture native selection owner changed","selection replacement rejected distinctly");fixture_world=pinned.world;
  fixture_world.root++;check(!gesture_current(runtime,pinned,e)&&e=="Scenery gesture native World root changed","World root rejected distinctly");fixture_world=pinned.world;
  fixture_world.origin++;check(!gesture_current(runtime,pinned,e)&&e=="Scenery gesture native World origin changed","World origin rejected distinctly");fixture_world=pinned.world;
  fixture_binding.native_generation++;check(!gesture_current(runtime,pinned,e)&&e=="Scenery gesture World binding changed","binding rejected distinctly");fixture_binding=pinned.binding;
  pinned.snapshot="foreign";check(!gesture_current(runtime,pinned,e)&&e=="Scenery gesture shared Snapshot changed","Snapshot rejected distinctly");
  reset();a=args();a["points"][0][0]=std::numeric_limits<double>::infinity();check(!execute_native_gesture(a,out,e)&&!fixture_calls,"bad coordinate no calls");
  std::cout<<assertions<<" native adapter assertions passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}

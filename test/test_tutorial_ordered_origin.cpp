#include <vendor/nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace fixture {
using json=nlohmann::json;
namespace action_session {
enum class Role {Host,Client};enum class Action {HireDeveloper,FireDeveloper,Other};
struct Context {Role role=Role::Client;std::string snapshot_uuid="snapshot";int session=2;bool operator==(const Context&)const=default;};
struct Binding {uint64_t native_epoch=1,native_generation=1;bool operator==(const Binding&)const=default;};
struct Request {Action action=Action::Other;};
Context actual;
struct Session {bool current(Context& out,std::string&){out=actual;return true;}};
Session& selected_session(){static Session s;return s;}
namespace tick_commands::authority {bool active(){return false;}bool matches(const Context&,const Binding&,const Request&,std::string&){return false;}}
}
struct World {void* game=reinterpret_cast<void*>(1);void* mode=reinterpret_cast<void*>(2);void* state=reinterpret_cast<void*>(3);void* world=reinterpret_cast<void*>(4);void* map=reinterpret_cast<void*>(5);std::string uuid="snapshot",sha256="hash";uint64_t bytes=100,world_epoch=1,generation=1;bool operator==(const World&)const=default;};
World fixture_world;
bool mode=true,bound=true,observer_valid=true,owners_valid=true,research_valid=true,drift=false;
unsigned thread=7,mainthread=7,world_reads=0,observer_reads=0,research_reads=0,owner_reads=0,checks=0;
uintptr_t image=0;
unsigned GetCurrentThreadId(){return thread;}
struct Config {bool is_host=false;static Config& instance(){static Config c;return c;}};
struct Load {bool armed=false,ready=false;std::string uuid;uint64_t generation=0;std::set<uintptr_t> sequences,quarantined;}load;
std::string fault;
bool fail(std::string& error,const std::string& why){error=why;return false;}
namespace native_open_runtime {bool read_current_world(World& w,std::string&){w=fixture_world;if(drift&&world_reads++)w.map=reinterpret_cast<void*>(6);return true;}}
namespace typed_action_runtime {
bool ordered_mode_selected(){return mode;}
bool read_binding(const std::string& snapshot,action_session::Binding& b,std::string& e){
    if(!bound||thread!=7||snapshot!=action_session::actual.snapshot_uuid)return fail(e,"fixture fixture_world Controller binding unavailable");b={};return true;}
}
#include "tutorial_origin_actual.inc"
bool same_world(const World& a,const World& b){return a==b;}
namespace live_host_state::apply_timing {bool page_read_scope=false;struct PageReadScope{explicit PageReadScope(bool){}};}
namespace mt2mp::game {namespace live_host_state=::fixture::live_host_state;}
struct Cursor {std::string scenario,last;int32_t count=17,next=16;bool complete=false;std::vector<std::string> history;std::array<int,2> message_progress{};};
struct NativeCursor {uintptr_t instance=0,sequence=0,steps=0;Cursor value;};
struct Row {bool deployed=false;std::optional<int> commit_bugs;};
struct State {bool initial_complete=false,leveltwo_complete=false;std::vector<Cursor> active;std::optional<Row> startingpoint;std::optional<int> staff_display;int research=0;};
struct CommitReceipt {World origin;uintptr_t tech=0,record=0;std::optional<int> bugs;}commit_receipt;
namespace tutorial_observer {bool read(json& out,std::string& e){++observer_reads;if(!observer_valid)return fail(e,"fixture native observer ownership failed");out={{"initial_tutorial_complete",true},{"leveltwo_tutorial_complete",false}};return true;}}
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){
    uint64_t value=0;
    if(p==3&&offset==0x1e8)value=10;
    else if(p==10&&offset==0x20)value=20;
    else if(p==10&&offset==0x28)value=1;
    else if(p==20&&offset==0)value=30;
    else if(p==30&&offset==0x58)value=40;
    else if(p==40&&offset==0x20)value=50;
    else if(p==40&&offset==0x28)value=17;
    else if(p==40&&offset==0x50)value=16;
    else if(p==40&&offset==0x54)value=0;
    else if(p==30&&offset==0x48)value=60;
    else if(p==30&&offset==0x50)value=1;
    else return fail(e,"fixture actual cursor field absent");out=T(value);return true;
}
bool str(uintptr_t p,size_t offset,std::string& out,std::string& e){
    if(p==30&&offset==0x18)out="leveltwo_tutorial";
    else if(p==30&&offset==0x60)out="fixture_world-message";
    else if(p==60&&offset==0)out="retained-history";
    else return fail(e,"fixture actual cursor string absent");return true;
}
bool supported(const std::string& id){return id=="initial_tutorial"||id=="leveltwo_tutorial";}
struct Memory{}memory;
namespace message_progress {struct Binding{std::array<int,2> values{3,4};};bool read(Memory&,uintptr_t,uintptr_t,int32_t,Binding&,std::string&){return true;}}
namespace tutorial_development {struct StartingpointSnapshot {World origin;std::optional<Row> row;uintptr_t tech=0,record=0;};
bool read_startingpoint(StartingpointSnapshot&,json&,std::string&,bool,bool){return false;}}
namespace display_native {bool capture(const action_session::Context&,const action_session::Binding&,const tutorial_development::StartingpointSnapshot&,std::optional<int>&,std::string&,bool,bool){return false;}}
namespace lifecycle_native {bool decorate(const World&,const action_session::Context&,State&,std::string&){return true;}}
bool research_capture(const action_session::Context&,const action_session::Binding&,const World&,State& out,std::string& e,bool ordered){
    ++research_reads;out.research=99;return research_valid&&ordered||fail(e,"fixture native research proof failed");}
bool valid(const State& s,std::string& e){return s.active.size()==1&&s.active[0].history==std::vector<std::string>{"retained-history"}&&s.research==99||fail(e,"fixture complete tutorial proof missing");}
#include "tutorial_state_actual.inc"
namespace scenario_intro {struct State{bool completed=false;};}
namespace intro_native {using Value=scenario_intro::State;struct Owners{};
bool owners(const World&,Owners&,Value& out,std::string& e){++owner_reads;out.completed=true;return owners_valid||fail(e,"fixture Intro native ownership failed");}
#include "tutorial_intro_sample_actual.inc"
}
#include "tutorial_capture_intro_actual.inc"
#include "tutorial_capture_actual.inc"
void need(bool condition,const std::string& why){++checks;if(!condition)throw std::runtime_error(why);}
void reset(){mode=bound=observer_valid=owners_valid=research_valid=true;drift=false;thread=mainthread=7;world_reads=observer_reads=research_reads=owner_reads=0;
    load={};fault.clear();fixture_world={};action_session::actual={};Config::instance().is_host=false;}
void run(){std::string error;action_session::Binding binding;
    for(bool host:{false,true}){reset();action_session::actual.role=host?action_session::Role::Host:action_session::Role::Client;Config::instance().is_host=host;
        scenario_intro::State intro;need(capture_intro(action_session::actual,binding,intro,error)&&intro.completed&&owner_reads==1,error);
        State state;need(capture(action_session::actual,binding,state,error)&&observer_reads==1&&research_reads==1,error);
        need(state.active.size()==1&&state.active[0].scenario=="leveltwo_tutorial"&&state.active[0].message_progress==std::array<int,2>{3,4},"Ordered read lost native active cursor/history/message proof");
        need(!load.armed&&!load.ready&&load.sequences.empty(),"Read fabricated legacy joined-load fences");}
    for(int defect=0;defect<9;++defect){reset();auto context=action_session::actual;auto b=binding;
        if(defect==0)bound=false;if(defect==1)load.armed=true;if(defect==2)Config::instance().is_host=true;
        if(defect==3)++context.session;if(defect==4)++b.native_generation;if(defect==5)++fixture_world.world_epoch;
        if(defect==6)thread=8;if(defect==7)fault="retained native fault";if(defect==8)mode=false;
        State out;need(!capture(context,b,out,error),"Invalid ordered origin admitted defect="+std::to_string(defect));}
    for(int defect=0;defect<3;++defect){reset();if(defect==0)observer_valid=false;if(defect==1)research_valid=false;if(defect==2)drift=true;
        State out;need(!capture(action_session::actual,binding,out,error),"Failed full native proof replaced by success");}
    reset();owners_valid=false;scenario_intro::State intro;need(!capture_intro(action_session::actual,binding,intro,error),"Failed Intro owner proof replaced by success");
    reset();World w;need(!origin(action_session::actual,binding,w,error),"Default mutation/adoption origin accepted unfenced ordered Client");
    mode=false;load={true,true,"snapshot",1,{40},{}};need(origin(action_session::actual,binding,w,error),error);
    load.generation=2;need(!origin(action_session::actual,binding,w,error),"Stale legacy joined-load generation admitted");
    reset();mode=false;load={true,true,"snapshot",1,{}, {}};State state;std::vector<NativeCursor> cursors;
    need(!native_state(action_session::actual,binding,state,cursors,error)&&error.find("not fenced")!=std::string::npos,"Legacy Client unfenced sequence check weakened");
}
}
int main()try{fixture::run();std::cout<<fixture::checks<<" actual tutorial origin/capture checks passed\n";return 0;}
catch(const std::exception& why){std::cerr<<why.what()<<'\n';return 1;}

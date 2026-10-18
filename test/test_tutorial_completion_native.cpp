#include "tutorial_replication/lifecycle.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
using namespace mt2mp::game::tutorial_replication;
namespace action_session {enum class Role{Host,Client};struct Context {Role role=Role::Client;int id=1;bool operator==(const Context&)const=default;};struct Binding {};}
namespace {
using DWORD=uint32_t;
struct World {int id=1;std::string uuid="world";uint64_t generation=1;};
struct NativeCursor {uintptr_t instance=0,sequence=0,steps=0;Cursor value;};
uintptr_t image=0x140000000,mainthread=7,scoped_sequence=0,return_address=0;
uint32_t thread_id=7;
std::string fault;
struct Load {bool armed=true;std::set<uintptr_t> sequences,quarantined;std::string uuid="world";uint64_t generation=1;bool ready=false;} load;
std::map<uintptr_t,unsigned char> memory_bytes;
std::map<uintptr_t,std::string> memory_strings;
bool origin_ok=true,type_ok=true,append_ok=true,remove_ok=true,version_ok=true,owner_change=false;
bool fixture_present=false,first_fence_seen=false;
unsigned advances=0,starts=0,completes=0,version_sets=0;
unsigned updates=0,terminals=0,scenario_starts=0;
DWORD start_code=0,advance_code=0,complete_code=0,version_code=0;
int32_t limit=17;
constexpr uintptr_t fixture_sequence=0x10000,instance=0x20000,fixture_steps=0x30000,first_step=0x40000;
std::array<int32_t,3> current_version{};
NativeCursor native_cursor;
std::vector<uintptr_t> instances;
size_t completed_ids=0;
unsigned checks=0;
State physical_state,desired_state;
unsigned state_reads=0,fail_read=0,research_calls=0;
bool code_ok=true,research_ok=true,prepare_ok=true,display_ok=true,research_effect_ok=true,display_fail_install=false;
State preflight_observed;
bool fail(std::string& e,const std::string& why){e=why;return false;}
void block(const std::string& e){if(fault.empty())fault=e;}
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
void pass(const char* label){++checks;std::cout<<"PASS "<<label<<'\n';}
bool same_world(const World& a,const World& b){return a.id==b.id;}
uint32_t GetCurrentThreadId(){return thread_id;}
void* fixture_return_address(){return reinterpret_cast<void*>(return_address);}
#define _ReturnAddress fixture_return_address
template<class T>void seed(uintptr_t address,const T& value){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)memory_bytes[address+i]=bytes[i];}
template<class T>bool at(uintptr_t address,size_t offset,T& value,std::string& e){auto* bytes=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i){auto it=memory_bytes.find(address+offset+i);if(it==memory_bytes.end())return fail(e,"original fixture read failed");bytes[i]=it->second;}return true;}
bool str(uintptr_t address,size_t offset,std::string& value,std::string& e){const auto it=memory_strings.find(address+offset);if(it==memory_strings.end())return fail(e,"original fixture string read failed");value=it->second;return true;}
bool type(uintptr_t,uintptr_t,std::string& e){return type_ok||fail(e,"original native type read failed");}
bool origin(const action_session::Context&,const action_session::Binding&,World& world,std::string& e){if(!fault.empty())return fail(e,fault);if(!origin_ok)return fail(e,"original NativeOpen owner changed");world={};return true;}
struct Scope {uintptr_t previous;Scope(uintptr_t p,bool,uintptr_t=0,uintptr_t=0):previous(scoped_sequence){scoped_sequence=p;}~Scope(){scoped_sequence=previous;}};
void original_advance(void*){
    ++advances;first_fence_seen=load.sequences.contains(fixture_sequence)&&scoped_sequence==fixture_sequence;
    int32_t next=0;uint8_t done=0;std::string e;need(at(fixture_sequence,0x50,next,e),e);
    if(next<limit)seed(fixture_sequence+0x50,next+1);else{done=1;seed(fixture_sequence+0x54,done);}
}
void original_update(void*,float){++updates;}
bool supported(const std::string& id){return id=="initial_tutorial"||id=="leveltwo_tutorial";}
namespace native_open_runtime {bool read_current_world(World& w,std::string& e){if(!origin_ok)return fail(e,"original NativeOpen owner changed");w={};return true;}}
template<class... Args>void fixture_log(const char*,const Args&... ){}
#define LOG_INFO fixture_log
#define LOG_WARN fixture_log
namespace lifecycle_native {
#include "lifecycle_cache.inc"
struct Owners {uintptr_t log=0,library=0,asset=0;std::vector<uintptr_t> instances;size_t completed=0;};
bool owners(const World&,const std::string&,Owners& out,std::string&){out={1,owner_change&&completes?99u:2u,3,instances,completed_ids};return true;}
bool bootstrap_owners(const std::string& id,Owners& out,std::string& e){return owners(World{},id,out,e);}
bool log_owners(const World& w,const std::string& id,Owners& out,std::string& e){return owners(w,id,out,e);}
bool owned_cursor(const Owners&,const std::string&,NativeCursor& out,bool& found,std::string& e){found=fixture_present;out=native_cursor;if(!found)return true;uint8_t done=0;return at(fixture_sequence,0x50,out.value.next,e)&&at(fixture_sequence,0x54,done,e)&&(out.value.complete=done!=0,true);}
bool array(uintptr_t address,size_t offset,int32_t,uintptr_t& data,int32_t& count,std::string& e){return at(address,offset,data,e)&&at(address,offset+8,count,e);}
void remember_terminal(uintptr_t){++terminals;}
#include "lifecycle_advance.inc"
bool host(World& w,action_session::Context&,action_session::Binding&,std::string& e){if(!origin_ok)return fail(e,"original Host owner changed");w={};return bind_cache(w,{},e);}
void original_scenario_start(void*,void*,void*){++scenario_starts;fixture_present=true;instances.push_back(instance);original_advance(reinterpret_cast<void*>(fixture_sequence));}
#include "lifecycle_start.inc"
}
#include "lifecycle_update.inc"
DWORD advance_call(uintptr_t p){if(advance_code)return advance_code;original_advance(reinterpret_cast<void*>(p));return 0;}
bool interval(const NativeCursor&,const Cursor&,const std::optional<startingpoint::Row>&,std::string&){return true;}
bool history(const NativeCursor&,const Cursor& target,std::string&){native_cursor.value.history=target.history;native_cursor.value.last=target.last;return true;}
struct ProgressCalls {};
struct Memory {} memory;
namespace message_progress {bool verify_code(Memory&,uintptr_t,std::string& e){return code_ok||fail(e,"original message code failed");}bool apply(Memory&,uintptr_t,uintptr_t,int32_t,const std::optional<MessageProgress>&,ProgressCalls&,bool&,std::string&){return true;}}
namespace lifecycle_native {
DWORD start_call(uintptr_t,uintptr_t){++starts;fixture_present=true;if(append_ok)instances.push_back(instance);advance(reinterpret_cast<void*>(fixture_sequence));return start_code;}
DWORD complete_call(uintptr_t,uintptr_t){++completes;if(remove_ok){fixture_present=false;completed_ids=1;instances.clear();physical_state=desired_state;}return complete_code;}
DWORD set_version_call(uintptr_t){++version_sets;if(version_ok)current_version={0,0,100};return version_code;}
bool version(const World&,uintptr_t& owner,std::array<int32_t,3>& out,std::string&){owner=77;out=current_version;return true;}
#include "lifecycle_pin.inc"
#include "lifecycle_execute.inc"
bool prepare(const action_session::Context&,const action_session::Binding&,const State& local,const State& target,lifecycle::Plan& plan,std::optional<startingpoint::Row>& technology,std::string& e){if(!prepare_ok)return fail(e,"original lifecycle prepare failed");technology=lifecycle::technology(local);return lifecycle::plan(local,target,plan,e);}
}
struct StartingpointCalls {
    action_session::Context context;action_session::Binding binding;
    struct Before {World origin;uintptr_t tech=0,record=0;} before;
    bool pin(const startingpoint::Row&,bool,std::string& e){return fail(e,"unexpected technology mutation");}
    bool create(std::string& e){return fail(e,"unexpected technology mutation");}
    bool project(const startingpoint::Row&,std::string& e){return fail(e,"unexpected technology mutation");}
    bool deploy(std::string& e){return fail(e,"unexpected technology mutation");}
    bool retire(std::string& e){return fail(e,"unexpected technology mutation");}
    bool readback(const startingpoint::Row&,std::string& e){return fail(e,"unexpected technology mutation");}
};
struct CommitReceipt {World origin;uintptr_t tech=0,record=0;std::optional<startingpoint::CommitBugs> bugs;std::string cause;} commit_receipt;
namespace live_host_state::apply_timing {struct PageReadScope {~PageReadScope(){};}; }
namespace tutorial_development {struct StartingpointSnapshot {}; }
namespace display_native {bool admit(const action_session::Context&,const action_session::Binding&,const std::optional<staff_display::State>&,std::string& e,bool install,const std::string& ="startingpoint",bool=false,const tutorial_development::StartingpointSnapshot* observed=nullptr){if(observed)need(install,"prepared Staff was used before effects");return (display_ok&&!(install&&display_fail_install))||fail(e,"original display admission failed");}}
bool native_state(const action_session::Context&,const action_session::Binding&,State& out,std::vector<NativeCursor>& cursors,std::string& e,bool=false){
    ++state_reads;if(!fault.empty())return fail(e,fault);if(state_reads==fail_read)return fail(e,"original native state failed");
    out=physical_state;cursors={native_cursor};return true;
}
bool research_preflight(const action_session::Context&,const action_session::Binding&,const State& local,const State& target,State& projected,std::string& e,StartingpointCalls* =nullptr){
    preflight_observed=local;projected=local;research::Plan operation;
    return (research_ok||fail(e,"original research preflight failed"))&&research::plan(local.research,target.research,operation,e);
}
bool research_apply(const action_session::Context&,const action_session::Binding&,const State& local,const State& target,bool& attempted,bool& unknown,std::string& e,StartingpointCalls* prepared=nullptr,tutorial_development::StartingpointSnapshot* observed=nullptr){
    need((prepared!=nullptr)==(observed!=nullptr),"partial Progress local reuse");
    ++research_calls;if(local.research==target.research)return true;
    attempted=unknown=true;physical_state.research=target.research;
    if(!research_effect_ok)return fail(e,"original research effect failed");unknown=false;return true;
}
#include "lifecycle_progress_only.inc"
#include "lifecycle_preflight.inc"
#include "lifecycle_outer_apply.inc"
void reset(const std::string& id="leveltwo_tutorial",int32_t next=0){
    memory_bytes.clear();memory_strings.clear();fault.clear();load={true,{},{}};scoped_sequence=0;return_address=image+0x4f61fe;thread_id=7;
    origin_ok=type_ok=append_ok=remove_ok=version_ok=true;owner_change=false;fixture_present=false;first_fence_seen=false;
    advances=starts=completes=version_sets=0;start_code=advance_code=complete_code=version_code=0;limit=id=="initial_tutorial"?107:17;
    updates=terminals=scenario_starts=0;
    physical_state={};desired_state={};preflight_observed={};state_reads=fail_read=research_calls=0;
    code_ok=research_ok=prepare_ok=display_ok=research_effect_ok=true;display_fail_install=false;
    current_version={0,0,0};instances.clear();completed_ids=0;lifecycle_native::cache.reset();lifecycle_native::creation=nullptr;
    native_cursor={instance,fixture_sequence,fixture_steps,{id,"",limit,next,false,{},{}}};
    seed(fixture_sequence+0x20,fixture_steps);seed(fixture_sequence+0x28,limit);seed(fixture_sequence+0x50,next);seed<uint8_t>(fixture_sequence+0x54,0);seed<uintptr_t>(fixture_sequence+0x48,0);
    seed(fixture_steps,first_step);seed(fixture_steps+93*8,first_step+93);seed(fixture_steps+100*8,first_step+100);
    memory_strings[first_step+0x40]=id=="initial_tutorial"?"MessageTo SoftwareStatus LockLevelUp":"MessageTo Hud Tutorial 1";
    memory_strings[3+0x18]=id;
}
State target(const std::string& id="leveltwo_tutorial",bool terminal=false){State s;Cursor c{id,"",id=="initial_tutorial"?107:17,terminal?(id=="initial_tutorial"?107:17):1,terminal,{},{}};if(terminal){s.terminal.push_back(c);s.leveltwo_complete=id=="leveltwo_tutorial";s.initial_complete=!s.leveltwo_complete;s.lifecycle_order[lifecycle::slot(id)]={1,2};}else{s.active.push_back(c);s.lifecycle_order[lifecycle::slot(id)][0]=1;}return s;}
void hooks(){using namespace lifecycle_native;
    {reset();Creation scope{{},{},{},1,3,0,"leveltwo_tutorial",false};creation=&scope;advance(reinterpret_cast<void*>(fixture_sequence));need(advances==1&&first_fence_seen&&scope.entered&&load.quarantined.contains(fixture_sequence),"constructor first_step Advance escaped early fence");creation=nullptr;pass("constructor is fenced before its first_step original Advance");}
    {reset();type_ok=false;Creation scope{{},{},{},1,3,0,"leveltwo_tutorial",false};creation=&scope;advance(reinterpret_cast<void*>(fixture_sequence));need(!advances&&load.quarantined.contains(fixture_sequence)&&!load.sequences.contains(fixture_sequence)&&fault=="original native type read failed","unconfirmed constructor type failure lost quarantine");creation=nullptr;pass("type failure retains quarantine without admitted fixture_sequence or effect");}
    {reset();memory_strings[first_step+0x40]="foreign";Creation scope{{},{},{},1,3,0,"leveltwo_tutorial",false};creation=&scope;advance(reinterpret_cast<void*>(fixture_sequence));need(!advances&&load.quarantined.contains(fixture_sequence)&&load.sequences.contains(fixture_sequence)&&!scope.entered&&!fault.empty(),"bad first_step Command was executed or lost fence");creation=nullptr;pass("wrong first_step Command remains fenced with visible original cause");}
    {reset();return_address++;Creation scope{{},{},{},1,3,0,"leveltwo_tutorial",false};creation=&scope;advance(reinterpret_cast<void*>(fixture_sequence));need(!advances&&load.quarantined.empty()&&!fault.empty(),"foreign constructor caller admitted");creation=nullptr;pass("foreign constructor caller cannot claim a receiver");}
    {reset();origin_ok=false;Creation scope{{},{},{},1,3,0,"leveltwo_tutorial",false};creation=&scope;advance(reinterpret_cast<void*>(fixture_sequence));need(!advances&&fault=="original NativeOpen owner changed","constructor owner failure cause lost");creation=nullptr;pass("constructor changed Origin fails before original");}
    {reset();Creation scope{{},{},{},1,3,0,"leveltwo_tutorial",false};creation=&scope;advance(reinterpret_cast<void*>(fixture_sequence));advance(reinterpret_cast<void*>(fixture_sequence));need(advances==1&&!fault.empty(),"same constructor replayed original Advance");creation=nullptr;pass("same constructor scope cannot repeat original Advance");}
    {reset();load.sequences.insert(fixture_sequence);advance(reinterpret_cast<void*>(fixture_sequence));need(!advances&&!fault.empty(),"joined Client autonomous Advance executed");pass("joined Client Advance requires exact Host-frame scope");}
    {reset();load.sequences.insert(fixture_sequence);Scope scope(fixture_sequence,false);advance(reinterpret_cast<void*>(fixture_sequence));need(advances==1&&fault.empty(),"legitimate Host-frame scoped Advance blocked");pass("exact Host-frame Advance calls original once");}
    {reset();load.sequences.insert(fixture_sequence);thread_id=8;Scope scope(fixture_sequence,false);advance(reinterpret_cast<void*>(fixture_sequence));need(!advances&&!fault.empty(),"foreign thread scoped Advance executed");pass("wrong thread cannot borrow a frame scope");}
    {reset();load.quarantined.insert(fixture_sequence);sequence_update(reinterpret_cast<void*>(fixture_sequence),1);need(!updates&&!terminals,"unconfirmed constructor updated autonomously");pass("unconfirmed constructor remains fenced at actual Sequence Update entry");}
    {reset();sequence_update(reinterpret_cast<void*>(fixture_sequence),1);need(updates==1&&terminals==1,"ordinary sequence Update did not call original once");pass("unfenced native Update calls original once then observes terminal receipt");}
    {reset("initial_tutorial");load.armed=false;scenario_start(reinterpret_cast<void*>(1),reinterpret_cast<void*>(3),nullptr);need(scenario_starts==1&&advances==1&&cache&&cache->proof.lifecycle_order[0][0]==1&&fault.empty(),"Host original Start was not observed once");pass("Host Start calls original once then stores confirmed native birth");}
    {reset();load.armed=false;origin_ok=false;scenario_start(reinterpret_cast<void*>(1),reinterpret_cast<void*>(3),nullptr);need(scenario_starts==1&&!cache,"Host pre-observer failure swallowed ordinary Start");pass("Host observer failure preserves ordinary original Start exactly once");}
    {reset();scenario_start(reinterpret_cast<void*>(1),reinterpret_cast<void*>(3),nullptr);need(!scenario_starts&&fault.empty(),"Client independently started supported scenario");pass("joined Client supported Start is deferred until Host frame");}
    {reset();memory_strings[3+0x18]="other";scenario_start(reinterpret_cast<void*>(1),reinterpret_cast<void*>(3),nullptr);need(scenario_starts==1&&fault.empty(),"foreign scenario original was swallowed");pass("foreign scenario Start preserves original once");}
}
void execute(){using namespace lifecycle_native;action_session::Context c;action_session::Binding b;World w;std::string e;bool attempted=false;
    {reset();auto t=target();need(create(c,b,w,1,t,attempted,e)&&attempted&&starts==1&&advances==1&&load.quarantined.empty()&&load.sequences.contains(fixture_sequence)&&cache->proof.lifecycle_order[1][0]==1&&!creation,"original Start readback not adopted: "+e);need(!create(c,b,w,1,t,attempted,e)&&starts==1,"existing scenario was started twice");pass("one original Start appends and confirms before releasing quarantine; duplicate rejects");}
    {reset();append_ok=false;auto t=target();need(!create(c,b,w,1,t,attempted,e)&&attempted&&starts==1&&load.quarantined.contains(fixture_sequence)&&!creation&&!cache,"partial append was falsely confirmed");pass("partial Start retains fence and cannot confirm cache metadata");}
    {reset();start_code=37;auto t=target();need(!create(c,b,w,1,t,attempted,e)&&attempted&&e.find("exception=37")!=std::string::npos&&load.quarantined.contains(fixture_sequence),"original Start exception cause lost");pass("Start exception after original attempt stays visible and fenced");}
    {reset("leveltwo_tutorial",16);fixture_present=true;instances={instance};load.sequences.insert(fixture_sequence);auto t=target("leveltwo_tutorial",true);need(complete(c,b,w,1,t,attempted,e)&&advances==2&&completes==1&&!load.sequences.contains(fixture_sequence)&&cache->proof.terminal==t.terminal,"terminal Stop/Complete readback failed: "+e);need(!complete(c,b,w,1,t,attempted,e)&&completes==1,"completed scenario called native Complete twice");pass("final Stop executes once then Complete once; only confirmed erase removes fence");}
    {reset("leveltwo_tutorial",17);fixture_present=true;instances={instance};seed<uint8_t>(fixture_sequence+0x54,1);load.sequences.insert(fixture_sequence);auto t=target("leveltwo_tutorial",true);need(complete(c,b,w,1,t,attempted,e)&&!advances&&completes==1,"already stopped cursor stopped twice");pass("already confirmed Stop is not repeated");}
    {reset("leveltwo_tutorial",17);fixture_present=true;instances={instance};seed<uint8_t>(fixture_sequence+0x54,1);load.sequences.insert(fixture_sequence);remove_ok=false;auto t=target("leveltwo_tutorial",true);need(!complete(c,b,w,1,t,attempted,e)&&completes==1&&load.sequences.contains(fixture_sequence)&&!cache&&e.find("removal/completed-ID")!=std::string::npos,"partial Complete falsely erased fence");pass("partial Complete retains early fence and does not claim metadata");}
    {reset("leveltwo_tutorial",17);fixture_present=true;instances={instance};seed<uint8_t>(fixture_sequence+0x54,1);load.sequences.insert(fixture_sequence);owner_change=true;auto t=target("leveltwo_tutorial",true);need(!complete(c,b,w,1,t,attempted,e)&&completes==1&&load.sequences.contains(fixture_sequence),"changed native owner falsely acknowledged");pass("post-Complete owner change remains unconfirmed and fenced");}
    {reset("leveltwo_tutorial",16);fixture_present=true;instances={instance};load.sequences.insert(fixture_sequence);advance_code=91;auto t=target("leveltwo_tutorial",true);need(!complete(c,b,w,1,t,attempted,e)&&!completes&&e.find("exception=91")!=std::string::npos&&load.sequences.contains(fixture_sequence),"failed Advance reached native Complete");pass("Advance failure preserves original cause before Complete");}
    {reset("initial_tutorial",100);fixture_present=true;instances={instance};load.sequences.insert(fixture_sequence);auto t=target("initial_tutorial");t.active[0].next=101;t.version=std::array<int32_t,3>{0,0,100};need(forward(c,b,w,0,101,t,false,attempted,e)&&version_sets==1&&advances==1&&current_version==*t.version,"exact Version Set failed: "+e);pass("actual forward runs exact Version Set before Step100 Advance");}
    {reset("initial_tutorial",100);fixture_present=true;instances={instance};load.sequences.insert(fixture_sequence);auto t=target("initial_tutorial");t.active[0].next=101;t.version=std::array<int32_t,3>{0,0,100};version_ok=false;need(!forward(c,b,w,0,101,t,false,attempted,e)&&version_sets==1&&!advances&&e.find("full owner/value")!=std::string::npos,"false Version Set readback advanced cursor");pass("wrong Version Set readback stops before cursor mutation");}
    {reset("initial_tutorial",100);fixture_present=true;instances={instance};load.sequences.insert(fixture_sequence);auto t=target("initial_tutorial");t.active[0].next=101;t.version=std::array<int32_t,3>{0,1,100};need(!forward(c,b,w,0,101,t,false,attempted,e)&&!version_sets&&!advances,"arbitrary Version Set executed");pass("invented version never reaches native Set or Advance");}
    {reset("leveltwo_tutorial",16);fixture_present=true;instances={instance};load.sequences.insert(fixture_sequence);physical_state=target();physical_state.active[0].next=16;desired_state=target("leveltwo_tutorial",true);bool unknown=false;
        need(apply(c,b,desired_state,attempted,unknown,e)&&attempted&&!unknown&&completes==1&&advances==2,"actual outer Apply failed confirmed terminal: "+e);
        need(apply(c,b,desired_state,attempted,unknown,e)&&!attempted&&!unknown&&completes==1&&advances==2,"duplicate confirmed outer Apply replayed native effect");pass("actual outer Apply confirms complete readback and duplicate has no native replay");}
    {reset("leveltwo_tutorial",16);fixture_present=true;instances={instance};load.sequences.insert(fixture_sequence);physical_state=target();physical_state.active[0].next=16;desired_state=target("leveltwo_tutorial",true);remove_ok=false;bool unknown=false;
        need(!apply(c,b,desired_state,attempted,unknown,e)&&attempted&&unknown&&completes==1&&!fault.empty()&&load.sequences.contains(fixture_sequence),"partial outer Apply lost Unknown/fence");const auto cause=e;
        need(!apply(c,b,desired_state,attempted,unknown,e)&&!attempted&&!unknown&&completes==1&&advances==2&&e==cause,"faulted outer Apply replayed original or lost original cause");pass("actual outer Apply latches partial Unknown and blocks replay with original cause");}
}
#include "tutorial_preflight_reuse_cases.inc"
#include "tutorial_progress_only_cases.inc"
}
int main(){try{hooks();execute();preflight_reuse_cases();progress_only_cases();std::cout<<checks<<" actual lifecycle native-body checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}

namespace intro_fixture {
namespace scenario_intro=mt2mp::game::scenario_intro;
namespace action_session=::action_session;
using Value=scenario_intro::State;
using DWORD=uint32_t;
struct World{int value=1;};
bool same_world(const World& a,const World& b){return a.value==b.value;}
struct Owners{struct{uintptr_t log=1,asset=2,library=3;std::vector<uintptr_t> instances{11,22};}log;uintptr_t instance=0;std::vector<std::string> completed{"initial_tutorial"};};
Value state;Owners owner;bool clock=true,owned=true,world_current=true,scoped_start=false,start_entered=false,bad_start=false;
std::string fault;constexpr const char* id="intro_cutscene";
size_t samples=0,starts=0,ends=0,stores=0,fail_store=0,fail_sample=0;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool startingpoint_clock(std::string& e){return clock||fail(e,"fixture Clock differs");}
bool verify_owned(std::string& e){return owned||fail(e,"fixture physical Intro hook owner differs");}
namespace lifecycle_native{bool verify_transition_code(std::string&){return true;}}
bool sample(const action_session::Context&,const action_session::Binding&,World& out,Owners& local,Value& value,std::string& e){++samples;if(!world_current||samples==fail_sample)return fail(e,"fixture original fresh sample failed");out={};local=owner;value=state;return true;}
DWORD invoke(bool complete,uintptr_t log,uintptr_t asset){need(log==owner.log.log&&asset==owner.log.asset,"Intro actual invocation receiver changed");
    if(complete){++ends;if(owner.instance)owner.log.instances.erase(std::find(owner.log.instances.begin(),owner.log.instances.end(),owner.instance));owner.instance=0;owner.completed.push_back(id);state={false,true,false,0,0};}
    else{++starts;need(scoped_start,"Intro original Start lacks typed scope");start_entered=true;owner.instance=33;owner.log.instances.push_back(33);state={true,false,false,0,0};if(bad_start)owner.log.instances.push_back(44);}return 0;}
DWORD cas(uintptr_t address,bool byte,int32_t prior,int32_t target,bool& stored){++stores;if(stores==fail_store){stored=false;return 17;}
    if(address==owner.instance+0x60){need(!byte&&state.phase==prior,"Intro phase prior differs");state.phase=target;}
    else if(address==owner.instance+0x78){need(!byte&&state.starting_points==prior,"Intro StartingPoint count prior differs");state.starting_points=target;}
    else{need(address==owner.instance+0x38&&byte&&int32_t(state.retiring)==prior,"Intro completion prior differs");state.retiring=target!=0;}stored=true;return 0;}
#include "intro_adopt_actual.inc"
struct{bool armed=false;}load;
size_t updates=0,events=0,blocks=0;bool member=true;
void native_update(void*,float){++updates;}void native_event(void*,const void*){++events;}
auto original_update=&native_update;auto original_event=&native_event;
bool fenced(uintptr_t,std::string& e){return member||fail(e,"fixture callback member differs");}
void block(const std::string& cause){need(!cause.empty(),"Intro hook lost original failure");++blocks;}
#include "intro_callback_actual.inc"
void reset(){state={};owner={};clock=owned=world_current=true;scoped_start=start_entered=bad_start=false;fault.clear();samples=starts=ends=stores=fail_store=fail_sample=0;load.armed=false;updates=events=blocks=0;member=true;}
void run(){action_session::Context c;c.role=action_session::Role::Client;action_session::Binding b;bool attempted=false;std::string e;Value target{true,false,false,2,4};
    reset();need(adopt(c,b,target,attempted,e)&&attempted&&starts==1&&stores==2&&state==target&&owner.log.instances==std::vector<uintptr_t>{11,22,33},"Intro actual original birth and complete scalar adoption: "+e);
    samples=stores=0;attempted=false;need(adopt(c,b,target,attempted,e)&&!attempted&&starts==1&&samples==1&&stores==0,"unchanged Intro repeated native work");
    Value completed{false,true,false,0,0};need(adopt(c,b,completed,attempted,e)&&ends==1&&state==completed&&owner.log.instances==std::vector<uintptr_t>{11,22}&&owner.completed==std::vector<std::string>{"initial_tutorial","intro_cutscene"},"Intro actual Complete changed other scenarios/order: "+e);
    attempted=false;need(!adopt(c,b,target,attempted,e)&&!attempted,"completed Intro restarted");
    reset();need(adopt(c,b,completed,attempted,e)&&starts==0&&ends==1,"coalesced Intro completion replayed cinematic Start");
    reset();attempted=false;bad_start=true;need(!adopt(c,b,target,attempted,e)&&attempted&&starts==1&&stores==0,"Intro unexpected original birth continued scalar writes");
    reset();attempted=false;fail_sample=3;need(!adopt(c,b,target,attempted,e)&&attempted&&stores==0&&samples==3,"fresh Intro guard failure after original birth continued");
    reset();attempted=false;fail_store=2;need(!adopt(c,b,target,attempted,e)&&attempted&&stores==2&&state.phase==2&&state.starting_points==0,"partial Intro scalar failure replayed or lost effect state");
    for(int guard=0;guard<3;++guard){reset();attempted=false;if(guard==0)clock=false;else if(guard==1)owned=false;else world_current=false;need(!adopt(c,b,target,attempted,e)&&!attempted&&starts==0&&stores==0,"Intro failed Clock/hook/World guard mutated");}
    reset();update(nullptr,1);event(nullptr,nullptr);need(updates==1&&events==1,"Host Intro native callbacks changed");load.armed=true;update(nullptr,1);event(nullptr,nullptr);need(updates==1&&events==1&&!blocks,"Client Intro ran independent region/Clock/Event decisions");member=false;update(nullptr,1);event(nullptr,nullptr);need(blocks==2&&updates==1&&events==1,"foreign Intro callback forwarded or lost failure");
    Value decoded;auto wire=scenario_intro::encode(target);need(scenario_intro::decode(wire,decoded,e)&&decoded==target,"Intro canonical codec changed values");wire["completed"]=true;need(!scenario_intro::decode(wire,decoded,e),"Intro active/completed conflict accepted");wire=scenario_intro::encode(target);wire["phase"]=true;need(!scenario_intro::decode(wire,decoded,e),"Intro Boolean accepted as phase");
}
}

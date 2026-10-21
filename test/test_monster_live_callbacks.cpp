#include "vendor/nlohmann/json.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
size_t checks=0;void need(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
using LiveUpdate=void(*)(uintptr_t,float);using LiveAdjust=float(*)(uintptr_t,int32_t,float,uintptr_t,bool);
struct Memory{};struct LiveLocal{struct{uint64_t uid=0;}entry;};struct LiveWorld{struct{int32_t saves=0,edits=0;uintptr_t active_graph=0;}world;};
struct {uintptr_t image=0x100000;uint32_t thread=1;}state;
struct Hook{void* original=nullptr;};Hook live_update_hook,live_adjust_hook;
uintptr_t fixture_caller=0;uint32_t thread_id=1;void* test_return_address(){return reinterpret_cast<void*>(fixture_caller);}uint32_t GetCurrentThreadId(){return thread_id;}
#define _ReturnAddress test_return_address
bool fail(std::string& e,const std::string& why){e=why;return false;}
uintptr_t fixture_ap=0,fixture_zone=0;bool memory_okay=true,hook_okay=true;
bool field(Memory&,uintptr_t,size_t offset,uintptr_t& out,std::string& e){if(!memory_okay)return fail(e,"read cause");out=offset==0?fixture_ap:fixture_zone;return true;}
bool live_hook_current(Memory&,const Hook&,std::string& e){return hook_okay||fail(e,"hook cause");}
int original_updates=0,original_adjustments=0;std::array<float,2> args{};
void original_update(uintptr_t object,float dt){++original_updates;args={float(object),dt};}
float original_adjust(uintptr_t object,int32_t attribute,float delta,uintptr_t source,bool fifth){++original_adjustments;need(object==55&&attribute==2&&source==99&&fifth,"float ABI arguments changed");return delta*0.25f;}
class MonsterLivePort {
public:
    bool enabled=true,world_okay=true,full_okay=true,actor_okay=true,present_okay=true;int failures=0,updates=0,adjustments=0;std::string cause;
    std::atomic<bool> wrong_thread_{false};nlohmann::json callback_scope_;
    bool active()const{return enabled;}
    bool world(Memory&,LiveWorld&,std::string& e,bool idle){need(!idle,"callback incorrectly requires idle world");return world_okay||fail(e,"world cause");}
    bool full(Memory&,const LiveWorld&,uintptr_t,decltype(LiveLocal::entry)&,std::string& birth,std::string& e){birth="birth";return full_okay||fail(e,"full UID cause");}
    bool actor(Memory&,uintptr_t,LiveLocal&,std::string& e){return actor_okay||fail(e,"actor cause");}
    bool classify(uintptr_t,bool&,LiveLocal&,std::string&);
    void failed(const std::string& e){++failures;cause=e;}
    bool present(uintptr_t,LiveLocal&,float dt,std::string& e){++updates;return (present_okay&&dt==0)||fail(e,"presentation cause");}
    void adjustment(){++adjustments;}
}monster_live_port;
#include "monster_live_classify.inc"
namespace management_runtime {bool intervention_replaying(){return false;}}
namespace interventions::actors::live {
bool monster_update(uintptr_t,float,bool& owned,std::string& e){owned=false;e.clear();return true;}
bool adjustment(uintptr_t,bool& owned,std::string& e){owned=false;e.clear();return true;}
}
#include "monster_live_callbacks.inc"
void reset(){monster_live_port.enabled=true;monster_live_port.world_okay=monster_live_port.full_okay=monster_live_port.actor_okay=monster_live_port.present_okay=true;
    monster_live_port.failures=monster_live_port.updates=monster_live_port.adjustments=0;monster_live_port.wrong_thread_=false;monster_live_port.cause.clear();
    original_updates=original_adjustments=0;memory_okay=hook_okay=true;fixture_ap=state.image+0x16c0770;fixture_zone=0;fixture_caller=state.image+0x29457d;thread_id=1;}
}
int main(){try{live_update_hook.original=reinterpret_cast<void*>(&original_update);live_adjust_hook.original=reinterpret_cast<void*>(&original_adjust);
    reset();monster_live_port.enabled=false;live_update_entry(55,0.75f);need(original_updates==1&&args[1]==0.75f,"offline/Host update not forwarded");
    need(live_adjust_entry(55,2,-7.0f,99,true)==-1.75f&&original_adjustments==1,"offline/Host float return changed");
    for(int foreign=0;foreign<2;++foreign){reset();if(foreign==0)fixture_ap=state.image+0x1744e90;else fixture_zone=66;live_update_entry(55,0.5f);
        need(original_updates==1&&monster_live_port.updates==0,"foreign/Zone gameplay suppressed");need(live_adjust_entry(55,2,3.0f,99,true)==0.75f&&original_adjustments==1,"foreign float forward changed");}
    for(uintptr_t return_rva:{0x29457d,0x2945ba,0x2946b8}){reset();fixture_caller=state.image+return_rva;live_update_entry(55,0);need(original_updates==0&&monster_live_port.updates==1&&!monster_live_port.failures,"actual manager fixture_caller rejected");}
    reset();need(live_adjust_entry(55,2,-12.0f,99,true)==0&&original_adjustments==0&&monster_live_port.adjustments==1,"Client owned damage mutated local gameplay");
    for(int failure=0;failure<8;++failure){reset();if(failure==0)memory_okay=false;if(failure==1)monster_live_port.world_okay=false;if(failure==2)monster_live_port.full_okay=false;
        if(failure==3)monster_live_port.actor_okay=false;if(failure==4)fixture_caller++;if(failure==5)hook_okay=false;if(failure==6)monster_live_port.present_okay=false;if(failure==7)thread_id=2;
        live_update_entry(55,0);need(original_updates==0&&monster_live_port.failures==1&&!monster_live_port.cause.empty(),"failed owned guard fell through to gameplay");
        if(failure==7)need(monster_live_port.wrong_thread_.load(),"off-thread callback not retained");}
    reset();hook_okay=false;need(live_adjust_entry(55,2,-4.0f,99,true)==0&&original_adjustments==0&&monster_live_port.failures==1,"unknown damage hook replayed");
    reset();live_update_entry(55,0.1f);need(monster_live_port.failures==1&&original_updates==0,"nonzero Client clock silently advanced presentation");
    std::cout<<checks<<" actual Monster ownership/update/float-return gates passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}

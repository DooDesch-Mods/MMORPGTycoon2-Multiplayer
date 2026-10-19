#include "release_edit_native.h"
#include <iostream>
#include <vector>
#include <atomic>
#include <optional>
#include "mt2mp/game/native_engine.h"
namespace p=mt2mp::game::release_edit;
namespace {
int checks=0;void need(bool value,const std::string& e){++checks;if(!value)throw std::runtime_error(e);}
struct Runtime{uintptr_t image=10;};struct Operation{uint32_t default_steps=0;};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
int fail_step=-1;bool read_failure=false;std::vector<int> calls;
namespace n {using Returned=mt2mp::game::release_edit_native::Returned;uint32_t default_step(uintptr_t image,uintptr_t release,bool announcement,Returned&,unsigned step)noexcept{if(image!=10||release!=20)return 999;calls.push_back(int(step)+(announcement?0:10));return int(step)==fail_step?123:0;}}
bool string(Runtime&,uintptr_t,std::string& out,std::string& e){out="Original default";if(read_failure){e="fixture native read interrupted";return false;}return true;}
#include "release_edit_actual.inc"
void run(){Runtime r;Operation op;p::Defaults out;std::string e;
    need(defaults(r,20,out,op,e)&&op.default_steps==1023&&calls==std::vector<int>({0,1,2,3,4,10,11,12,13,14}),e);
    calls.clear();op={};fail_step=0;need(!defaults(r,20,out,op,e)&&calls==std::vector<int>({0})&&op.default_steps==0&&e.find("getter/format")!=std::string::npos,"throwing constructor was destroyed/replayed");
    calls.clear();op={};fail_step=1;need(!defaults(r,20,out,op,e)&&calls==std::vector<int>({0,1,3,4})&&op.default_steps==25,"returned Loc value leaked or partial string was destroyed");
    calls.clear();op={};fail_step=-1;read_failure=true;need(!defaults(r,20,out,op,e)&&calls==std::vector<int>({0,1,2,3,4})&&e=="fixture native read interrupted","read cause lost during complete returned cleanup");
    calls.clear();op={};read_failure=false;fail_step=2;need(!defaults(r,20,out,op,e)&&calls==std::vector<int>({0,1,2})&&e.find("cleanup")!=std::string::npos,"failed owning destructor replayed");
    calls.clear();op={};read_failure=true;need(!defaults(r,20,out,op,e)&&e.find("fixture native read interrupted; cleanup")!=std::string::npos,"upstream first failure lost");
}
}
namespace retirement_fixture {
uint32_t thread=7;bool readable=true;unsigned reads=0;
mt2mp::game::native::WorldStatus world;
struct Runtime {
    uint32_t thread=7;bool ready=true;uintptr_t image=10;
    std::optional<int> pending=42;std::vector<int> presentations{1},operations{2};
    bool unknown=true,replay=false;std::string cause="retained original cause";
    bool operator==(const Runtime&)const=default;
};
namespace cycle {enum class Phase {Idle,Waiting};}
struct Cycle {
    std::optional<int> pending=1,context=2,binding=3;std::vector<int> operations{4};
    std::string cycle="cycle",last_terminal="terminal",finalize_id="finalize";
    cycle::Phase phase=cycle::Phase::Waiting;bool local_finished=true;
    bool operator==(const Cycle&)const=default;
};
}
uint32_t GetCurrentThreadId(){return retirement_fixture::thread;}
namespace mt2mp::game::native {
bool read_world_status(WorldStatus& out,std::string& e){
    ++retirement_fixture::reads;
    if(!retirement_fixture::readable){e="original WorldStatus read failed";return false;}
    out=retirement_fixture::world;return true;
}
}
namespace retirement_fixture {
namespace native=mt2mp::game::native;
namespace release {
std::atomic<Runtime*> published{nullptr};Cycle cycle_runtime;
#include "release_retire_actual.inc"
}
namespace project {
std::atomic<Runtime*> published{nullptr};
#include "general_project_retire_actual.inc"
}
void run(){
    need(native::WorldStatus{}.saves_in_progress==-1,"real Title status save sentinel differs");
    for(bool is_release:{true,false}){
        auto& published=is_release?release::published:project::published;
        auto retire=is_release?release::retire_lobby:project::retire_lobby;
        std::string e;reads=0;thread=7;readable=true;published.store(nullptr);
        need(retire(e)&&e.empty()&&!reads,"absent owner performed native retirement work");
        for(int saves:{-1,0}){
            Runtime value;published.store(&value);release::cycle_runtime=Cycle{};
            world={};world.active_mode_index=2;world.saves_in_progress=saves;
            need(retire(e)&&e.empty(),"actual idle Title save sentinel rejected");
            need(!value.pending&&value.presentations.empty()&&value.operations.empty()&&value.cause.empty(),"retirement retained old action state");
            need(value.ready&&value.image==10&&published.load()==&value,"retirement discarded installed hook owner");
            if(is_release){const auto& actual_cycle=release::cycle_runtime;
                need(!value.unknown&&!actual_cycle.pending&&actual_cycle.operations.empty()&&!actual_cycle.context&&!actual_cycle.binding&&actual_cycle.cycle.empty()&&actual_cycle.last_terminal.empty()&&actual_cycle.finalize_id.empty()&&actual_cycle.phase==cycle::Phase::Idle&&!actual_cycle.local_finished,"Release cycle not fully retired");}
        }
        for(int guard=0;guard<(is_release?6:7);++guard){
            Runtime value;release::cycle_runtime=Cycle{};world={};world.active_mode_index=2;thread=7;readable=true;
            if(guard==0)world.active_mode_index=3;
            if(guard==1)world.title_transition_pending=true;
            if(guard==2)world.saves_in_progress=1;
            if(guard==3)world.saves_in_progress=INT32_MAX;
            if(guard==4)readable=false;
            if(guard==5)thread=8;
            if(guard==6)value.replay=true;
            const auto before=value;const auto cycle_before=release::cycle_runtime;published.store(&value);e.clear();
            need(!retire(e),"invalid Title/read/thread/replay admitted retirement");
            need(value==before&&release::cycle_runtime==cycle_before&&published.load()==&value,"rejected retirement changed retained owner or original cause");
            if(guard==4)need(e=="original WorldStatus read failed","upstream native read failure lost");
        }
        published.store(nullptr);
    }
}
}
int main(){try{run();retirement_fixture::run();std::cout<<checks<<" actual Release default ownership/cleanup checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "live_host_state/apply_timing.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
#include <atomic>
#include <bit>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace timing=mt2mp::game::live_host_state::apply_timing;
namespace mt2mp::game::native {
static std::atomic<uintptr_t> image=0x140000000;
bool ready(){return image.load()!=0;}
#include "engine_phase_entry.inc"
}
namespace mt2mp::hooks {
struct Call {void* target;void* detour;};
static std::vector<Call> calls;
static int failed=-1,null_original=-1;
bool create_hook(void* target,void* detour,void** original){
    const int slot=int(calls.size());calls.push_back({target,detour});
    if(slot==failed)return false;
    *original=slot==null_original?nullptr:target;return true;
}
}
namespace mt2mp {
#include "engine_phase_hooks.inc"
}
namespace {
unsigned checks=0,entered=0;void* seen_receiver=nullptr;void* seen_pipeline=nullptr;void* seen_options=nullptr;uint32_t seen_dt=0;
bool throw_cpp=false,raise_seh=false,reenter=false;
void need(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
void update(void* receiver,float dt){
    ++entered;seen_receiver=receiver;seen_dt=std::bit_cast<uint32_t>(dt);
    if(throw_cpp)throw std::runtime_error("original update failure");
    if(raise_seh)RaiseException(0xe0123456,0,0,nullptr);
    if(reenter){reenter=false;mt2mp::timed_screen_update(receiver,dt);}
}
void draw(void* receiver,void* pipeline,void* options){++entered;seen_receiver=receiver;seen_pipeline=pipeline;seen_options=options;if(throw_cpp)throw std::runtime_error("original draw failure");if(raise_seh)RaiseException(0xe0123457,0,0,nullptr);}
DWORD seh_call(){
    __try {mt2mp::timed_mode_update(reinterpret_cast<void*>(0x1234),0.5f);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD draw_seh_call(){
    __try {mt2mp::timed_screen_draw_pipeline(nullptr,nullptr,nullptr);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void reset(){
    timing::totals={};entered=0;seen_receiver=seen_pipeline=seen_options=nullptr;seen_dt=0;throw_cpp=raise_seh=reenter=false;
    mt2mp::original_timed_mode_update=update;mt2mp::original_timed_screen_update=update;mt2mp::original_timed_screen_draw_pipeline=draw;
}
void registration(){
    using namespace mt2mp;std::string error;
    for(int failure=-1;failure<3;++failure){
        hooks::calls.clear();hooks::failed=failure;hooks::null_original=-1;
        const bool result=install_engine_timing_hooks(error);
        need(result==(failure<0),"registration result changed");
        need(hooks::calls.size()==size_t(failure<0?3:failure+1),"registration retried or continued after failure");
        const char* causes[]={"native diagnostic Mode Update hook failed","native diagnostic Screen Update hook failed","native diagnostic Screen DrawPipeline hook failed"};
        if(failure>=0)need(error==causes[failure],"registration cause lost");else need(error.empty(),"successful registration retained cause");
        constexpr uintptr_t rvas[]={0x4a1bb0,0x9ca0e0,0x9cb150};
        void* detours[]={reinterpret_cast<void*>(&timed_mode_update),reinterpret_cast<void*>(&timed_screen_update),reinterpret_cast<void*>(&timed_screen_draw_pipeline)};
        for(size_t i=0;i<hooks::calls.size();++i){need(uintptr_t(hooks::calls[i].target)==game::native::image.load()+rvas[i],"wrong actual entrypoint");need(hooks::calls[i].detour==detours[i],"wrong typed detour");}
    }
    for(int slot=0;slot<3;++slot){hooks::calls.clear();hooks::failed=-1;hooks::null_original=slot;original_timed_mode_update=nullptr;original_timed_screen_update=nullptr;original_timed_screen_draw_pipeline=nullptr;need(!install_engine_timing_hooks(error)&&hooks::calls.size()==size_t(slot+1),"null original accepted");}
    game::native::image=0;need(game::native::entrypoint(game::native::EntryPoint::ScreenDrawPipeline)==nullptr,"unresolved entry admitted");game::native::image=0x140000000;
    need(game::native::entrypoint(static_cast<game::native::EntryPoint>(1000))==nullptr,"invalid entry index admitted");
}
}
int main(){try{
    registration();
    for(auto wrapper:{&mt2mp::timed_mode_update,&mt2mp::timed_screen_update}){
        for(uint32_t bits:{0u,0x80000000u,0x3f800000u,0x7fc12345u}){
            reset();void* receiver=reinterpret_cast<void*>(0x12345678);wrapper(receiver,std::bit_cast<float>(bits));
            need(entered==1&&seen_receiver==receiver&&seen_dt==bits,"update receiver/dt bits or exactly-once behavior changed");
            const auto own=wrapper==&mt2mp::timed_mode_update?timing::Part::ModeUpdate:timing::Part::ScreenUpdate;
            need(timing::totals[size_t(own)].count==1,"update counter missing");
            need(timing::totals[size_t(own)].max_ns==timing::totals[size_t(own)].total_ns,"single call timing differs");
        }
    }
    reset();mt2mp::timed_screen_draw_pipeline(reinterpret_cast<void*>(uintptr_t(0x87654321)),reinterpret_cast<void*>(uintptr_t(0x76543210)),reinterpret_cast<void*>(uintptr_t(0x65432100)));
    need(entered==1&&seen_receiver==reinterpret_cast<void*>(uintptr_t(0x87654321))&&timing::totals[size_t(timing::Part::ScreenDrawPipeline)].count==1,"draw forwarding/counter changed");
    need(seen_pipeline==reinterpret_cast<void*>(uintptr_t(0x76543210))&&seen_options==reinterpret_cast<void*>(uintptr_t(0x65432100)),"pipeline/options pointers changed");
    reset();mt2mp::timed_screen_draw_pipeline(nullptr,reinterpret_cast<void*>(uintptr_t(0x76543210)),nullptr);
    need(entered==1&&seen_receiver==nullptr&&seen_pipeline==reinterpret_cast<void*>(uintptr_t(0x76543210))&&seen_options==nullptr,"null optional shader options changed");
    reset();reenter=true;mt2mp::timed_mode_update(nullptr,-1.f);
    need(entered==2&&timing::totals[size_t(timing::Part::ModeUpdate)].count==1&&timing::totals[size_t(timing::Part::ScreenUpdate)].count==1,"nested engine calls changed");
    need(timing::totals[size_t(timing::Part::ModeUpdate)].total_ns>=timing::totals[size_t(timing::Part::ScreenUpdate)].total_ns,"inclusive timing lost");
    reset();throw_cpp=true;bool caught=false;try{mt2mp::timed_mode_update(nullptr,1);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original update failure";}
    need(caught&&entered==1&&timing::totals[size_t(timing::Part::ModeUpdate)].count==1,"original update exception changed");
    reset();throw_cpp=true;caught=false;try{mt2mp::timed_screen_draw_pipeline(nullptr,nullptr,nullptr);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original draw failure";}
    need(caught&&entered==1&&timing::totals[size_t(timing::Part::ScreenDrawPipeline)].count==1,"original draw exception changed");
    reset();raise_seh=true;need(seh_call()==0xe0123456&&entered==1,"original SEH code changed or replayed");
    reset();raise_seh=true;need(draw_seh_call()==0xe0123457&&entered==1,"original DrawPipeline SEH changed or replayed");
    reset();timing::query_phase=timing::QueryPhase::Economy;mt2mp::timed_screen_draw_pipeline(nullptr,nullptr,nullptr);
    need(timing::query_phase==timing::QueryPhase::Economy&&!timing::take_depth,"new counters changed query phase/take depth");
    const auto snapshot=timing::snapshot();need(snapshot.contains("mode_update")&&snapshot.contains("screen_update")&&snapshot["screen_draw_pipeline"]["count"]==1,"existing status omits engine counters");
    std::thread other([]{mt2mp::timed_screen_draw_pipeline(nullptr,nullptr,nullptr);});other.join();need(timing::totals[size_t(timing::Part::ScreenDrawPipeline)].count==1,"foreign thread changed calling thread totals");
    struct Guard {uintptr_t rva;const char* bytes;size_t size;};constexpr Guard guards[]={
#include "engine_phase_guards.inc"
    };
    need(std::size(guards)==3&&guards[0].rva==0x4a1bb0&&guards[1].rva==0x9ca0e0&&guards[2].rva==0x9cb150,"resolver guard selection changed");
    const unsigned char draw_prefix[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xe8};
    need(guards[2].size==sizeof(draw_prefix)&&!std::memcmp(guards[2].bytes,draw_prefix,sizeof(draw_prefix)),"actual DrawPipeline entry guard differs");
    std::cout<<checks<<" actual engine timing checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"failure after "<<checks<<": "<<e.what()<<"\n";return 1;}}

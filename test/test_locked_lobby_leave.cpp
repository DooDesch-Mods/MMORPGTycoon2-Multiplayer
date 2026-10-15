#include "bridge.h"
#include "economic_fence.h"
#include "mt2mp/game/native_engine.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace action=mt2mp::action_session;
namespace fixture {
uint32_t thread=7;unsigned world_reads=0,route_reads=0,checks=0;
int mode=3,saves=0;bool transition=false,world_ok=true;
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void check(bool ok,const std::string& why){++checks;need(ok,why);}
void reset(){thread=7;world_reads=route_reads=0;mode=3;saves=0;transition=false;world_ok=true;}
}
uint32_t GetCurrentThreadId(){return fixture::thread;}
namespace mt2mp::game::native {
bool read_world_status(WorldStatus& out,std::string& e){++fixture::world_reads;if(!fixture::world_ok){e="original native Title read failed";return false;}out={};out.active_mode_index=fixture::mode;out.title_transition_pending=fixture::transition;out.saves_in_progress=fixture::saves;return true;}
}
namespace mt2mp::game::live_host_state {
namespace {bool fail(std::string& e,const std::string& cause){e=cause;return false;}}
bool EconomicFence::routes_ready(std::string&){++fixture::route_reads;return true;}
bool EconomicFence::read_current_fence(const Pin&,std::string& e){return fail(e,"fixture must not perform native stores");}
bool read_time_authority(const Context&,uint64_t&,std::string& e){return fail(e,"fixture must not rebind old Time authority");}
bool day::Sync::suspend_snapshot(std::string& e){return fail(e,"fixture Day snapshot called during Leave");}
bool day::Sync::rebind_snapshot(const Context&,const Binding&,std::string& e){return fail(e,"fixture Day rebind called during Leave");}
bool day::economy::Coupling::rebind_snapshot(std::string& e){return fail(e,"fixture Economy rebind called during Leave");}
#include "bridge_leave.inc"
#include "fence_suspend.inc"
#include "fence_retire.inc"
}
namespace {
struct Port:live::Port {
    unsigned reads=0,stores=0;
    bool read(live::Pin&,std::string& e)override{++reads;e="unexpected physical old-World read";return false;}
    bool store(const live::Pin&,size_t,int32_t,int32_t,std::string& e)override{++stores;e="unexpected old-World store";return false;}
};
struct Transport:action::Transport {
    unsigned sends=0;
    bool claim(Handler,std::string& e)override{e="unexpected transport claim";return false;}
    bool send(uint64_t,const std::vector<uint8_t>&,std::string& e)override{++sends;e="unexpected send";return false;}
};
live::Context context(){return {41,2,1,3,action::Role::Client,"11111111-1111-4111-8111-111111111111"};}
live::EconomicFence fence(){live::EconomicFence f;f.prepared_=f.armed_=true;f.pin_.image=0x140000000;f.pin_.thread=7;f.context_=context();f.binding_={7,9};return f;}
void bridge_cases(){
    fixture::reset();Port port;Transport transport;std::string e;unsigned clocks=0;
    live::Bridge bridge(context(),{7,9},7,port,transport,[&](uint64_t&,std::string&){++clocks;return false;});
    bridge.locked_=true;bridge.queued_=live::Frame{};bridge.queued_->revision=19;
    bridge.receipt_.attempted=bridge.receipt_.unknown=true;bridge.receipt_.cause="actual Economy partial NPC pose mismatch";
    bridge.network_activation_receipt_.attempted_events=3;const auto receipt=bridge.receipt_;
    fixture::check(!bridge.suspend_snapshot(e),"normal snapshot suspension accepted a retained Unknown");
    fixture::check(bridge.suspend_lobby(e)&&bridge.suspended_&&bridge.lobby_retiring_&&bridge.locked_,"explicit idle Leave could not suspend retained Unknown");
    fixture::check(bridge.queued_&&bridge.queued_->revision==19&&bridge.receipt_.cause==receipt.cause&&bridge.receipt_.unknown&&bridge.receipt_.attempted&&bridge.network_activation_receipt_.attempted_events==3,"Leave erased or confirmed the failed frame/receipts");
    fixture::check(!bridge.rebind_snapshot(context(),{8,10},8,e)&&!port.reads&&!clocks&&!transport.sends,"retiring Unknown rebound or touched the old native pin");
    fixture::check(!bridge.suspend_lobby(e)&&bridge.receipt_.cause==receipt.cause,"repeated Leave suspension overwrote original Unknown");
    live::Bridge active(context(),{7,9},7,port,transport,{});active.host_before_=live::Frame{};
    fixture::check(!active.suspend_lobby(e)&&!active.suspended_&&!active.lobby_retiring_,"active Host-before action was retired");
    live::Bridge ordinary(context(),{7,9},7,port,transport,{});ordinary.queued_=live::Frame{};
    fixture::check(ordinary.suspend_lobby(e)&&ordinary.queued_.has_value()&&!ordinary.locked_,"queued but unexecuted frame could not be retained during Leave");
    fixture::check(!ordinary.rebind_snapshot(context(),{8,10},8,e)&&!port.reads&&!port.stores,"healthy Leave was mistaken for snapshot rebind");
}
void fence_cases(){
    fixture::reset();std::string e;auto f=fence();f.locked_=true;f.cause_="original economic native failure";std::vector<std::string> logs;
    f.logger_=[&](const char* level,const std::string& cause){logs.push_back(std::string(level)+" "+cause);};
    fixture::check(!f.suspend_snapshot(e)&&f.locked_&&f.cause_=="original economic native failure","snapshot path cleared economic Unknown");
    fixture::check(f.suspend_lobby(e)&&f.armed_&&f.suspended_&&f.lobby_retiring_&&f.locked_&&f.cause_=="original economic native failure","explicit Leave failed to preserve locked native owner");
    fixture::check(!f.retire_lobby(e)&&f.armed_&&f.locked_&&f.retired_cause_.empty(),"active World retired economic owner");
    fixture::mode=2;fixture::transition=true;fixture::check(!f.retire_lobby(e)&&f.armed_,"transitioning Title retired owner");
    fixture::transition=false;fixture::saves=1;fixture::check(!f.retire_lobby(e)&&f.armed_,"active save retired owner");
    fixture::saves=0;fixture::world_ok=false;fixture::check(!f.retire_lobby(e)&&e=="original native Title read failed"&&f.cause_=="original economic native failure","native read failure was hidden");
    fixture::world_ok=true;fixture::check(f.retire_lobby(e)&&!f.armed_&&!f.locked_&&!f.suspended_&&!f.lobby_retiring_&&f.retired_cause()=="original economic native failure"&&f.pin_.image==0x140000000,"actual idle Title did not retire only the old binding and retain original failure");
    fixture::check(logs.size()==1&&logs[0].find("warn ")==0&&logs[0].find("original economic native failure")!=std::string::npos,"retired original cause was not visibly retained");
    fixture::check(f.can_arm(e)&&fixture::route_reads==1&&f.retired_cause()=="original economic native failure","retired binding prevented new session or erased historical cause");
    const auto reads=fixture::world_reads;fixture::check(f.retire_lobby(e)&&fixture::world_reads==reads&&logs.size()==1&&f.retired_cause()=="original economic native failure","already retired fence repeated native work or lost receipt");
    for(unsigned kind=0;kind<5;++kind){auto blocked=fence();if(kind==0)blocked.armed_=false;if(kind==1)blocked.action_=action::Message{};if(kind==2)blocked.frame_depth_=2;if(kind==3)fixture::thread=8;if(kind==4)blocked.suspended_=true;
        fixture::check(!blocked.suspend_lobby(e)&&!blocked.lobby_retiring_,"unbound/action/reentry/thread/repeated owner entered Leave suspension");fixture::thread=7;}
    for(unsigned depth:{0u,1u}){auto idle=fence();idle.frame_depth_=depth;fixture::check(idle.suspend_lobby(e),"actual top-level UI frame could not request Leave");if(depth){fixture::check(!idle.retire_lobby(e)&&idle.armed_,"active native frame released fence before unwinding");}}
    auto unscoped=fence();unscoped.suspended_=unscoped.locked_=true;fixture::check(!unscoped.retire_lobby(e)&&unscoped.armed_,"ordinary snapshot suspension bypassed Unknown retirement scope");
}
}
int main(){try{bridge_cases();fence_cases();std::cout<<fixture::checks<<" actual Bridge/Fence locked Leave cases passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}

#include "movement_input.h"
#include "tutorial_replication/state.h"
#include <iostream>
#include <stdexcept>
namespace mt2mp::game::tutorial_movement {
namespace native {struct WorldStatus {int active_mode_index=3,in_game_state=4,saves_in_progress=0;bool title_transition_pending=false;};}
namespace {
using SDL_Window=int;
Fixture fixture;FocusReceipt focus_receipt;
Snapshot native_snapshot{"22222222-2222-4222-8222-222222222222",1,2,3,4,5,0,0,false,false,false};
bool observed=true,raise_ok=true;unsigned raises=0;
FocusWindowState window_observation;unsigned window_observations=0;
bool reject(std::string& e,const std::string& cause){e=cause;return false;}
bool observe(void*,Snapshot& out,std::string& e){if(!observed)return reject(e,"original Host ownership read failed");out=native_snapshot;return true;}
FocusWindowState observe_focus_window(const Snapshot& owner){
    if(owner.window!=native_snapshot.window||owner.hwnd!=native_snapshot.hwnd)throw std::runtime_error("focus observation targeted a different window");
    ++window_observations;return window_observation;
}
bool SDL_RaiseWindow(SDL_Window* target){if(reinterpret_cast<uintptr_t>(target)!=native_snapshot.window)throw std::runtime_error("raise targeted a different window");++raises;return raise_ok;}
const char* SDL_GetError(){return "original SDL raise failure";}
}
#include "focus_bodies.inc"
unsigned run_focus_checks(){
    unsigned checks=0;std::string error;FocusReceipt result;
    auto require=[](bool value,const char* cause){if(!value)throw std::runtime_error(cause);};
    auto pass=[&](const char* label){++checks;std::cout<<"PASS "<<label<<'\n';};
    native::WorldStatus status;
    require(idle_world(status,error),"actual InGame3/state4 rejected");
    status.active_mode_index=4;require(!idle_world(status,error)&&error.find("mode3; actual=4")!=std::string::npos,"mode/state confusion accepted");
    status.active_mode_index=3;status.in_game_state=3;require(!idle_world(status,error)&&error.find("state4; actual=3")!=std::string::npos,"loading state admitted");
    status.in_game_state=4;status.title_transition_pending=true;require(!idle_world(status,error),"Title transition admitted");
    status.title_transition_pending=false;status.saves_in_progress=1;require(!idle_world(status,error),"pending save admitted");
    pass("actual world guard keeps mode3 and idle state4 distinct and rejects pending transitions/saves");
    for(uint32_t mods:{0u,0x1000u,0x2000u,0x8000u,0xb000u})require(keyboard_admission(mods,'w',false,false,false,true,true,error),"benign lock rejected fixed native W");
    for(uint32_t mods:{1u,0x40u,0x100u,0x4000u,0x10000u})require(!keyboard_admission(mods,'w',false,false,false,true,true,error)&&error.find("blocking=")!=std::string::npos,"held/unknown modifier admitted");
    require(!keyboard_admission(0x1000,'w',true,false,false,true,true,error)&&error.find("text=1")!=std::string::npos,"SDL text capture admitted or hidden");
    require(!keyboard_admission(0,'w',false,true,false,true,true,error)&&error.find("native_string_mode=1")!=std::string::npos,"native string input admitted or hidden");
    require(!keyboard_admission(0,'w',false,false,true,true,true,error)&&error.find("native_string_clearing=1")!=std::string::npos,"native clearing input admitted or hidden");
    require(!keyboard_admission(0,'W',false,false,false,true,true,error),"wrong actual event key admitted");
    pass("actual keyboard guard admits benign locks only and retains text/native blockers with measurements");
    tutorial_replication::Cursor step;step.scenario="initial_tutorial";step.next=7;step.message_progress=tutorial_replication::MessageProgress{true,0,0,100};
    require(wasd_admission(step,error),"native_snapshot WASD current index6/next7 rejected");step.next=6;
    require(!wasd_admission(step,error)&&error.find("index6/next7")!=std::string::npos,"off-by-one cursor accepted");pass("native_snapshot native admission predicate accepts next7 and rejects next6");
    window_observation.observed=true;window_observation.hidden=true;window_observation.sdl_flags=37;
    require(request_focus(native_snapshot.origin,error)&&raises==1,"exact own Host focus was not requested");
    require(read_focus(result,error)&&result.attempted&&result.returned&&!result.ready,"raise request was confused with delivered focus");
    require(window_observations==1&&result.window_state.observed&&result.window_state.hidden&&result.window_state.sdl_flags==37,"current focus window diagnostics were not retained");
    native_snapshot.keyboard_focus=true;require(read_focus(result,error)&&!result.ready,"SDL focus alone admitted native key");
    native_snapshot.input_enabled=true;require(read_focus(result,error)&&result.ready,"native focus plus input admission not observed");
    require(!request_focus(native_snapshot.origin,error)&&raises==1,"focus repeated automatically");pass("native_snapshot request/read bodies distinguish raised, SDL focus, native admission and no replay");
    const auto window_reads=window_observations;
    observed=false;require(!read_focus(result,error)&&!result.ready&&error=="original Host ownership read failed","failed focus read retained ready success");
    require(window_observations==window_reads&&!result.window_state.observed&&result.window_state.sdl_flags==0,"failed owner read retained stale window diagnostics or observed a window");pass("failed current-owner read clears ready and preserves original cause");
    focus_receipt={};require(!request_focus(native_snapshot.origin,error)&&raises==1,"Host guard failure raised window");pass("native_snapshot Host/world admission failure has no raise effect");
    observed=true;require(!request_focus("11111111-1111-4111-8111-111111111111",error)&&raises==1,"mismatched origin raised window");pass("native_snapshot exact native origin is required before raise");
    focus_receipt={};native_snapshot.keyboard_focus=false;native_snapshot.input_enabled=false;raise_ok=false;
    require(!request_focus(native_snapshot.origin,error)&&raises==2&&error=="tutorial movement SDL_RaiseWindow failed: original SDL raise failure","original SDL failure was hidden");
    require(read_focus(result,error)&&!result.ready&&!result.error.empty()&&!result.returned,"failed raise became success");pass("SDL raise failure remains separate and visible after readback");
    focus_receipt={};native_snapshot.keyboard_focus=true;native_snapshot.input_enabled=true;
    require(request_focus(native_snapshot.origin,error)&&raises==2&&read_focus(result,error)&&result.ready&&!result.attempted,"already ready owner raised unnecessarily");pass("already focused native owner needs no raise effect");
    return checks;
}
}

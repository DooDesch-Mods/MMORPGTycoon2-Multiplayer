#include "movement_input.h"
#include "input_contract.h"
#include "tutorial_replication/native.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "typed_action_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/hooks/hook_window.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/game/native_engine.h"
#include <SDL3/SDL.h>
#include <windows.h>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace mt2mp::game::tutorial_movement {
namespace {
Fixture fixture;placement_ui_probe::ProcessMemory memory;
FocusReceipt focus_receipt;
uintptr_t image=0,sdl=0;uint32_t mainthread=0;
std::string runtime,dll_sha,last_error;
bool reject(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(uintptr_t p,size_t offset,T& value,std::string& e){if(!p||p>UINTPTR_MAX-offset)return reject(e,"tutorial movement native address invalid");return memory.copy(p+offset,&value,sizeof(value),e);}
struct Array {uintptr_t ap=0,data=0;int32_t count=0,capacity=0;bool operator==(const Array&)const=default;};
bool array(uintptr_t p,uintptr_t ap,size_t stride,Array& a,std::string& e){
    if(!at(p,0,a,e))return false;
    return a.ap==image+ap&&a.count>=0&&a.count<=4096&&a.capacity>=a.count&&a.capacity<=4096&&(!a.count||(a.data&&size_t(a.count)<=(UINTPTR_MAX-a.data)/stride))||reject(e,"tutorial movement native array contract differs");
}
DWORD modal_call(bool* modal){__try{*modal=reinterpret_cast<bool(*)()>(image+0x81a350)();return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool idle_world(const native::WorldStatus& status,std::string& e){
    if(status.active_mode_index!=3)return reject(e,"tutorial movement requires InGame mode3; actual="+std::to_string(status.active_mode_index));
    if(status.in_game_state!=4)return reject(e,"tutorial movement requires idle world state4; actual="+std::to_string(status.in_game_state));
    if(status.title_transition_pending)return reject(e,"tutorial movement Title transition pending");
    if(status.saves_in_progress>0)return reject(e,"tutorial movement native saves pending="+std::to_string(status.saves_in_progress));
    return true;
}
static_assert((SDL_KMOD_NUM|SDL_KMOD_CAPS|SDL_KMOD_SCROLL)==0xb000);
bool keyboard_admission(uint32_t mods,uint32_t event_key,bool text,bool string_mode,bool clearing,bool focus,bool enabled,std::string& e){
    const uint32_t blocking=mods&~uint32_t(0xb000);
    const std::string measured=" mods="+std::to_string(mods)+" blocking="+std::to_string(blocking)+
        " event_key="+std::to_string(event_key)+" text="+std::to_string(text)+
        " native_string_mode="+std::to_string(string_mode)+" native_string_clearing="+std::to_string(clearing)+
        " focus="+std::to_string(focus)+" native_enabled="+std::to_string(enabled);
    if(blocking)return reject(e,"tutorial movement modifiers block W;"+measured);
    if(text)return reject(e,"tutorial movement SDL text input active;"+measured);
    if(string_mode||clearing)return reject(e,"tutorial movement native input blocked;"+measured);
    if(event_key!='w')return reject(e,"tutorial movement event key is not W;"+measured);
    return true;
}
bool observe(void*,Snapshot& out,std::string& e){
    out={};if(GetCurrentThreadId()!=mainthread)return reject(e,"tutorial movement Mainthread differs");
    action_session::Context context;if(!action_session::selected_session().current(context,e))return false;
    if(context.role!=action_session::Role::Host||!Config::instance().is_host)return reject(e,"tutorial movement requires actual Host");
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("initialized",false)||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!controller.value("cause",std::string{}).empty())return reject(e,"tutorial movement Controller is not active idle Host");
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e))return false;
    native::WorldStatus status;if(!native::read_world_status(status,e))return false;
    if(!idle_world(status,e))return false;
    tutorial_replication::State state;if(!tutorial_replication::capture(context,{world.world_epoch,world.generation},state,e))return false;
    if(state.initial_complete||state.active.size()!=1)return reject(e,"tutorial movement requires one active initial tutorial");
    const auto& step=state.active[0];
    if(step.scenario!="initial_tutorial"||step.next!=7||step.complete||!step.message_progress||step.message_progress->minimum!=0||step.message_progress->maximum!=100)return reject(e,"tutorial movement requires current initial WASD index6/next7/Meter");
    if(!verify_contract(memory,image,sdl,e))return false;
    bool modal=false;const auto exception=modal_call(&modal);if(exception)return reject(e,"tutorial movement modal read SEH="+std::to_string(exception));
    if(modal)return reject(e,"tutorial movement modal Window blocks native progress");
    const auto window=hooks::get_game_window();
    if(!window)return reject(e,"tutorial movement owned SDL window absent");
    out.keyboard_focus=SDL_GetKeyboardFocus()==window;
    const auto hwnd=reinterpret_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
    DWORD pid=0;const auto window_thread=GetWindowThreadProcessId(hwnd,&pid);
    if(!hwnd||!IsWindow(hwnd)||pid!=GetCurrentProcessId()||window_thread!=mainthread)return reject(e,"tutorial movement HWND is not current process Mainthread owner");
    out.window_id=SDL_GetWindowID(window);out.window=reinterpret_cast<uintptr_t>(window);out.hwnd=reinterpret_cast<uintptr_t>(hwnd);
    if(!out.window_id||SDL_GetWindowFromID(out.window_id)!=window)return reject(e,"tutorial movement SDL window identity changed");
    int key_count=0;const auto keys=SDL_GetKeyboardState(&key_count);
    if(!keys||key_count<=SDL_SCANCODE_W)return reject(e,"tutorial movement SDL keyboard read unavailable");
    const auto modifiers=SDL_GetModState();
    const bool text_input=SDL_TextInputActive(window);
    const auto event_key=SDL_GetKeyFromScancode(SDL_SCANCODE_W,modifiers,true);
    if(MapVirtualKeyW('W',MAPVK_VK_TO_VSC_EX)!=0x11)return reject(e,"tutorial movement current keyboard mapping does not resolve virtual W to native W");
    uintptr_t input=0,ap=0;if(!at(image,0x13df990,input,e)||!at(input,0,ap,e))return false;
    if(ap!=image+0x1749430)return reject(e,"tutorial movement Input AP differs");
    uint8_t string_mode=0,clearing=0,paused=0,enabled=0;
    if(!at(input,0xa6,string_mode,e)||!at(input,0xa8,clearing,e)||!at(input,0xa9,paused,e)||!at(input,0xc0,enabled,e))return false;
    if(enabled>1)return reject(e,"tutorial movement native keyboard admission byte invalid");out.input_enabled=enabled!=0;
    if(!keyboard_admission(modifiers,event_key,text_input,string_mode!=0,clearing!=0,out.keyboard_focus,out.input_enabled,e)){
        e+=" native_string_paused="+std::to_string(paused);return false;
    }
    Array axes;if(!array(input+0x48,0x17461c0,0x70,axes,e))return false;uint32_t matches=0,controls_read=0;
    if(axes.count<6)return reject(e,"tutorial movement native movement axes absent");
    for(int32_t i=0;i<axes.count;++i){
        const auto axis=axes.data+size_t(i)*0x70;Array controls;if(!array(axis+0x40,0x1746310,0x14,controls,e))return false;
        float value=0;uint8_t subtract=0;if(!at(axis,0x5c,value,e)||!at(axis,0x63,subtract,e))return false;
        if(!std::isfinite(value))return reject(e,"tutorial movement native axis nonfinite");
        if(i>=2&&i<=5){if(subtract)return reject(e,"tutorial movement direction is a derived axis");if(i==2)out.axis=value;else if(value!=0)return reject(e,"tutorial movement other direction is held");}
        for(int32_t j=0;j<controls.count;++j){
            if(++controls_read>4096)return reject(e,"tutorial movement binding scan exceeds4096");
            struct Control{int32_t type,code;uint8_t ctrl;uint8_t pad[3];int32_t extra1,extra2;} control{};
            if(!at(controls.data,size_t(j)*0x14,control,e))return false;
            if((control.type==6&&control.code==SDL_SCANCODE_W)||(control.type==7&&control.code=='w')){
                if(control.ctrl||i!=2||control.type!=6)return reject(e,"tutorial movement W binding is not exclusive native axis2/scancode");++matches;
            }
        }
        Array after;if(!at(axis,0x40,after,e)||after!=controls)return reject(e,e.empty()?"tutorial movement controls changed":e);
    }
    if(matches!=1)return reject(e,"tutorial movement requires exactly one native W binding");
    Array after_axes;if(!at(input,0x48,after_axes,e)||after_axes!=axes)return reject(e,e.empty()?"tutorial movement axes changed":e);
    building_mixed_identity::opened_file::CurrentWorld after;if(!native_open_runtime::read_current_world(after,e))return false;
    if(world.game!=after.game||world.mode!=after.mode||world.state!=after.state||world.world!=after.world||world.uuid!=after.uuid||world.world_epoch!=after.world_epoch||world.generation!=after.generation)return reject(e,"tutorial movement actual origin changed during read");
    out.origin=world.uuid;out.epoch=world.world_epoch;out.generation=world.generation;out.progress=step.message_progress->value;out.key_down=keys[SDL_SCANCODE_W];e.clear();return true;
}
bool post(void*,const Snapshot& owner,bool down,std::string& e){
    if(GetCurrentThreadId()!=mainthread)return reject(e,"tutorial movement post Mainthread differs");
    const auto window=reinterpret_cast<SDL_Window*>(owner.window);const auto hwnd=reinterpret_cast<HWND>(owner.hwnd);
    DWORD pid=0;if(!window||SDL_GetWindowFromID(owner.window_id)!=window||
        SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr)!=hwnd||
        GetWindowThreadProcessId(hwnd,&pid)!=mainthread||pid!=GetCurrentProcessId())return reject(e,"tutorial movement original release window owner changed");
    if(!verify_contract(memory,image,sdl,e))return false;
    // A zero scan code selects SDL's normal virtual-key WindowProc path, including raw-keyboard mode.
    if(!PostMessageW(hwnd,down?WM_KEYDOWN:WM_KEYUP,'W',down?LPARAM(1):LPARAM(0xc0000001)))return reject(e,"tutorial movement PostMessage failed error="+std::to_string(GetLastError()));
    e.clear();return true;
}
void report(const std::string& error){if(!error.empty()&&last_error!=error){last_error=error;LOG_ERROR("[TutorialMovement] cause={}; no replay",error);}}
}
bool initialize(uintptr_t base,uint32_t thread,const std::string& run,const std::string& sha,std::string& e){
    if(image)return reject(e,"tutorial movement initialization already attempted");
    if(base!=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))||thread!=GetCurrentThreadId()||run.empty()||sha.size()!=64)return reject(e,"tutorial movement actual process/runtime identity missing");
    image=base;mainthread=thread;runtime=run;dll_sha=sha;sdl=reinterpret_cast<uintptr_t>(GetModuleHandleW(L"SDL3.dll"));
    if(!sdl)return reject(e,"tutorial movement original SDL module is not loaded");
    std::array<wchar_t,32768> path{};const auto length=GetModuleFileNameW(reinterpret_cast<HMODULE>(sdl),path.data(),DWORD(path.size()));
    if(!length||length==path.size())return reject(e,"tutorial movement SDL module path read failed");
    std::ifstream input(std::filesystem::path(path.data()),std::ios::binary);
    if(!input)return reject(e,"tutorial movement SDL file read failed");
    const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    if(input.bad())return reject(e,"tutorial movement SDL file read failed during hash");
    try{if(mt2mp::sha256(bytes)!=sdl_sha256)return reject(e,"tutorial movement exact SDL build differs");}
    catch(const std::exception& cause){return reject(e,cause.what());}
    if(!verify_contract(memory,image,sdl,e))return false;
    return fixture.initialize(thread,{nullptr,[](void*){return uint32_t(GetCurrentThreadId());},[](void*){return uint64_t(GetTickCount64());},observe,post},e);
}
bool start(const std::string& token,const std::string& origin,uint64_t frame_id,std::string& e){
    if(GetAsyncKeyState('W')&0x8000)return reject(e,"tutorial movement physical W is already held");
    return fixture.start(token,origin,frame_id,e);
}
void poll(void* raw,uint64_t frame_id){
    const auto event=static_cast<const SDL_Event*>(raw);if(!event||(event->type!=SDL_EVENT_KEY_DOWN&&event->type!=SDL_EVENT_KEY_UP))return;
    std::string e;if(!fixture.poll(event->key.windowID,event->key.which,uint32_t(event->key.scancode),event->key.key,event->type==SDL_EVENT_KEY_DOWN,event->key.repeat,frame_id,e))report(e);
}
void frame(uint64_t frame_id){std::string e;if(!fixture.frame(frame_id,e))report(e);}
void suspend(const std::string& cause){std::string e;if(!fixture.suspend(cause,e))report(e);}
const Receipt& receipt(){return fixture.receipt();}
FocusWindowState observe_focus_window(const Snapshot& owner){
    const auto window=reinterpret_cast<SDL_Window*>(owner.window);
    const auto hwnd=reinterpret_cast<HWND>(owner.hwnd);
    FocusWindowState out;
    out.sdl_flags=SDL_GetWindowFlags(window);
    out.hidden=(out.sdl_flags&SDL_WINDOW_HIDDEN)!=0;
    out.minimized=(out.sdl_flags&SDL_WINDOW_MINIMIZED)!=0;
    out.not_focusable=(out.sdl_flags&SDL_WINDOW_NOT_FOCUSABLE)!=0;
    out.input_focus_flag=(out.sdl_flags&SDL_WINDOW_INPUT_FOCUS)!=0;
    out.visible=IsWindowVisible(hwnd)!=0;out.enabled=IsWindowEnabled(hwnd)!=0;out.iconic=IsIconic(hwnd)!=0;
    out.foreground_owned=GetForegroundWindow()==hwnd;
    out.thread_focus_owned=GetFocus()==hwnd;out.thread_active_owned=GetActiveWindow()==hwnd;
    out.force_raise=SDL_GetHintBoolean(SDL_HINT_FORCE_RAISEWINDOW,false);
    out.activate_when_raised=SDL_GetHintBoolean(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED,true);
    out.observed=true;return out;
}
bool read_focus(FocusReceipt& out,std::string& e){
    out=focus_receipt;out.keyboard_focus=false;out.input_enabled=false;out.ready=false;out.window_state={};
    Snapshot actual;if(!observe(nullptr,actual,e))return false;
    if(!out.origin.empty()&&out.origin!=actual.origin)return reject(e,"tutorial movement focus actual origin changed");
    out.window_state=observe_focus_window(actual);
    out.origin=actual.origin;out.keyboard_focus=actual.keyboard_focus;out.input_enabled=actual.input_enabled;
    out.ready=actual.keyboard_focus&&actual.input_enabled;e.clear();return true;
}
bool request_focus(const std::string& origin,std::string& e){
    if(focus_receipt.requested)return reject(e,"tutorial movement focus request already consumed; no replay");
    if(fixture.unresolved())return reject(e,"tutorial movement focus cannot change during unresolved input");
    Snapshot before,after;if(!observe(nullptr,before,e)||!observe(nullptr,after,e))return false;
    if(before!=after||before.origin!=origin)return reject(e,"tutorial movement focus exact native origin/window changed");
    focus_receipt={};focus_receipt.origin=origin;focus_receipt.requested=true;
    if(before.keyboard_focus&&before.input_enabled){focus_receipt.keyboard_focus=focus_receipt.input_enabled=focus_receipt.ready=true;e.clear();return true;}
    focus_receipt.attempted=true;
    focus_receipt.returned=SDL_RaiseWindow(reinterpret_cast<SDL_Window*>(before.window));
    if(!focus_receipt.returned){focus_receipt.error="tutorial movement SDL_RaiseWindow failed: "+std::string(SDL_GetError());return reject(e,focus_receipt.error);}
    e.clear();return true;
}
}

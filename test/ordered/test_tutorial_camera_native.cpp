#include "game/tutorial_movement/camera_input.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
using namespace mt2mp::game::tutorial_movement;
using Uint64=uint64_t;using SDL_MouseID=uint32_t;
struct SDL_Window{};struct SDL_Rect{int x,y,w,h;};
constexpr uint32_t SDL_EVENT_MOUSE_MOTION=0x400,SDL_EVENT_MOUSE_BUTTON_DOWN=0x401,SDL_EVENT_MOUSE_BUTTON_UP=0x402,SDL_EVENT_MOUSE_WHEEL=0x403,SDL_BUTTON_RIGHT=3;
struct Motion{uint32_t type,reserved;uint64_t timestamp;uint32_t windowID,which,state;float x,y,xrel,yrel;};
struct Button{uint32_t type,reserved;uint64_t timestamp;uint32_t windowID,which;uint8_t button;};
struct Wheel{uint32_t type,reserved;uint64_t timestamp;uint32_t windowID,which;float x,y;};
union SDL_Event{uint32_t type;struct {uint32_t type,reserved;uint64_t timestamp;} common;Motion motion;Button button;Wheel wheel;};
SDL_Window fake_window;SDL_Rect fake_rect{100,50,600,400};bool confined=false,pin=true;float mouse_x=-10,mouse_y=900;uint32_t buttons=0;uintptr_t sdl=0;int motions=0;std::string reported;
SHORT physical_button=0;SHORT fake_async_key_state(int){return physical_button;}
#define GetAsyncKeyState fake_async_key_state
bool reject(std::string& e,const std::string& c){e=c;return false;}
bool camera_release_observe(void*,const Snapshot& owner,Snapshot& out,std::string&){out=owner;out.mouse_focus=true;out.mouse_relative=false;out.mouse_capture=false;return true;}
bool camera_pin(std::string& e){if(!pin)e="pin failed";return pin;}
bool SDL_GetWindowSize(SDL_Window*,int* w,int* h){*w=800;*h=600;return true;}
const SDL_Rect* SDL_GetWindowMouseRect(SDL_Window*){return confined?&fake_rect:nullptr;}
uint32_t SDL_GetMouseState(float* x,float* y){if(x)*x=mouse_x;if(y)*y=mouse_y;return buttons;}
uint64_t SDL_GetTicksNS(){return 123;}
void position_ingress(uint64_t,SDL_Window*,uint32_t,bool relative,float x,float y){if(relative)throw std::runtime_error("relative position");++motions;mouse_x=x;mouse_y=y;}
struct FakeFixture {
    struct Receipt {Snapshot before;} value;bool acknowledged=false,suspended=false;uint64_t frame=0;
    const Receipt& receipt()const{return value;}
    bool polled_event(CameraInput input,uint64_t at,std::string&){acknowledged=input==CameraInput::Position;frame=at;return true;}
    bool suspend(const std::string&,std::string&){suspended=true;return false;}
} fake;
// Match the real fixture method name without replacing any production body.
struct FixtureAdapter:FakeFixture {bool polled(CameraInput input,uint64_t at,std::string& e){return polled_event(input,at,e);}} camera_fixture;
bool camera_active(){return true;}void report(const std::string& e){reported=e;}
#include "ordered_tutorial_camera_native_body.inc"
void need(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main()try{
    sdl=reinterpret_cast<uintptr_t>(&position_ingress)-0x5c200;Snapshot owner;owner.window=reinterpret_cast<uintptr_t>(&fake_window);owner.window_id=2;
    camera_fixture.value.before=owner;std::string error;bool pending=false;
    need(camera_prepare_position(nullptr,owner,pending,error)&&pending&&motions==1,"absolute position was not requested");
    need(camera_position.target_x==400&&camera_position.target_y==300&&camera_position_ready(nullptr,owner,error),"actual position readback differs");
    physical_button=SHORT(-32768);need(!camera_position_ready(nullptr,owner,error),"changed physical mouse button admitted");physical_button=0;
    SDL_Event event{};event.motion.type=SDL_EVENT_MOUSE_MOTION;event.motion.timestamp=123;event.motion.windowID=2;event.motion.which=0;
    event.motion.state=0;event.motion.x=400;event.motion.y=300;camera_poll(event,8);
    need(camera_fixture.acknowledged&&camera_fixture.frame==8&&!camera_fixture.suspended&&!camera_position.position_event_bytes.empty(),"exact position event was not acknowledged or retained");
    need(camera_prepare_position(nullptr,owner,pending,error)&&!pending&&motions==1,"unchanged position emitted another event");
    confined=true;mouse_x=0;mouse_y=0;need(camera_prepare_position(nullptr,owner,pending,error)&&pending&&camera_position.target_x==400&&camera_position.target_y==250,"confinement center differs");
    event.motion.windowID=0;camera_poll(event,9);need(camera_fixture.suspended,"foreign position event was admitted");
    buttons=1;need(!camera_prepare_position(nullptr,owner,pending,error),"held button admitted for positioning");
    std::cout<<"actual SDL camera position checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}

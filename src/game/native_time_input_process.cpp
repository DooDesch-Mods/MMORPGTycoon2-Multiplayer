#include "native_time_input.h"
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <SDL3/SDL.h>
#endif
namespace mt2mp::game::native_time_input {
namespace {
#ifdef _WIN32
unsigned long copy_seh(uintptr_t address,void* out,size_t size){
    __try{std::memcpy(out,reinterpret_cast<const void*>(address),size);return 0;}
    __except(GetExceptionCode()==0xe06d7363UL?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool read(void*,uintptr_t address,void* out,size_t size,std::string& error){
    if(!address||!out||!size||address>UINTPTR_MAX-size){error="time_input_read_span_invalid";return false;}
    for(auto p=address;p<address+size;){MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<const void*>(p),&region,sizeof(region))){error="time_input_VirtualQuery_failed error="+std::to_string(GetLastError());return false;}
        if(region.State!=MEM_COMMIT||(region.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(region.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){error="time_input_memory_not_readable";return false;}
        const auto base=reinterpret_cast<uintptr_t>(region.BaseAddress);if(region.RegionSize>UINTPTR_MAX-base||base+region.RegionSize<=p){error="time_input_memory_region_overflow";return false;}p=base+region.RegionSize;
    }
    const auto seh=copy_seh(address,out,size);if(seh){error="time_input_read_SEH="+std::to_string(seh);return false;}return true;
}
unsigned long push_seh(const Event* input,bool* window_valid,bool* accepted){
    __try{
        const auto window=reinterpret_cast<SDL_Window*>(input->owned_window);
        *window_valid=window&&SDL_GetWindowFromID(input->window_id)==window;if(!*window_valid)return 0;
        SDL_Event event{};event.type=input->down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;event.key.windowID=input->window_id;event.key.timestamp=input->timestamp_ns;
        event.key.scancode=SDL_Scancode(input->scancode);event.key.key=input->keycode;event.key.mod=SDL_KMOD_NONE;event.key.down=input->down;event.key.repeat=false;
        *accepted=SDL_PushEvent(&event);return 0;
    }__except(GetExceptionCode()==0xe06d7363UL?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool push(void*,const Event& input,std::string& error){
    bool window_valid=false,accepted=false;const auto seh=push_seh(&input,&window_valid,&accepted);
    if(seh)throw std::runtime_error("time_input_SDL_effect_unknown_SEH="+std::to_string(seh));
    if(!window_valid){error="time_input_owned_SDL_window_changed";return false;}
    if(!accepted){error=std::string(input.down?"SDL key down queue failed: ":"SDL key up queue failed: ")+SDL_GetError();return false;}return true;
}
uint32_t thread(void*){return GetCurrentThreadId();}
uint64_t timestamp(void*){return SDL_GetTicksNS();}
#endif
}
Services process_services(void* user,bool(*context)(void*,Context&,std::string&)){
#ifdef _WIN32
    return {user,thread,context,read,push,timestamp};
#else
    (void)user;(void)context;return {};
#endif
}
}

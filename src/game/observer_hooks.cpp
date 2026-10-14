#include "observer_hooks.h"
#include "node_observer.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/game/native_engine.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
namespace mt2mp::game::node_observer {
namespace {
using VoidFn=void(*)(void*,float);using FloatFn=float(*)(void*,float);
VoidFn display_original=nullptr,network_original=nullptr,finish_original=nullptr;
FloatFn drain_original=nullptr,charge_original=nullptr;
uintptr_t hook_image=0;bool attempted=false,installed=false,prepared=false;std::string install_cause;
uintptr_t caller(void* address)noexcept{const auto p=uintptr_t(address);return p>=hook_image&&p-hook_image<30986240?p-hook_image:0;}
int propagate(Ticket ticket,DWORD code)noexcept{leave(ticket,false,code);return EXCEPTION_CONTINUE_SEARCH;}
void invoke_void(Leaf leaf,VoidFn original,void* object,float argument,uintptr_t origin){
    const auto ticket=enter(leaf,object,origin,argument);
    __try{original(object,argument);}
    __except(propagate(ticket,GetExceptionCode())){__assume(0);}
    leave(ticket,true,0);
}
float invoke_float(Leaf leaf,FloatFn original,void* object,float argument,uintptr_t origin){
    const auto ticket=enter(leaf,object,origin,argument);float result=0;
    __try{result=original(object,argument);}
    __except(propagate(ticket,GetExceptionCode())){__assume(0);}
    leave(ticket,true,0);return result;
}
void display_hook(void* p,float a){invoke_void(Leaf::Presentation,display_original,p,a,caller(_ReturnAddress()));}
void network_hook(void* p,float a){invoke_void(Leaf::Network,network_original,p,a,caller(_ReturnAddress()));}
void finish_hook(void* p,float a){invoke_void(Leaf::Finish,finish_original,p,a,caller(_ReturnAddress()));}
float drain_hook(void* p,float a){return invoke_float(Leaf::Drain,drain_original,p,a,caller(_ReturnAddress()));}
float charge_hook(void* p,float a){return invoke_float(Leaf::Charge,charge_original,p,a,caller(_ReturnAddress()));}
}
bool install(std::string& e){
    if(installed){e.clear();return true;}if(attempted){e=install_cause;return false;}attempted=true;
    auto failed=[&](){install_cause=e.empty()?"node observer hook installation failed without cause":e;e=install_cause;return false;};
    uintptr_t resolved_image=0;uint32_t resolved_thread=0;std::string resolved_sha;
    if(!native::read_resolver_receipt(resolved_image,resolved_thread,resolved_sha,e))return failed();
    if(!prepare(resolved_image,resolved_sha,resolved_thread,e))return failed();hook_image=resolved_image;prepared=true;
    void* detours[]={reinterpret_cast<void*>(&display_hook),reinterpret_cast<void*>(&network_hook),reinterpret_cast<void*>(&finish_hook),reinterpret_cast<void*>(&drain_hook),reinterpret_cast<void*>(&charge_hook)};
    void** originals[]={reinterpret_cast<void**>(&display_original),reinterpret_cast<void**>(&network_original),reinterpret_cast<void**>(&finish_original),reinterpret_cast<void**>(&drain_original),reinterpret_cast<void**>(&charge_original)};
    for(size_t i=0;i<5;++i)if(!hooks::create_hook(entrypoint(Leaf(i)),detours[i],originals[i])||!*originals[i]){e="node observer CreateHook failed index="+std::to_string(i)+"; no reinstall";return failed();}
    if(!hooks::enable_all()){e="node observer EnableAll failed; no reinstall";return failed();}
    if(!set_hook_mask(31,e))return failed();installed=true;e.clear();return true;
}
bool suspend_if_prepared(const std::string& reason,std::string& e){if(!prepared){e.clear();return true;}return suspend(reason,e);}
}

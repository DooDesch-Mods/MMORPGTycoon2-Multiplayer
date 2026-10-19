#include "intervention_actor_native.h"
#include <windows.h>

namespace mt2mp::game::interventions::actors::native {
uint32_t maximum(uintptr_t image,uintptr_t object,int32_t attribute,float& out)noexcept{
    __try{out=reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x39f6e0)(object,attribute);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t clip(uintptr_t image,const Render& render,int32_t semantic,Clip& out)noexcept{
    __try{const int32_t index=render.indexes[size_t(semantic)];
        if(index<0){out.length.reset();out.time.reset();out.paused=false;out.speed=0;return 0;}
        const auto length=reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x23860)(render.actor,index);out.length=std::bit_cast<uint32_t>(length);
        if(reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x28760)(render.character,semantic)){
            out.time=std::bit_cast<uint32_t>(reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x23890)(render.actor,index));
            out.paused=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x23840)(render.actor,index);
            out.speed=std::bit_cast<uint32_t>(reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x238b0)(render.actor,index));
        }else{out.time.reset();out.paused=false;out.speed=0;}return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t clip(uintptr_t image,const Render& render,int32_t semantic,const Clip& value)noexcept{
    __try{const int32_t index=render.indexes[size_t(semantic)];
        if(value.time){reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x28730)(render.character,semantic);
            reinterpret_cast<void(*)(uintptr_t,int32_t,float)>(image+0x237a0)(render.actor,index,std::bit_cast<float>(value.speed));
            reinterpret_cast<void(*)(uintptr_t,int32_t,bool)>(image+0x23770)(render.actor,index,value.paused);
            reinterpret_cast<void(*)(uintptr_t,int32_t,float)>(image+0x237e0)(render.actor,index,std::bit_cast<float>(*value.time));
        }else reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x28750)(render.character,semantic);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t on_map(uintptr_t image,Family family,uintptr_t object,bool value)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t,bool)>(image+(family==Family::Npc?0x3c2a00:0x39a920))(object,value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t ready(uintptr_t image,uintptr_t object,bool value)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t,bool)>(image+0x3bbc00)(object,value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t transform(uintptr_t image,uintptr_t object,const subscriber_motion::Pose& pose)noexcept{
    alignas(4) std::array<unsigned char,0x6c> value{};
    __try{reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(image+0x9ff370)(value.data(),pose.orientation.data(),pose.position.data(),pose.scale.data());
        reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xc67c80)(object+0x30,value.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t spatial(uintptr_t image,uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(image+0x3b1b50)(object);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t registered(uintptr_t image,uint64_t uid,uintptr_t& object)noexcept{
    __try{object=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t scene(uintptr_t image,uintptr_t screen,uintptr_t& out)noexcept{
    __try{out=reinterpret_cast<uintptr_t(*)(uintptr_t,int32_t)>(image+0x9ca3a0)(screen,0);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t present(uintptr_t image,Family family,uintptr_t object,const Render& render)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(image+0x39d670)(object);
        const bool visible=*reinterpret_cast<const uint8_t*>(object+0x368)&&*reinterpret_cast<const uint8_t*>(object+0x808);
        reinterpret_cast<void(*)(uintptr_t,bool)>(image+0x287d0)(render.character,visible);
        reinterpret_cast<void(*)(uintptr_t,float)>(image+0x28970)(render.character,0.0f);
        reinterpret_cast<void(*)(uintptr_t)>(image+0x21df0)(render.actor);
        if(family==Family::Npc&&*reinterpret_cast<const uintptr_t*>(object+0x810))reinterpret_cast<void(*)(uintptr_t)>(image+0x3c27b0)(object);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t compare_pointer(uintptr_t address,uintptr_t expected,uintptr_t value,bool& exchanged)noexcept{
    exchanged=false;
    if(!address||(address&(alignof(uintptr_t)-1)))return ERROR_INVALID_ADDRESS;
    __try{exchanged=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(address),reinterpret_cast<void*>(value),reinterpret_cast<void*>(expected))==reinterpret_cast<void*>(expected);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}

#include "intervention_actor_native.h"
#include <windows.h>

namespace mt2mp::game::interventions::actors::native {
uint32_t maximum(uintptr_t image,uintptr_t object,int32_t attribute,float& out)noexcept{
    __try{out=reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x3a41e0)(object,attribute);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t clip(uintptr_t image,const Render& render,int32_t semantic,Clip& out)noexcept{
    __try{const int32_t index=render.indexes[size_t(semantic)];
        if(index<0){out.length.reset();out.time.reset();out.paused=false;out.speed=0;return 0;}
        const auto length=reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x23a00)(render.actor,index);out.length=std::bit_cast<uint32_t>(length);
        if(reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x28900)(render.character,semantic)){
            out.time=std::bit_cast<uint32_t>(reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x23a30)(render.actor,index));
            out.paused=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x239e0)(render.actor,index);
            out.speed=std::bit_cast<uint32_t>(reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x23a50)(render.actor,index));
        }else{out.time.reset();out.paused=false;out.speed=0;}return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t clip(uintptr_t image,const Render& render,int32_t semantic,const Clip& value)noexcept{
    __try{const int32_t index=render.indexes[size_t(semantic)];
        if(value.time){reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x288d0)(render.character,semantic);
            reinterpret_cast<void(*)(uintptr_t,int32_t,float)>(image+0x23940)(render.actor,index,std::bit_cast<float>(value.speed));
            reinterpret_cast<void(*)(uintptr_t,int32_t,bool)>(image+0x23910)(render.actor,index,value.paused);
            reinterpret_cast<void(*)(uintptr_t,int32_t,float)>(image+0x23980)(render.actor,index,std::bit_cast<float>(*value.time));
        }else reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x288f0)(render.character,semantic);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t on_map(uintptr_t image,Family family,uintptr_t object,bool value)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t,bool)>(image+(family==Family::Npc?0x3c7510:0x39f420))(object,value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t ready(uintptr_t image,uintptr_t object,bool value)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t,bool)>(image+0x3c0710)(object,value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t transform(uintptr_t image,uintptr_t object,const subscriber_motion::Pose& pose)noexcept{
    alignas(4) std::array<unsigned char,0x6c> value{};
    __try{reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(image+0xa09f30)(value.data(),pose.orientation.data(),pose.position.data(),pose.scale.data());
        reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xc733c0)(object+0x30,value.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t spatial(uintptr_t image,uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(image+0x3b6660)(object);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t registered(uintptr_t image,uint64_t uid,uintptr_t& object)noexcept{
    __try{object=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x481f60)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t scene(uintptr_t image,uintptr_t screen,uintptr_t& out)noexcept{
    __try{out=reinterpret_cast<uintptr_t(*)(uintptr_t,int32_t)>(image+0x9d3f60)(screen,0);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t present(uintptr_t image,Family family,uintptr_t object,const Render& render)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(image+0x3a2170)(object);
        const bool visible=*reinterpret_cast<const uint8_t*>(object+0x368)&&*reinterpret_cast<const uint8_t*>(object+0x808);
        reinterpret_cast<void(*)(uintptr_t,bool)>(image+0x28970)(render.character,visible);
        reinterpret_cast<void(*)(uintptr_t,float)>(image+0x28b10)(render.character,0.0f);
        reinterpret_cast<void(*)(uintptr_t)>(image+0x21f80)(render.actor);
        if(family==Family::Npc&&*reinterpret_cast<const uintptr_t*>(object+0x810))reinterpret_cast<void(*)(uintptr_t)>(image+0x3c72c0)(object);
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

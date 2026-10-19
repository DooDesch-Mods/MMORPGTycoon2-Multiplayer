#pragma once
#include "boss_placement_reader.h"
#include <Windows.h>

namespace mt2mp::game::boss_placement_calls {
inline uint32_t purchase(uintptr_t image,const std::array<uint32_t,3>& position,bool& paid){
    paid=false;
    __try{paid=reinterpret_cast<bool(*)(const void*)>(image+0x399ac0)(position.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t region(uintptr_t image,uintptr_t map,const std::array<uint32_t,3>& point,uintptr_t& result){
    result=0;
    __try{result=reinterpret_cast<uintptr_t(*)(void*,const void*,void*)>(image+0x32a990)(reinterpret_cast<void*>(map),point.data(),nullptr);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t add(uintptr_t image,uintptr_t manager,int32_t type,const std::array<unsigned char,0x6c>& pose,uintptr_t& result){
    result=0;
    __try{result=reinterpret_cast<uintptr_t(*)(void*,int32_t,const void*)>(image+0x239410)(reinterpret_cast<void*>(manager),type,pose.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t cancel(uintptr_t image,uintptr_t cursor){
    __try{reinterpret_cast<void(*)(void*,void*)>(image+0x5854e0)(reinterpret_cast<void*>(cursor),nullptr);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
// This is the exact final store in original HandleRelease_Left. It changes
// only the originating local tool after native Cursor deactivation.
inline uint32_t clear(uintptr_t image,uintptr_t receiver){
    __try{
        if(*reinterpret_cast<uintptr_t*>(receiver)!=image+boss_placement_reader::addresspoint)return 0xE0040001;
        *reinterpret_cast<uint8_t*>(receiver+0x3b1)=0;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t seed(uintptr_t owner,const std::array<uint64_t,4>& before,const std::array<uint64_t,4>& target,bool& attempted){
    __try{
        if(*reinterpret_cast<std::array<uint64_t,4>*>(owner)!=before)return 0xE0040002;
        attempted=true;*reinterpret_cast<std::array<uint64_t,4>*>(owner)=target;
        return *reinterpret_cast<std::array<uint64_t,4>*>(owner)==target?0:0xE0040003;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}

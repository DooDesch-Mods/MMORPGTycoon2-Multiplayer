#pragma once
#include "npc_quest_targets.h"
#include <Windows.h>
#include <array>
#include <cstdint>
#include <string>

namespace mt2mp::game::group_object_calls {
inline uint32_t cost(uintptr_t image,uintptr_t object,int32_t& out){
    __try{out=reinterpret_cast<int32_t(*)(const void*)>(image+0x4d4880)(reinterpret_cast<const void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy(uintptr_t image,uintptr_t manager,uintptr_t object){
    __try{reinterpret_cast<void(*)(void*,void*)>(image+0x2d8c10)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
template<class Memory>bool binding(Memory& memory,uintptr_t image,uintptr_t object,std::string& e){
    namespace q=npc_quest_targets;uintptr_t value=0;
    if(!q::typed(memory,image,object,0x1750180,e))return false;
    struct Slot{size_t offset;uintptr_t rva;};
    constexpr Slot slots[]{{0xd0,0xe52bf0},{0x100,0x4d4820},{0x120,0xe4af70},{0x278,0x4d4880},{0x280,0xe4aea0},{0x2a0,0x4d2870}};
    for(const auto& slot:slots)if(!q::at(memory,image+0x1750180,slot.offset,value,e)||value!=image+slot.rva)
        return q::fail(e,e.empty()?"Group original Move/Cost/member notification virtual method differs":e);
    return true;
}
}

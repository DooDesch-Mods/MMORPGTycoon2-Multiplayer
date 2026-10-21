#pragma once
#include "duplicate_calls.h"
#include "selection_move_native.h"
namespace mt2mp::game::dungeon_gizmo_control_calls {
inline uint32_t offline(uintptr_t image,uintptr_t dungeon,bool& allowed){
    __try{allowed=reinterpret_cast<bool(*)(const void*)>(image+0xa29a0)(reinterpret_cast<const void*>(dungeon));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t capability(uintptr_t object,bool duplicate,int32_t& cost,bool& allowed){
    __try{const auto table=*reinterpret_cast<uintptr_t**>(object);
        allowed=reinterpret_cast<bool(*)(void*)>(table[(duplicate?0xd8:0xd0)/8])(reinterpret_cast<void*>(object));
        cost=reinterpret_cast<int32_t(*)(void*)>(table[0x278/8])(reinterpret_cast<void*>(object));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t cost_position(uintptr_t object,std::array<uint32_t,3>& position){
    __try{const auto table=*reinterpret_cast<uintptr_t**>(object);
        reinterpret_cast<void*(*)(void*,const void*)>(table[0x280/8])(position.data(),reinterpret_cast<const void*>(object));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy(uintptr_t image,uintptr_t manager,duplicate_calls::Array& targets){
    __try{reinterpret_cast<void(*)(void*,duplicate_calls::Array*)>(image+0x2d42c0)(reinterpret_cast<void*>(manager),&targets);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}

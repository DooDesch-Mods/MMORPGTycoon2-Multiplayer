#pragma once
#include <Windows.h>
#include <cstdint>

namespace mt2mp::game::dungeon_build_calls {
inline uint32_t finish_tasks(uintptr_t image,uintptr_t manager,bool& entered,bool& returned){
    entered=false;returned=false;
    __try{
        entered=true;
        reinterpret_cast<void(*)(void*)>(image+0x526d50)(reinterpret_cast<void*>(manager));
        returned=true;return 0;
    }__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){
        return GetExceptionCode();
    }
}
}

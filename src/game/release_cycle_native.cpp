#include "release_cycle_native.h"
#include <Windows.h>
#include <intrin.h>
namespace mt2mp::game::release_cycle_native {
uint32_t predicate(uintptr_t image,uintptr_t release,bool& allowed)noexcept{
    __try{allowed=reinterpret_cast<bool(*)(const void*,void*)>(image+0x4def00)(reinterpret_cast<const void*>(release),nullptr);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t phase(uintptr_t image,uintptr_t celebration,release_cycle::Command command)noexcept{
    uintptr_t rva=0;switch(command){case release_cycle::Command::Show:rva=0x4e6cd0;break;case release_cycle::Command::Begin:rva=0x4e6eb0;break;case release_cycle::Command::Abort:rva=0x4e6d40;break;case release_cycle::Command::Skip:rva=0x4e6dc0;break;default:return ERROR_INVALID_PARAMETER;}
    __try{reinterpret_cast<void(*)(void*)>(image+rva)(reinterpret_cast<void*>(celebration));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t show(uintptr_t image,uintptr_t manager)noexcept{__try{reinterpret_cast<void(*)(void*)>(image+0x4eb360)(reinterpret_cast<void*>(manager));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t script(uintptr_t image,Receipt& receipt)noexcept{
    alignas(8) unsigned char record[0x78]{};
    __try{receipt.attempted|=1;reinterpret_cast<void(*)(void*,const char*)>(image+0x967370)(record,"MessageTo Mode LevelUp");receipt.record_constructed=true;receipt.returned|=1;
        receipt.attempted|=2;reinterpret_cast<void(*)(void*)>(image+0x4fbff0)(record);receipt.returned|=2;
    }__except(EXCEPTION_EXECUTE_HANDLER){receipt.exception=GetExceptionCode();}
    if(receipt.record_constructed){__try{receipt.attempted|=4;reinterpret_cast<void(*)(void*)>(image+0x9676a0)(record);receipt.record_destroyed=true;receipt.returned|=4;}__except(EXCEPTION_EXECUTE_HANDLER){if(!receipt.exception)receipt.exception=GetExceptionCode();}}
    return receipt.exception;
}
uint32_t clock_pause(uintptr_t image,uintptr_t clock)noexcept{__try{reinterpret_cast<void(*)(void*,float)>(image+0x259d70)(reinterpret_cast<void*>(clock),0.0f);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t random_cas(uintptr_t target,uint64_t before,uint64_t after,bool& stored)noexcept{__try{stored=uint64_t(InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(target),LONG64(after),LONG64(before)))==before;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
}

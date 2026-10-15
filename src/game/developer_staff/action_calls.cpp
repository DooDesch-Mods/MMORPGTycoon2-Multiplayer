#include "action_calls.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
namespace mt2mp::game::developer_staff::action_runtime {
uint32_t invoke_ui(initial_release::UiCommand command,uintptr_t image,uintptr_t window,bool hire,NativeReceipt* r){
    initial_release::Record record{};const char* label=hire?"Hire":"Fire";uint32_t fault=0;
    __try{
        reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,label);r->constructed=true;
        r->label_confirmed=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,label);
        if(r->label_confirmed){r->attempted=true;r->accepted=command(reinterpret_cast<void*>(window),&record);r->returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){fault=GetExceptionCode();}
    if(r->constructed){r->cleanup_attempted=true;
        __try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);r->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){r->cleanup_exception=GetExceptionCode();}}
    return fault;
}
uint32_t invoke_count(uintptr_t image,uintptr_t lounge,int32_t count){
    __try{reinterpret_cast<void(*)(void*,int32_t)>(image+0x25eed0)(reinterpret_cast<void*>(lounge),count);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
UiIntent classify(uintptr_t image,const void* record,uint32_t& fault)noexcept{
    fault=0;if(!image||!record)return UiIntent::Unreadable;
    __try{const auto equals=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80);
        if(equals(record,"Hire"))return UiIntent::Hire;if(equals(record,"Fire"))return UiIntent::Fire;return UiIntent::Other;}
    __except(EXCEPTION_EXECUTE_HANDLER){fault=GetExceptionCode();return UiIntent::Unreadable;}
}
}

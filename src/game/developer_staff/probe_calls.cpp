#include "probe.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
namespace mt2mp::game::developer_staff {
uint32_t original_show(uintptr_t image,uintptr_t window){
    __try{reinterpret_cast<void(*)(void*,bool,bool)>(image+0x827210)(reinterpret_cast<void*>(window),true,false);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t original_command(Command command,uintptr_t image,uintptr_t window,NativeReceipt* receipt){
    initial_release::Record record{};const char* label=command==Command::Hire?"Hire":"Fire";uint32_t fault=0;
    const initial_release::Api api{reinterpret_cast<initial_release::RecordCtor>(image+0x970590),
        reinterpret_cast<initial_release::RecordDtor>(image+0x9708c0),reinterpret_cast<initial_release::RecordEquals>(image+0x111f720),
        reinterpret_cast<initial_release::UiCommand>(image+0x6cef90)};
    __try{
        api.construct(&record,label);receipt->constructed=true;receipt->label_confirmed=api.equals(&record,label);
        if(receipt->label_confirmed){receipt->attempted=true;receipt->accepted=api.command(reinterpret_cast<void*>(window),&record);receipt->returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){fault=GetExceptionCode();}
    if(receipt->constructed){receipt->cleanup_attempted=true;
        __try{api.destroy(&record);receipt->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){receipt->cleanup_exception=GetExceptionCode();}}
    return fault;
}
}

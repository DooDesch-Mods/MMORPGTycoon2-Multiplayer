#include "manual_fixture.h"
#include <Windows.h>
#include <cstdio>
namespace mt2mp::game::bug_authority::manual_action::fixture {
void original_select(uintptr_t image,uintptr_t window,int32_t id,NativeCall* out)noexcept{
    initial_release::Record record{};char command[48]{};std::snprintf(command,sizeof command,"SelectBug %d",id);
    __try{
        reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,command);out->constructed=true;
        out->record_confirmed=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,command);
        if(out->record_confirmed){out->accepted=reinterpret_cast<initial_release::UiCommand>(image+0x6c7b70)(reinterpret_cast<void*>(window),&record);out->returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){out->fault=GetExceptionCode();}
    if(out->constructed){
        __try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);out->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out->cleanup_fault=GetExceptionCode();}
    }
}
uint32_t original_page(uintptr_t image,uintptr_t tabs)noexcept{
    struct Text {const char* data;uint64_t length;unsigned char storage[16];};
    constexpr char name[]="{dev_tab_bugs}";const Text page{name,sizeof(name)-1,{}};
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x7524f0)(reinterpret_cast<void*>(tabs),&page);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t original_input(uintptr_t image,uintptr_t button,Input input)noexcept{
    const uintptr_t rva=input==Input::Hover?0x66f1f0:input==Input::Press?0x674c40:0x674ed0;
    const float position[2]{};
    // The admitted original branches use the pane receiver only.
    __try{reinterpret_cast<void(*)(void*,void*,const float*)>(image+rva)(reinterpret_cast<void*>(button),nullptr,position);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}

#include "manual_ui.h"
#include <Windows.h>
namespace mt2mp::game::bug_authority::manual_action {
Command classify(uintptr_t image,const void* record,uint32_t& fault)noexcept{
    struct Text {const char* data;size_t size;char local[16];};
    Text text{};std::memcpy(text.local,"BugButton",10);text.data=text.local;text.size=9;fault=0;
    if(!image||!record)return Command::Unreadable;
    __try{return reinterpret_cast<bool(*)(const void*,const void*)>(image+0x96e1d0)(record,&text)?Command::Button:Command::Other;}
    __except(EXCEPTION_EXECUTE_HANDLER){fault=GetExceptionCode();return Command::Unreadable;}
}
}

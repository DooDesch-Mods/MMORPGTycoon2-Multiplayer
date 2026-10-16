#include "bug_policy_calls.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
namespace mt2mp::game::developer_staff::bug_policy {
struct BorrowedString {const char* data;uint64_t length;unsigned char storage[16]{};};
static_assert(sizeof(BorrowedString)==32);
uint32_t original_page(uintptr_t image,uintptr_t tabs){
    constexpr char name[]="{dev_tab_policies}";const BorrowedString page{name,sizeof(name)-1,{}};
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x7524f0)(reinterpret_cast<void*>(tabs),&page);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t original_input(uintptr_t image,uintptr_t checkbox,Input input){
    const uintptr_t rva=input==Input::Hover?0x6a0ac0:input==Input::Press?0x6a0a30:0x6a32a0;
    const float position[2]{};
    // These exact original bodies use only the checkbox receiver, not cursor/position.
    __try{reinterpret_cast<void(*)(void*,void*,const float*)>(image+rva)(reinterpret_cast<void*>(checkbox),nullptr,position);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}

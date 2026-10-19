#include "intervention_native.h"
#include <Windows.h>
#include <cstring>
namespace mt2mp::game::interventions::native {
namespace {struct String {const char* data;size_t length;unsigned char rest[16];};}
uint32_t matches(uintptr_t image,const void* native_string,Command command,bool& out)noexcept{
    return matches_name(image,native_string,original_command(command),out);
}
uint32_t matches_name(uintptr_t image,const void* native_string,const char* name,bool& out)noexcept{
    out=false;if(!image||!native_string||!name)return ERROR_INVALID_PARAMETER;
    __try{out=reinterpret_cast<bool(*)(const void*,const char*)>(image+0x11e3aa0)(native_string,name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
Receipt execute(Original original,uintptr_t actor,Command command)noexcept{
    Receipt out;const auto* name=original_command(command);if(!original||!actor||!name){out.exception=ERROR_INVALID_PARAMETER;return out;}
    const String text{name,std::strlen(name),{}};
    __try{out.attempted=true;out.result=original(reinterpret_cast<void*>(actor),&text);out.returned=true;return out;}__except(EXCEPTION_EXECUTE_HANDLER){out.exception=GetExceptionCode();return out;}
}
}

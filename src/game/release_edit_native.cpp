#include "release_edit_native.h"
#include <Windows.h>
#include <intrin.h>
namespace mt2mp::game::release_edit_native {
uint32_t default_step(uintptr_t image,uintptr_t release,bool announcement,Returned& out,unsigned step)noexcept{
    __try{switch(step){
        case 0:reinterpret_cast<void(*)(void*,const void*)>(image+(announcement?0x4e0b00:0x4e0d40))(out.localized.data(),reinterpret_cast<const void*>(release));break;
        case 1:reinterpret_cast<void(*)(void*,const void*)>(image+0xa18e60)(out.text.data(),out.localized.data());break;
        case 2:reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(out.text.data());break;
        case 3:reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(out.localized.data()+32);break;
        case 4:reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(out.localized.data());break;
        default:return ERROR_INVALID_PARAMETER;
    }return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t set_type(uintptr_t image,uintptr_t release,const std::array<int32_t,3>& version,int32_t type,uint32_t& returned)noexcept{
    alignas(8) unsigned char native_version[40]{};
    __try{reinterpret_cast<void(*)(void*,int32_t,int32_t,int32_t)>(image+0x525690)(native_version,version[0],version[1],version[2]);returned|=1;
        reinterpret_cast<void(*)(void*,const void*,int32_t)>(image+0x4de390)(reinterpret_cast<void*>(release),native_version,type);returned|=2;
        reinterpret_cast<void(*)(void*)>(image+0x89da30)(native_version);returned|=4;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t assign(uintptr_t image,uintptr_t target,const String& value)noexcept{
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x11bfce0)(reinterpret_cast<void*>(target),&value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t custom(uintptr_t release,bool before,bool after)noexcept{
    __try{return _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(release+0x100),char(after),char(before))==char(before)?0:ERROR_INVALID_DATA;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t reset(uintptr_t image,uintptr_t release,bool announcement)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(image+(announcement?0xa980b0:0xa97f50))(reinterpret_cast<void*>(release));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t query_reset(uintptr_t image,uintptr_t game,const String& key,const String& extra,const String& command)noexcept{
    __try{reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0x465480)(reinterpret_cast<void*>(game),&key,&extra,&command);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t refresh(uintptr_t image,uintptr_t window)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(image+0x79b6c0)(reinterpret_cast<void*>(window));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t pane_set(uintptr_t image,uintptr_t pane,const String& value)noexcept{
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x7f1630)(reinterpret_cast<void*>(pane),&value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}

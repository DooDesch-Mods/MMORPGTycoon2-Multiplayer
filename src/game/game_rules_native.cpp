#include "game_rules_native.h"
#include <Windows.h>
#include <intrin.h>
#include <cstring>
namespace mt2mp::game::game_rules::native {
namespace {struct String{const char* data;size_t length;unsigned char rest[16];};}
uint32_t find(uintptr_t image,uintptr_t tree,const std::string& key,uintptr_t& out)noexcept{const String name{key.data(),key.size(),{}};
    __try{out=uintptr_t(reinterpret_cast<void*(*)(void*,const void*)>(image+0x2f14e0)(reinterpret_cast<void*>(tree),&name));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t parse(uintptr_t image,const std::string& text,int32_t& out,bool& parsed)noexcept{const String value{text.data(),text.size(),{}};
    __try{parsed=reinterpret_cast<bool(*)(int32_t*,const void*)>(image+0x89fbd0)(&out,&value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t command(uintptr_t image,const void* record,const char* name,bool& matches)noexcept{
    __try{matches=reinterpret_cast<bool(*)(const void*,const char*)>(image+0x1113d80)(record,name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t buttons(uintptr_t image,void* view,bool enabled)noexcept{
    __try{reinterpret_cast<void(*)(void*,bool)>(image+0x8267d0)(view,enabled);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
Receipt change(uintptr_t image,const Pin& pin,const Intent& intent)noexcept{
    Receipt out;if(!valid(intent)||!pin.design||!pin.features){out.exception=ERROR_INVALID_PARAMETER;return out;}
    __try{switch(intent.command){
    case Command::ApplyXP:
        for(size_t i=0;i<14;++i){out.attempted=true;if(uint32_t(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(pin.design+0x188+i*4),LONG(intent.after.xp[i]),LONG(intent.before.xp[i])))!=intent.before.xp[i]){out.exception=ERROR_INVALID_DATA;return out;}++out.stores;}break;
    case Command::RespawnSeconds:{out.attempted=true;if(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(pin.design+0x1d8),intent.after.respawn_seconds,intent.before.respawn_seconds)!=intent.before.respawn_seconds){out.exception=ERROR_INVALID_DATA;return out;}++out.stores;
        const String name{"seconds",7,{}};reinterpret_cast<void(*)(void*,const void*)>(image+0xb63030)(reinterpret_cast<void*>(pin.design+0x1c0),&name);break;}
    case Command::RespawnDelay:{out.attempted=true;if(uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(pin.design+0x1dc),char(intent.after.respawn_delay),char(intent.before.respawn_delay)))!=uint8_t(intent.before.respawn_delay)){out.exception=ERROR_INVALID_DATA;return out;}++out.stores;
        const String name{"delayForToons",13,{}};reinterpret_cast<void(*)(void*,const void*)>(image+0xb63030)(reinterpret_cast<void*>(pin.design+0x1c0),&name);break;}
    case Command::Features:
        for(int32_t i=0;i<2;++i)if(intent.before.available[size_t(i)]){out.attempted=true;reinterpret_cast<void(*)(void*,int32_t,bool)>(image+0x46fb70)(reinterpret_cast<void*>(pin.features),i,intent.after.features[size_t(i)]!=0);out.feature_calls|=uint8_t(1u<<i);if(!intent.before.features[size_t(i)])++out.feature_events;}break;
    }out.returned=true;return out;}__except(EXCEPTION_EXECUTE_HANDLER){out.exception=GetExceptionCode();return out;}
}
}

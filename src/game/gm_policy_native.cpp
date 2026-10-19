#include "gm_policy_native.h"
#include <Windows.h>
#include <intrin.h>
#include <cstring>
namespace mt2mp::game::gm_policy::native {
Receipt change(uintptr_t image,uintptr_t policy,const Intent& intent)noexcept{
    Receipt out;const auto field=size_t(intent.field);if(!valid(intent)||!policy){out.exception=ERROR_INVALID_PARAMETER;return out;}
    // The native bound controls store this exact policy member, then notify its
    // owner. UI pointers are local presentation and are never replayed remotely.
    struct String {const char* data;size_t length;unsigned char rest[16];} name{names[field],std::strlen(names[field]),{}};
    __try{const auto address=policy+offsets[field];
        if(integer(intent.field))out.stored=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),intent.value,intent.before)==intent.before;
        else out.stored=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(intent.value),char(intent.before)))==uint8_t(intent.before);
        if(!out.stored)return out;
        reinterpret_cast<void(*)(void*,const void*)>(image+0x3af510)(reinterpret_cast<void*>(policy),&name);out.notified=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out.exception=GetExceptionCode();}return out;
}
}

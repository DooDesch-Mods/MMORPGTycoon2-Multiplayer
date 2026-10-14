#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "live_host_state/memory_query_timing.h"
#include <cstdint>
#include <string>
namespace mt2mp::game::live_host_state::day {
inline DWORD pointer_store_seh(uintptr_t slot,uintptr_t before,uintptr_t target,uintptr_t* actual)noexcept {
    __try {*actual=uintptr_t(InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(slot),reinterpret_cast<void*>(target),reinterpret_cast<void*>(before)));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
// The caller proves the owning History and exact native slot before publication.
inline bool compare_owned_pointer(uintptr_t slot,uintptr_t before,uintptr_t after,std::string& e){
    static_assert(sizeof(uintptr_t)==8);
    if(!slot||(slot&7)||slot>UINTPTR_MAX-sizeof(uintptr_t)){e="Day actual pointer slot alignment/extent differs";return false;}
    MEMORY_BASIC_INFORMATION page{};
    if(mt2mp::game::live_host_state::apply_timing::virtual_query(reinterpret_cast<void*>(slot),&page,sizeof page)!=sizeof page||page.State!=MEM_COMMIT||(page.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(page.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))||uintptr_t(page.BaseAddress)>UINTPTR_MAX-page.RegionSize||slot+sizeof(uintptr_t)>uintptr_t(page.BaseAddress)+page.RegionSize){e="Day actual pointer CAS requires committed writable whole slot";return false;}
    uintptr_t actual=0;const auto code=pointer_store_seh(slot,before,after,&actual);
    if(code){e="Day pointer CAS SEH="+std::to_string(code);return false;}
    if(actual!=before){e="Day exact expected pointer owner differs";return false;}e.clear();return true;
}
}

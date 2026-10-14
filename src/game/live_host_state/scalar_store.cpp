#include "scalar_store.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "live_host_state/memory_query_timing.h"
namespace mt2mp::game::live_host_state {
namespace {
bool fail(std::string& e,const std::string& s){e=s;return false;}
DWORD compare_store(uintptr_t p,int32_t before,int32_t target,LONG* observed)noexcept{__try{*observed=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(p),target,before);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
}
bool compare_fixed_i32(uintptr_t p,int32_t before,int32_t target,std::string& e){MEMORY_BASIC_INFORMATION page{};if(!p||(p&3)||mt2mp::game::live_host_state::apply_timing::virtual_query(reinterpret_cast<const void*>(p),&page,sizeof page)!=sizeof page)return fail(e,"live scalar page query failed Win32="+std::to_string(GetLastError()));const uintptr_t start=uintptr_t(page.BaseAddress);const DWORD access=page.Protect&0xff;if(page.State!=MEM_COMMIT||(page.Protect&PAGE_GUARD)||(access!=PAGE_READWRITE&&access!=PAGE_WRITECOPY&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY)||page.RegionSize<4||p<start||p-start>page.RegionSize-4)return fail(e,"live scalar fixed slot page not writable");LONG observed=0;const auto code=compare_store(p,before,target,&observed);if(code)return fail(e,"live scalar CAS SEH="+std::to_string(code));if(observed!=before)return fail(e,"live scalar before differs measured="+std::to_string(observed)+" required="+std::to_string(before));e.clear();return true;}
}

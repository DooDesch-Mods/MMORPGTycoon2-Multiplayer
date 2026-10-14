#include "hud_time_reader_process.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
namespace mt2mp::game::hud_time {
namespace {
uint32_t thread_id(void*) { return GetCurrentThreadId(); }
DWORD copy_bytes(uintptr_t address,void* out,size_t count) {
    __try { std::memcpy(out,reinterpret_cast<const void*>(address),count);return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool read_bytes(void*,uintptr_t address,void* out,size_t count,std::string& error) {
    if(!address||!out||!count||count>4096||count>UINTPTR_MAX-address) {
        error="hud_process_read_invalid_range";return false;
    }
    for(uintptr_t cursor=address;cursor<address+count;) {
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<const void*>(cursor),&region,sizeof(region))) {
            error="hud_process_virtual_query_error="+std::to_string(GetLastError());return false;
        }
        const DWORD access=region.Protect&0xff;
        if(region.State!=MEM_COMMIT || (region.Protect&PAGE_GUARD) ||
           (access!=PAGE_READONLY&&access!=PAGE_READWRITE&&access!=PAGE_WRITECOPY&&
            access!=PAGE_EXECUTE_READ&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY)) {
            error="hud_process_unreadable_page";return false;
        }
        const uintptr_t start=reinterpret_cast<uintptr_t>(region.BaseAddress);
        if(region.RegionSize>UINTPTR_MAX-start||start+region.RegionSize<=cursor) {
            error="hud_process_region_overflow";return false;
        }
        cursor=start+region.RegionSize;
    }
    const DWORD code=copy_bytes(address,out,count);
    if(code){error="hud_process_read_exception="+std::to_string(code);return false;}
    return true;
}
}
Services process_read_services(){return {nullptr,thread_id,read_bytes};}
}

#pragma once
#include "placement_ui_probe/ui_probe.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "live_host_state/memory_query_timing.h"
#include <algorithm>
namespace mt2mp::game::live_host_state {
// Keep each existing guarded copy inside one VirtualQuery region while allowing
// the known Record/Toon array to span adjacent readable regions.
inline bool read_native_block(placement_ui_probe::Memory& memory,uintptr_t address,void* output,size_t bytes,std::string& error){
    if(!address||!output||!bytes||address>UINTPTR_MAX-(bytes-1)){error="live native bulk address/extent invalid";return false;}
    size_t offset=0;
    while(offset<bytes){
        MEMORY_BASIC_INFORMATION page{};const auto cursor=address+offset;
        if(mt2mp::game::live_host_state::apply_timing::virtual_query(reinterpret_cast<const void*>(cursor),&page,sizeof page)!=sizeof page){error="live native bulk page query failed Win32="+std::to_string(GetLastError());return false;}
        const auto start=uintptr_t(page.BaseAddress);
        if(start>cursor||cursor-start>=page.RegionSize){error="live native bulk page extent invalid";return false;}
        const auto count=std::min(bytes-offset,size_t(page.RegionSize-(cursor-start)));
        if(!memory.copy(cursor,static_cast<unsigned char*>(output)+offset,count,error))return false;
        offset+=count;
    }
    error.clear();return true;
}
}

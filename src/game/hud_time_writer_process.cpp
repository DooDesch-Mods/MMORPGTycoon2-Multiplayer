#include "hud_time_writer_process.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace mt2mp::game::hud_time {
namespace {
DWORD store(uintptr_t address,int32_t value){
    __try {*reinterpret_cast<int32_t*>(address)=value;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD original_clock(OriginalClockUpdate original,void* receiver,float dt){
    __try {original(receiver,dt);return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool writable(uintptr_t address,std::string& error){
    if(!address||(address&3)||address>UINTPTR_MAX-sizeof(int32_t)){error="hud_writer_store_address_invalid";return false;}
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))){error="hud_writer_store_virtual_query_error="+std::to_string(GetLastError());return false;}
    const uintptr_t start=reinterpret_cast<uintptr_t>(region.BaseAddress);
    const DWORD access=region.Protect&0xff;
    if(region.RegionSize>UINTPTR_MAX-start||start+region.RegionSize<address+4||region.State!=MEM_COMMIT||
       (region.Protect&PAGE_GUARD)||(access!=PAGE_READWRITE&&access!=PAGE_WRITECOPY&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY)){
        error="hud_writer_store_page_not_writable";return false;
    }
    return true;
}
}
bool process_store_item_state(void*,uintptr_t address,int32_t value,std::string& error){
    if(value!=0&&value!=1){error="hud_writer_store_state_invalid";return false;}
    if(!writable(address,error))return false;
    const DWORD code=store(address,value);
    if(code){error="hud_writer_store_seh="+std::to_string(code);return false;}return true;
}
bool process_call_clock(void*,OriginalClockUpdate original,void* receiver,float dt,std::string& error){
    if(!original){error="hud_writer_original_clock_null";return false;}
    const DWORD code=original_clock(original,receiver,dt);
    if(code){error="hud_writer_original_clock_seh="+std::to_string(code);return false;}return true;
}
}

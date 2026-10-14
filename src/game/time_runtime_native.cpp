#include "time_runtime.h"
#include "mt2mp/game/native_engine.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace mt2mp::game::time_runtime {
namespace {
#ifdef _WIN32
// C++ exceptions continue to the adapter's normal catch. SEH keeps its code.
int seh_filter(unsigned long code){return code==0xe06d7363UL?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER;}
unsigned long clock_read_seh(native::ClockState* out,std::string* e,bool* okay){
    __try {*okay=native::read_clock(*out,*e);return 0;}
    __except(seh_filter(GetExceptionCode())){return GetExceptionCode();}
}
unsigned long clock_set_seh(float target,std::string* e,bool* okay){
    __try {*okay=native::set_clock(target==0,target==0?1:target,*e);return 0;}
    __except(seh_filter(GetExceptionCode())){return GetExceptionCode();}
}
unsigned long software_read_seh(native::SoftwareState* out,std::string* e,bool* okay){
    __try {*okay=native::read_software_state(*out,*e);return 0;}
    __except(seh_filter(GetExceptionCode())){return GetExceptionCode();}
}
bool read_clock(void*,wire::Clock& out,std::string& e){
    native::ClockState c{};bool okay=false;const auto code=clock_read_seh(&c,&e,&okay);
    if(code){e="native_read_clock_seh="+std::to_string(code);return false;}
    if(!okay)return false;out={c.time_scale,c.pause_scale};return true;
}
bool set_clock(void*,float target,std::string& e){
    bool okay=false;const auto code=clock_set_seh(target,&e,&okay);
    if(code){e="native_set_clock_seh="+std::to_string(code);return false;}return okay;
}
bool epoch(void*,uint64_t& out,std::string& e){
    native::SoftwareState s{};bool okay=false;const auto code=software_read_seh(&s,&e,&okay);
    if(code){e="native_software_read_seh="+std::to_string(code);return false;}
    if(!okay)return false;if(!s.replica_bound){e="software_replica_not_bound";return false;}out=s.bound_epoch;return true;
}
#endif
}
bool production_backend(const ui::Config& c,Native& n,std::string& e){
#ifdef _WIN32
    if(!native::ready()){e="native_resolver_not_ready";return false;}
    if(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))!=c.image||
       reinterpret_cast<uintptr_t>(native::entrypoint(native::EntryPoint::GameUpdate))!=c.image+0x463ce0){e="native_image_receipt_mismatch";return false;}
    if(GetCurrentThreadId()!=c.game_thread){e="wrong_game_thread";return false;}
    n={nullptr,ui::process_backend(nullptr,nullptr),read_clock,set_clock,epoch};return true;
#else
    (void)c;(void)n;e="windows_native_adapter_required";return false;
#endif
}
}

#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "../../src/core/countdown_owner.h"

using byte=unsigned char;
std::array<byte,0x400> mode_bytes{};std::array<byte,0x600> window_bytes{};std::array<byte,0x300> table_bytes{};
std::atomic<uintptr_t> image{};uintptr_t mode_slot=1;int calls=0;bool pin_ok=true;
template<class T> void put(void* base,size_t offset,T value){std::memcpy(static_cast<byte*>(base)+offset,&value,sizeof(value));}
template<class T> bool read_at(void* base,size_t offset,T& value){if(!base)return false;std::memcpy(&value,static_cast<byte*>(base)+offset,sizeof(value));return true;}
bool check_thread(std::string&){return true;} bool in_game(){return true;} void* singleton(uintptr_t){return mode_bytes.data();}
bool readable(const void*,size_t){return true;} bool has_vtable(void*,uintptr_t){return true;}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
namespace hooks::framework {bool verify_original_hash(uintptr_t,void*,size_t,const char*,std::string& error){if(!pin_ok)error="pin rejected";return pin_ok;}}
void original_show(void* window,bool show,bool force){
    if(!show||force)throw std::runtime_error("Show arguments differ");
    ++calls;put<byte>(window,0x45f,1);put<byte>(window,0x4ec,0);put<int>(window,0x4e8,0);put<float>(window,0x4e4,20.0f);
}
#include "ordered_periodic_countdown_body.inc"
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
int main()try{
    MT2FW_MT2_StepReceipt step{};step.phase=MT2FW_MT2_STEP_ENTERED;step.callbacks_inflight=1;step.original_attempted=1;
    step.invocation=1;step.lease=1;step.session=1;step.world_epoch=1;
    need(mt2mp::core::countdown_owner(step),"actual entered Client owner rejected");
    step.workers_held=1;need(!mt2mp::core::countdown_owner(step),"entered owner with held workers admitted");step.workers_held=0;
    step.original_returned=1;need(!mt2mp::core::countdown_owner(step),"returned original invocation admitted");
    const auto module=reinterpret_cast<uintptr_t>(&original_show)-0x81ec20;image=module;
    put<void*>(mode_bytes.data(),0x90,window_bytes.data());put<int>(mode_bytes.data(),0x30c,0);
    put<uintptr_t>(window_bytes.data(),0,reinterpret_cast<uintptr_t>(table_bytes.data()));
    put<uintptr_t>(table_bytes.data(),0x1d0,module+0x81ec20);put<uintptr_t>(table_bytes.data(),0xb0,module+0x65fa60);
    put<uintptr_t>(table_bytes.data(),0xd8,module+0x65fc80);
    std::string error;pin_ok=false;need(!request_original_periodic_countdown(error)&&calls==0,"invalid pin called original Show");
    pin_ok=true;need(request_original_periodic_countdown(error)&&calls==1,"original countdown Show did not run once");
    need(!request_original_periodic_countdown(error)&&calls==1,"visible countdown replayed original Show");
    std::cout<<"actual periodic countdown checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}

#include "notice_hook.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>
#include "mt2mp/hooks/minhook_backend.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

namespace mt2mp::game::marketing_authority::notice_hook {
namespace {
constexpr uintptr_t rva=0x518290;constexpr size_t extent=5968;
constexpr const char* sha="7ac283fbadada906dd43eb1b8ea92646e3bfecafa06f192d41953b0a450f663b";
uintptr_t image=0;Original original=nullptr;Observer* observer=nullptr;
std::atomic<uint32_t> thread=0;std::atomic<bool> wrong_thread=false;
bool attempted=false,ready=false;std::string installation_error;std::mutex installation;
std::array<unsigned char,16> installed{};std::array<unsigned char,64> relocated{};
std::vector<unsigned char> pristine;
placement_ui_probe::ProcessMemory memory;
bool fail(std::string& e,const std::string& cause){e="Campaign IsActive hook "+cause;return false;}
bool dispatch(uintptr_t receiver){
    const auto owner_thread=thread.load(std::memory_order_acquire);
    if(!owner_thread)return original(receiver);
    if(owner_thread!=GetCurrentThreadId()){wrong_thread.store(true,std::memory_order_release);return original(receiver);}
    return observer->observe(receiver,original);
}
}
bool verify(uintptr_t base,std::string& e){
    std::vector<unsigned char> body(extent);if(!memory.copy(base+rva,body.data(),body.size(),e))return false;
    if(!attempted)return mt2mp::sha256(body)==sha||fail(e,"original whole body SHA differs");
    if(!ready||image!=base||!original)return fail(e,installation_error.empty()?"installation unconfirmed":installation_error);
    std::array<unsigned char,64> trampoline{};
    if(!memory.copy(reinterpret_cast<uintptr_t>(original),trampoline.data(),trampoline.size(),e))return false;
    if(!std::equal(installed.begin(),installed.end(),body.begin())||trampoline!=relocated||body[0]!=0xe9||
       !std::equal(body.begin()+5,body.end(),pristine.begin()+5))return fail(e,"owned entry/trampoline/body tail differs");
    int32_t displacement=0;std::memcpy(&displacement,body.data()+1,4);
    const auto relay=base+rva+5+static_cast<intptr_t>(displacement);std::array<unsigned char,14> jump{};
    if(!memory.copy(relay,jump.data(),jump.size(),e))return false;
    uintptr_t target=0;std::memcpy(&target,jump.data()+6,sizeof target);
    return (jump[0]==0xff&&jump[1]==0x25&&jump[2]==0&&jump[3]==0&&jump[4]==0&&jump[5]==0&&target==reinterpret_cast<uintptr_t>(&dispatch))||fail(e,"owned relay differs");
}
bool initialize(uintptr_t base,std::string& e){
    std::lock_guard lock(installation);
    if(attempted)return verify(base,e);
    if(!base||!verify(base,e))return false;
    attempted=true;image=base;
    const auto reject=[&](const std::string& cause){installation_error=cause;return fail(e,cause);};
    pristine.resize(extent);if(!memory.copy(base+rva,pristine.data(),pristine.size(),e))return reject(e);
    if(mt2mp::sha256(pristine)!=sha)return reject("original body changed before installation");
    const auto target=reinterpret_cast<void*>(base+rva);
    auto result=MH_CreateHook(target,reinterpret_cast<void*>(&dispatch),reinterpret_cast<void**>(&original));
    if(result!=MH_OK)return reject("MH_CreateHook status="+std::to_string(result));
    if(!memory.copy(reinterpret_cast<uintptr_t>(original),relocated.data(),relocated.size(),e))return reject(e);
    result=MH_EnableHook(target);if(result!=MH_OK)return reject("MH_EnableHook status="+std::to_string(result));
    if(!memory.copy(base+rva,installed.data(),installed.size(),e))return reject(e);
    ready=true;if(!verify(base,e)){ready=false;return reject(e);}return true;
}
bool arm(Observer* value,uint32_t owner_thread,std::string& e){
    if(!ready||!value||!owner_thread||owner_thread!=GetCurrentThreadId()||thread.load(std::memory_order_acquire))return fail(e,"arm requires idle current Mainthread");
    observer=value;wrong_thread.store(false,std::memory_order_release);thread.store(owner_thread,std::memory_order_release);e.clear();return true;
}
void disarm(Observer* value){if(observer==value){thread.store(0,std::memory_order_release);observer=nullptr;}}
bool healthy(std::string& e){return !wrong_thread.load(std::memory_order_acquire)||fail(e,"original IsActive observed off bound Mainthread");}
}

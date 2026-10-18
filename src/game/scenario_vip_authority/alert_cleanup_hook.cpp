#include "alert_cleanup_invoke.h"
#include "../placement_ui_probe/ui_probe.h"
#include "../live_host_state/memory_query_timing.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>
#include "mt2mp/hooks/minhook_backend.h"
#include <intrin.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace mt2mp::game::scenario_vip_authority::client_alert::cleanup_hook {
namespace {
using Original=void(*)(uintptr_t);
uintptr_t image=0;
Original original_deinit=nullptr;
Callbacks callbacks;
placement_ui_probe::ProcessMemory memory;
std::mutex installation;
std::array<unsigned char,16> installed{};
std::array<unsigned char,64> relocated{};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
void deinit(uintptr_t mode){
    live_host_state::apply_timing::PageReadScope boundary(live_host_state::apply_timing::PageBoundary::OriginalCall);
    invoke(original_deinit,callbacks,mode);
}
}
bool verify(std::string& error){
    if(!image||!original_deinit)return fail(error,"VIP owned entry missing");
    std::array<unsigned char,16> entry{};std::array<unsigned char,64> trampoline{};
    if(!memory.copy(image+0x4a0ba0,entry.data(),entry.size(),error)||
       !memory.copy(reinterpret_cast<uintptr_t>(original_deinit),trampoline.data(),trampoline.size(),error))return false;
    if(entry!=installed||trampoline!=relocated||entry[0]!=0xe9)return fail(error,"VIP owned entry/trampoline changed");
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,4);
    const auto relay=image+0x4a0ba0+5+static_cast<intptr_t>(displacement);
    std::array<unsigned char,14> jump{};if(!memory.copy(relay,jump.data(),jump.size(),error))return false;
    uintptr_t target=0;std::memcpy(&target,jump.data()+6,sizeof target);
    return (jump[0]==0xff&&jump[1]==0x25&&jump[2]==0&&jump[3]==0&&jump[4]==0&&jump[5]==0&&
        target==reinterpret_cast<uintptr_t>(&deinit))||fail(error,"VIP owned relay target differs");
}
bool initialize(uintptr_t base,const Callbacks& provider,std::string& error){
    std::lock_guard lock(installation);
    if(image)return fail(error,"VIP installation already attempted");
    if(!base||!provider.before||!provider.after)return fail(error,"VIP Deinit image/before/after provider missing");
    image=base;callbacks=provider;
    std::vector<unsigned char> before(1472);
    if(!memory.copy(image+0x4a0ba0,before.data(),before.size(),error))return false;
    if(mt2mp::sha256(before)!="fa38d9f2457813b78a8058d1df618f32755bd19c90c1831e6992f2ab3bcf7967")
        return fail(error,"VIP original Deinit whole body differs");
    const auto target=reinterpret_cast<void*>(image+0x4a0ba0);
    auto status=MH_CreateHook(target,reinterpret_cast<void*>(&deinit),reinterpret_cast<void**>(&original_deinit));
    if(status!=MH_OK)return fail(error,"VIP MH_CreateHook status="+std::to_string(status));
    if(!memory.copy(reinterpret_cast<uintptr_t>(original_deinit),relocated.data(),relocated.size(),error))return false;
    status=MH_EnableHook(target);if(status!=MH_OK)return fail(error,"VIP MH_EnableHook status="+std::to_string(status));
    std::vector<unsigned char> after(before.size());
    if(!memory.copy(image+0x4a0ba0,after.data(),after.size(),error))return false;
    std::copy_n(after.begin(),installed.size(),installed.begin());
    if(!std::equal(after.begin()+5,after.end(),before.begin()+5))return fail(error,"VIP hook changed original body tail");
    if(!verify(error))return false;
    return true;
}
}

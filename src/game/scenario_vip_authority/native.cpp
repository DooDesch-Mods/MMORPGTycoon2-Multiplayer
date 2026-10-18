#include "native.h"
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

namespace mt2mp::game::scenario_vip_authority {
namespace {
using Update=void(*)(uintptr_t,float);
uintptr_t image=0;
Update original_update=nullptr;
Callbacks callbacks;
placement_ui_probe::ProcessMemory memory;
std::atomic<bool> ready=false;
std::mutex installation;
std::array<unsigned char,16> installed{};
std::array<unsigned char,64> relocated{};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
void dispatch(uintptr_t receiver,float dt,uintptr_t caller){
    if(!callbacks.client_active||!callbacks.client_active()){
        if(callbacks.host_update)callbacks.host_update(receiver,dt,caller,original_update);
        else original_update(receiver,dt);
        return;
    }
    Receipt receipt;receipt.owned=true;receipt.receiver=receiver;
    try{
        live_host_state::apply_timing::PageReadScope page_reads;
        if(!callbacks.owner(receiver,receipt,receipt.cause)){
            if(receipt.cause.empty())receipt.cause="VIP Client owner rejected without cause";
        }else if(receipt.owned){
            if(caller!=image+0x4f43f5)receipt.cause="VIP owned Update has a different original ScenarioLog caller";
            else if(!ready.load(std::memory_order_acquire))receipt.cause="VIP owned hook is not ready";
            else if(verify_hook(receipt.cause))receipt.suppressed=true;
        }
    }catch(const std::exception& exception){receipt.cause=std::string("VIP Client owner exception: ")+exception.what();}
    catch(...){receipt.cause="VIP Client owner nonstandard exception";}
    if(!receipt.owned){original_update(receiver,dt);return;}
    callbacks.publish(receipt);
}
void update(uintptr_t receiver,float dt){dispatch(receiver,dt,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
}
bool verify_hook(std::string& error){
    if(!image||!original_update)return fail(error,"VIP owned entry missing");
    std::array<unsigned char,16> entry{};std::array<unsigned char,64> trampoline{};
    if(!memory.copy(image+0x4f85b0,entry.data(),entry.size(),error)||
       !memory.copy(reinterpret_cast<uintptr_t>(original_update),trampoline.data(),trampoline.size(),error))return false;
    if(entry!=installed||trampoline!=relocated||entry[0]!=0xe9)return fail(error,"VIP owned entry/trampoline changed");
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,4);
    const auto relay=image+0x4f85b0+5+static_cast<intptr_t>(displacement);
    std::array<unsigned char,14> jump{};if(!memory.copy(relay,jump.data(),jump.size(),error))return false;
    uintptr_t target=0;std::memcpy(&target,jump.data()+6,sizeof target);
    return (jump[0]==0xff&&jump[1]==0x25&&jump[2]==0&&jump[3]==0&&jump[4]==0&&jump[5]==0&&
        target==reinterpret_cast<uintptr_t>(&update))||fail(error,"VIP owned relay target differs");
}
bool initialize(uintptr_t base,const Callbacks& provider,std::string& error){
    std::lock_guard lock(installation);
    if(image)return fail(error,"VIP installation already attempted");
    if(!base||!provider.owner||!provider.publish||!provider.client_active)return fail(error,"VIP image/owner/publish provider missing");
    image=base;callbacks=provider;
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    constexpr Body callers[]={
        {0x4f4360,1600,"3518cf516a2bda5a4ce50f5f8ba1bdc24f0925588cdea90221a696804acd26b2"},
        {0x4f7080,208,"20587754148c0fd249e2496040fcfc8c550a3eec39e150fea4bcb5a4dd71a9e8"}};
    for(const auto& body:callers){std::vector<unsigned char> bytes(body.size);
        if(!memory.copy(image+body.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=body.sha)return fail(error,"VIP original owner body differs RVA="+std::to_string(body.rva));}
    std::vector<unsigned char> before(640);
    if(!memory.copy(image+0x4f85b0,before.data(),before.size(),error))return false;
    if(mt2mp::sha256(before)!="cb443c522e00a89018c602fbc620409eb2904918180199a938057da0ce032542")
        return fail(error,"VIP original Update whole body differs");
    const auto target=reinterpret_cast<void*>(image+0x4f85b0);
    auto status=MH_CreateHook(target,reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original_update));
    if(status!=MH_OK)return fail(error,"VIP MH_CreateHook status="+std::to_string(status));
    if(!memory.copy(reinterpret_cast<uintptr_t>(original_update),relocated.data(),relocated.size(),error))return false;
    status=MH_EnableHook(target);if(status!=MH_OK)return fail(error,"VIP MH_EnableHook status="+std::to_string(status));
    std::vector<unsigned char> after(before.size());
    if(!memory.copy(image+0x4f85b0,after.data(),after.size(),error))return false;
    std::copy_n(after.begin(),installed.size(),installed.begin());
    if(!std::equal(after.begin()+5,after.end(),before.begin()+5))return fail(error,"VIP hook changed original body tail");
    if(!verify_hook(error))return false;
    ready.store(true,std::memory_order_release);return true;
}
}

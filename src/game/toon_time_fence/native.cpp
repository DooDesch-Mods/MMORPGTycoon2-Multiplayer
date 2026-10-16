#include "native.h"
#include "../placement_ui_probe/ui_probe.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <intrin.h>
#include "../live_host_state/memory_query_timing.h"

namespace mt2mp::game::toon_time_fence {
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
thread_local bool entered=false;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
uint32_t clear_call(uintptr_t toon,uint32_t expected,bool* exchanged){
    __try{*exchanged=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(toon+0xad4),0,std::bit_cast<LONG>(expected))==std::bit_cast<LONG>(expected);}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
uint32_t update_call(uintptr_t toon,float dt){
    __try{original_update(toon,dt);}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
class Native {
    uintptr_t caller_=0;
    bool physical_verified_=false;
public:
    using Owner=toon_time_fence::Owner;
    explicit Native(uintptr_t caller):caller_(caller){}
    bool pin(uintptr_t toon,Owner& owner,bool& owned,std::string& error){
        if(!callbacks.owner)return fail(error,"Toon time owner provider missing");
        if(!callbacks.owner(toon,owner,owned,error))return false;
        if(!owned)return true;
        if(owner.toon!=toon||!owner.subscriber||owner.subscriber_id<0||owner.context.role!=action_session::Role::Client)
            return fail(error,"Toon time owned receiver/Subscriber/role differs");
        if(caller_!=image+0x2ffea1&&caller_!=image+0x300064)
            return fail(error,"Toon time owned entry has a different original ToonManager caller");
        if(!ready.load(std::memory_order_acquire))return fail(error,"Toon time owned hook is not ready");
        if(!physical_verified_){if(!verify_hook(error))return false;physical_verified_=true;}
        return true;
    }
    bool read_backlog(uintptr_t toon,uint32_t& out,std::string& error){
        if(!toon||toon>UINTPTR_MAX-0xad8)return fail(error,"Toon time receiver extent invalid");
        return memory.copy(toon+0xad4,&out,sizeof out,error);
    }
    bool clear_backlog(uintptr_t toon,uint32_t expected,std::string& error){
        bool exchanged=false;const auto exception=clear_call(toon,expected,&exchanged);
        if(exception)return fail(error,"Toon time cache CAS exception="+std::to_string(exception));
        return exchanged||fail(error,"Toon time prior culling cache changed before CAS");
    }
    bool original(uintptr_t toon,float dt,std::string& error){
        const auto exception=update_call(toon,dt);
        return !exception||fail(error,"Toon original zero-time entry exception="+std::to_string(exception));
    }
};
void update(uintptr_t toon,float dt){
    if(!callbacks.client_active||!callbacks.client_active()){original_update(toon,dt);return;}
    live_host_state::apply_timing::PageReadScope page_reads;
    Native calls(reinterpret_cast<uintptr_t>(_ReturnAddress()));Receipt receipt;
    if(entered){
        Owner owner;std::string error;
        try{if(!calls.pin(toon,owner,receipt.owned,error))receipt.cause=error;}
        catch(const std::exception& exception){receipt.cause=std::string("Toon nested owner exception: ")+exception.what();}
        catch(...){receipt.cause="Toon nested owner nonstandard exception";}
        if(!receipt.owned){original_update(toon,dt);return;}
        if(receipt.cause.empty())receipt.cause="Toon time nested owned entry rejected";
        if(callbacks.publish)callbacks.publish(toon,receipt);return;
    }
    entered=true;struct Exit{~Exit(){entered=false;}} exit;
    normalize(calls,toon,dt,receipt);
    if(receipt.forward){original_update(toon,dt);return;}
    if(callbacks.publish)callbacks.publish(toon,receipt);
}
}
bool verify_hook(std::string& error){
    if(!image||!original_update)return fail(error,"Toon time owned entry missing");
    std::array<unsigned char,16> entry{};std::array<unsigned char,64> trampoline{};
    if(!memory.copy(image+0x3ea3a0,entry.data(),entry.size(),error)||
       !memory.copy(reinterpret_cast<uintptr_t>(original_update),trampoline.data(),trampoline.size(),error))return false;
    if(entry!=installed||trampoline!=relocated||entry[0]!=0xe9)return fail(error,"Toon time owned entry/trampoline changed");
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,4);
    const auto relay=image+0x3ea3a0+5+static_cast<intptr_t>(displacement);
    std::array<unsigned char,14> jump{};if(!memory.copy(relay,jump.data(),jump.size(),error))return false;
    uintptr_t target=0;std::memcpy(&target,jump.data()+6,sizeof target);
    return (jump[0]==0xff&&jump[1]==0x25&&jump[2]==0&&jump[3]==0&&jump[4]==0&&jump[5]==0&&target==reinterpret_cast<uintptr_t>(&update))||fail(error,"Toon time owned relay target differs");
}
bool initialize(uintptr_t base,const Callbacks& provider,std::string& error){
    std::lock_guard lock(installation);
    if(image)return fail(error,"Toon time installation already attempted");
    if(!base||!provider.owner||!provider.publish||!provider.client_active)return fail(error,"Toon time image/owner/publish provider missing");
    image=base;callbacks=provider;
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    constexpr Body callers[]={
        {0x2ffdf0,288,"f17a7d8ec337b9c458883496a19f848facfaff59cf728c12ebc30339fb44587a"},
        {0x2fff60,528,"fb4861d0b64a5b40f62677e6461df7b9d567a58569f6adacb9c1bce08d63e895"}};
    for(const auto& body:callers){std::vector<unsigned char> bytes(body.size);
        if(!memory.copy(image+body.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=body.sha)return fail(error,"Toon original Manager caller body differs RVA="+std::to_string(body.rva));}
    std::vector<unsigned char> before(3488);
    if(!memory.copy(image+0x3ea3a0,before.data(),before.size(),error))return false;
    if(mt2mp::sha256(before)!="97cf6d74da03fd6d99c7a8e80cd2807e77fde9f66aca3545ed604d35c4811cbb")
        return fail(error,"Toon original EntityUpdate whole body differs");
    const auto target=reinterpret_cast<void*>(image+0x3ea3a0);
    auto status=MH_CreateHook(target,reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original_update));
    if(status!=MH_OK)return fail(error,"Toon time MH_CreateHook status="+std::to_string(status));
    if(!memory.copy(reinterpret_cast<uintptr_t>(original_update),relocated.data(),relocated.size(),error))return false;
    status=MH_EnableHook(target);if(status!=MH_OK)return fail(error,"Toon time MH_EnableHook status="+std::to_string(status));
    std::vector<unsigned char> after(before.size());
    if(!memory.copy(image+0x3ea3a0,after.data(),after.size(),error))return false;
    std::copy_n(after.begin(),installed.size(),installed.begin());
    if(!std::equal(after.begin()+5,after.end(),before.begin()+5))return fail(error,"Toon time hook changed original body tail");
    if(!verify_hook(error))return false;
    ready.store(true,std::memory_order_release);return true;
}
}

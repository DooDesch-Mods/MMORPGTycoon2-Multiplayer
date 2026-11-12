#include "native.h"
#include "gate_relocation.h"
#include "../placement_ui_probe/ui_probe.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>
#include "mt2mp/hooks/minhook_backend.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <intrin.h>
#include "../live_host_state/memory_query_timing.h"

extern "C" void mt2mp_toon_camera_gate();
extern "C" void mt2mp_toon_travel_gate();
extern "C" void mt2mp_toon_attribute_gate();
extern "C" {
uintptr_t mt2mp_toon_gate_original=0;
uintptr_t mt2mp_toon_gate_full=0;
uintptr_t mt2mp_toon_travel_original=0;
uintptr_t mt2mp_toon_travel_rotation=0;
uintptr_t mt2mp_toon_attribute_original=0;
uintptr_t mt2mp_toon_attribute_batched=0;
int mt2mp_toon_gate_selected(uintptr_t receiver) noexcept{
    return mt2mp::game::toon_time_fence::camera_gate::selected(receiver)?1:0;
}
}
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
std::array<unsigned char,16> camera_installed{};
std::array<unsigned char,64> camera_relocated{};
std::array<unsigned char,16> travel_installed{};
std::array<unsigned char,64> travel_relocated{};
std::array<unsigned char,16> attribute_installed{};
std::array<unsigned char,64> attribute_relocated{};
bool camera_ready=false,camera_attempted=false;
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
        live_host_state::apply_timing::Scope timing(live_host_state::apply_timing::Part::ToonOriginal);
        uint32_t exception=0;
        {live_host_state::apply_timing::PageReadScope boundary(live_host_state::apply_timing::PageBoundary::OriginalCall);
            exception=update_call(toon,dt);}
        return !exception||fail(error,"Toon original zero-time entry exception="+std::to_string(exception));
    }
};
void update(uintptr_t toon,float dt){
    if(!callbacks.client_active||!callbacks.client_active()){
        const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
        bool owned=false;
        if(camera_gate::state.root&&callbacks.ordered_owner){
            std::string error;
            try{if(!callbacks.ordered_owner(toon,caller,camera_gate::state.root,owned,error)){
                camera_gate::state.failed=true;if(camera_gate::state.cause.empty())camera_gate::state.cause=error.empty()?"Toon camera owner failed without cause":error;}}
            catch(const std::exception& ex){camera_gate::state.failed=true;if(camera_gate::state.cause.empty())camera_gate::state.cause=ex.what();}
            catch(...){camera_gate::state.failed=true;if(camera_gate::state.cause.empty())camera_gate::state.cause="Toon camera owner nonstandard exception";}
        }
        camera_gate::Entry selected(owned?toon:0);
        if(callbacks.observe)callbacks.observe(toon,dt,caller,false);
        original_update(toon,dt);
        if(callbacks.observe)callbacks.observe(toon,dt,caller,true);
        return;
    }
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
    {live_host_state::apply_timing::Scope timing(live_host_state::apply_timing::Part::ToonNormalize);
        normalize(calls,toon,dt,receipt);}
    if(receipt.forward){original_update(toon,dt);return;}
    if(callbacks.publish)callbacks.publish(toon,receipt);
}
bool camera_gate_impl(std::string& error){
    std::lock_guard lock(installation);
    if(camera_ready){
        MT2FW_MT2_RouteReceipt camera_receipt{},travel_receipt{},attribute_receipt{};
        if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(image+0x3ea462),camera_receipt)||
           !hooks::framework::process_backend().read_route(reinterpret_cast<void*>(image+0x3b7b37),travel_receipt)||
           !hooks::framework::process_backend().read_route(reinterpret_cast<void*>(image+0x3a0942),attribute_receipt))
            return fail(error,"Toon gate Framework ownership changed: "+hooks::framework::process_backend().error());
        std::array<unsigned char,16> entry{},travel_entry{},attribute_entry{};std::array<unsigned char,64> trampoline{},travel_trampoline{},attribute_trampoline{};
        if(!memory.copy(image+0x3ea462,entry.data(),entry.size(),error)||
           !memory.copy(mt2mp_toon_gate_original,trampoline.data(),trampoline.size(),error)||
           !memory.copy(image+0x3b7b37,travel_entry.data(),travel_entry.size(),error)||
           !memory.copy(mt2mp_toon_travel_original,travel_trampoline.data(),travel_trampoline.size(),error)||
           !memory.copy(image+0x3a0942,attribute_entry.data(),attribute_entry.size(),error)||
           !memory.copy(mt2mp_toon_attribute_original,attribute_trampoline.data(),attribute_trampoline.size(),error))return false;
        return (entry==camera_installed&&trampoline==camera_relocated&&travel_entry==travel_installed&&travel_trampoline==travel_relocated&&
            attribute_entry==attribute_installed&&attribute_trampoline==attribute_relocated)||fail(error,"Toon gate entry or trampoline changed");
    }
    if(camera_attempted)return fail(error,"Toon camera gate installation already failed; no retry");
    camera_attempted=true;
    if(!ready.load(std::memory_order_acquire)||!callbacks.ordered_owner)return fail(error,"Toon camera gate requires entry owner");
    std::array<unsigned char,30> before{};
    if(!memory.copy(image+0x3ea462,before.data(),before.size(),error))return false;
    if(mt2mp::sha256(std::vector<unsigned char>(before.begin(),before.end()))!="3d45300ee4bf4b417a337091479e37bfd119ac92bc240759c02710e2b7d0d99f")
        return fail(error,"Toon camera gate original branch differs");
    std::vector<unsigned char> travel_body(5120);
    if(!memory.copy(image+0x3b78b0,travel_body.data(),travel_body.size(),error))return false;
    if(mt2mp::sha256(travel_body)!="5227570ec1fc062edf107ffbb456d83234e4fba1e5a70e270e2e107fc2d58500")
        return fail(error,"Toon travel original whole body differs");
    std::array<unsigned char,30> travel_before{};
    if(!memory.copy(image+0x3b7b37,travel_before.data(),travel_before.size(),error))return false;
    if(mt2mp::sha256(std::vector<unsigned char>(travel_before.begin(),travel_before.end()))!="d41c365582b8c40f01a4e3e08ceed68d9bb1090aafe2f06af4740f327a3fd8fc")
        return fail(error,"Toon travel rotation branch differs");
    std::vector<unsigned char> attribute_body(6816);
    if(!memory.copy(image+0x3a0350,attribute_body.data(),attribute_body.size(),error))return false;
    if(mt2mp::sha256(attribute_body)!="e994c1ef7cf8ff10fe463e8def99af65798238dce88481dbd3dc82838c828b79")
        return fail(error,"Toon attribute original whole body differs");
    std::array<unsigned char,30> attribute_before{};
    if(!memory.copy(image+0x3a0942,attribute_before.data(),attribute_before.size(),error))return false;
    if(mt2mp::sha256(std::vector<unsigned char>(attribute_before.begin(),attribute_before.end()))!="daf271c95120774c9f0da8cf3696e87a1ce61f7053aa60371753fe406104e14b")
        return fail(error,"Toon attribute cadence branch differs");
    auto* target=reinterpret_cast<void*>(image+0x3ea462);
    auto status=MH_CreateHook(target,reinterpret_cast<void*>(&mt2mp_toon_camera_gate),reinterpret_cast<void**>(&mt2mp_toon_gate_original));
    if(status!=MH_OK||!mt2mp_toon_gate_original)return fail(error,"Toon camera gate prepare status="+std::to_string(status));
    mt2mp_toon_gate_full=image+camera_gate::full_update_rva;
    if(!memory.copy(mt2mp_toon_gate_original,camera_relocated.data(),camera_relocated.size(),error))return false;
    if(!camera_gate::camera_relocation(before,camera_relocated,image))return fail(error,"Toon camera gate relocated CMP or continuation differs");
    status=MH_EnableHook(target);if(status!=MH_OK)return fail(error,"Toon camera gate enable status="+std::to_string(status));
    std::array<unsigned char,30> after{};
    if(!memory.copy(image+0x3ea462,after.data(),after.size(),error))return false;
    std::copy_n(after.begin(),camera_installed.size(),camera_installed.begin());
    if(after[0]!=0xe9||!std::equal(after.begin()+5,after.end(),before.begin()+5))return fail(error,"Toon camera gate modified original branch tail");
    auto* travel_target=reinterpret_cast<void*>(image+0x3b7b37);
    status=MH_CreateHook(travel_target,reinterpret_cast<void*>(&mt2mp_toon_travel_gate),reinterpret_cast<void**>(&mt2mp_toon_travel_original));
    if(status!=MH_OK||!mt2mp_toon_travel_original)return fail(error,"Toon travel gate prepare status="+std::to_string(status));
    mt2mp_toon_travel_rotation=image+0x3b7b3d;
    if(!memory.copy(mt2mp_toon_travel_original,travel_relocated.data(),travel_relocated.size(),error))return false;
    if(!camera_gate::travel_relocation(travel_relocated,image))
        return fail(error,"Toon travel relocated conditional or continuations differ");
    status=MH_EnableHook(travel_target);if(status!=MH_OK)return fail(error,"Toon travel gate enable status="+std::to_string(status));
    std::array<unsigned char,30> travel_after{};
    if(!memory.copy(image+0x3b7b37,travel_after.data(),travel_after.size(),error))return false;
    std::copy_n(travel_after.begin(),travel_installed.size(),travel_installed.begin());
    if(travel_after[0]!=0xe9||!std::equal(travel_after.begin()+5,travel_after.end(),travel_before.begin()+5))
        return fail(error,"Toon travel gate modified original branch tail");
    auto* attribute_target=reinterpret_cast<void*>(image+0x3a0942);
    status=MH_CreateHook(attribute_target,reinterpret_cast<void*>(&mt2mp_toon_attribute_gate),reinterpret_cast<void**>(&mt2mp_toon_attribute_original));
    if(status!=MH_OK||!mt2mp_toon_attribute_original)return fail(error,"Toon attribute gate prepare status="+std::to_string(status));
    mt2mp_toon_attribute_batched=image+0x3a0948;
    if(!memory.copy(mt2mp_toon_attribute_original,attribute_relocated.data(),attribute_relocated.size(),error))return false;
    if(!camera_gate::attribute_relocation(attribute_relocated,image))return fail(error,"Toon attribute relocated conditional or continuations differ");
    status=MH_EnableHook(attribute_target);if(status!=MH_OK)return fail(error,"Toon attribute gate enable status="+std::to_string(status));
    std::array<unsigned char,30> attribute_after{};
    if(!memory.copy(image+0x3a0942,attribute_after.data(),attribute_after.size(),error))return false;
    std::copy_n(attribute_after.begin(),attribute_installed.size(),attribute_installed.begin());
    if(attribute_after[0]!=0xe9||!std::equal(attribute_after.begin()+5,attribute_after.end(),attribute_before.begin()+5))
        return fail(error,"Toon attribute gate modified original branch tail");
    camera_ready=true;return true;
}
}
bool initialize_camera_gate(std::string& error){return camera_gate_impl(error);}
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

#include "visual.h"
#include "probe.h"
#include "../npc_census.h"
#include "fingerprints.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <exception>
#include <mutex>

namespace mt2mp::game::toon_weapon_visual {
namespace {
using Equip=void(*)(uintptr_t,bool);
using Attach=int32_t(*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,int32_t,int32_t,bool,bool,uintptr_t);
uintptr_t image=0;
Callbacks callbacks;
Equip original=nullptr;
std::atomic<bool> ready=false;
std::mutex gate;
placement_ui_probe::ProcessMemory memory;
std::array<unsigned char,16> installed{};
std::array<unsigned char,64> relocated{};
thread_local bool entered=false;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
uint32_t scene_call(uintptr_t shard,uintptr_t* out){
    __try{*out=reinterpret_cast<uintptr_t(*)(uintptr_t)>(image+0x507ba0)(shard);}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
uint32_t remove_call(uintptr_t toon){
    __try{reinterpret_cast<void(*)(uintptr_t)>(image+0x39d4d0)(toon);}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
uint32_t attach_call(const Plan* p,const Part* part,int32_t* result){
    __try{*result=reinterpret_cast<Attach>(image+0x38a00)(p->skeleton,part->name,part->transform,p->toon+0x4b8,p->ilvl,p->variant,false,false,p->scene);}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
uint32_t cache_call(const Plan* p,bool* exchanged){
    __try{*exchanged=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(p->toon+0x4dc),p->attack,p->prior_attack)==p->prior_attack;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
uint32_t matrix_call(void* transform,const void* bone,void* result){
    __try{
        const auto local=reinterpret_cast<const void*(*)(void*)>(image+0x9ff8d0)(transform);
        reinterpret_cast<void(*)(void*,const void*,const void*)>(image+0x9f88e0)(result,bone,local);
    }__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
bool returned(Call& call,std::string& error,const char* operation){
    if(call.exception)return fail(error,std::string("Weapon visual ")+operation+" exception="+std::to_string(call.exception));
    call.returned=true;return true;
}
class RandomMemory final:public mt2mp::npc_census::Memory {
public:
    bool copy(uintptr_t address,void* out,size_t count,std::string& error)override{return memory.copy(address,out,count,error);}
};
class Native final:public Calls {
public:
    bool random(std::array<uint64_t,4>& out,std::string& error)override{
        RandomMemory reader;return mt2mp::npc_census::read_global_random(reader,image,out,error);
    }
    bool pin(uintptr_t toon,Lease& lease,bool& owned,std::string& error)override{
        owned=false;
        if(!callbacks.owner)return fail(error,"Weapon visual owner provider missing");
        if(!callbacks.owner(toon,lease,owned,error))return false;
        if(!owned)return true;
        if(lease.toon!=toon||!lease.subscriber||lease.context.role!=action_session::Role::Client)
            return fail(error,"Weapon visual Client lease receiver/Subscriber/role differs");
        if(!ready.load(std::memory_order_acquire))return fail(error,"Weapon visual owned hook not ready");
        return verify_hook(error);
    }
    bool plan(uintptr_t toon,Plan& out,std::string& error)override{return read_plan(memory,image,toon,out,error);}
    bool scene(Plan& p,Call& call,std::string& error)override{
        if(!p.shard)return fail(error,"Weapon visual actor has no current shard");
        call.attempted=true;call.exception=scene_call(p.shard,&p.scene);
        return returned(call,error,"GetScene")&&(p.scene!=0||fail(error,"Weapon visual current scene is null"));
    }
    bool matches(const Plan& p,bool& same,std::string& error)override{return read_attachment(memory,p,same,error);}
    bool remove(const Plan& p,Call& call,std::string& error)override{
        call.attempted=true;call.exception=remove_call(p.toon);return returned(call,error,"RemoveWeaponModel");
    }
    bool cache(const Plan& p,Call& call,std::string& error)override{
        bool exchanged=false;call.attempted=true;call.exception=cache_call(&p,&exchanged);
        return returned(call,error,"attack cache")&&(exchanged||fail(error,"Weapon visual prior attack cache changed"));
    }
    bool attach(const Plan& p,const Part& part,Call& call,std::string& error)override{
        int32_t result=0;call.attempted=true;call.exception=attach_call(&p,&part,&result);
        return returned(call,error,"SetWeaponAttachment")&&(result==p.variant||fail(error,"Weapon explicit attachment returned different variant"));
    }
    bool matrix(const Plan& p,const Part& part,Call& call,std::string& error)override{
        // GetMatrix only mutates this local copy's cached matrix/dirty bit.
        alignas(16) std::array<unsigned char,0x6c> transform{};
        alignas(16) std::array<float,16> bone{},expected{},actual{};
        uintptr_t matrices=0,instance=0;
        if(!memory.copy(part.transform,transform.data(),transform.size(),error)||
           !memory.copy(p.skeleton+0x18,&matrices,sizeof matrices,error)||!matrices||
           !memory.copy(matrices+static_cast<size_t>(part.bone)*0x40,bone.data(),sizeof bone,error)||
           !memory.copy(p.skeleton+0xe8,&instance,sizeof instance,error)||!instance||
           !memory.copy(instance,actual.data(),sizeof actual,error))return fail(error,error.empty()?"Weapon attachment matrix owner absent":error);
        call.attempted=true;call.exception=matrix_call(transform.data(),bone.data(),expected.data());
        return returned(call,error,"local attachment matrix")&&(actual==expected||fail(error,"Weapon attachment actual matrix differs from original bone/transform composition"));
    }
};
void publish(uintptr_t toon,const Receipt& receipt)noexcept{if(callbacks.publish)callbacks.publish(toon,receipt);}
void equip(uintptr_t toon,bool rez){
    Receipt receipt;receipt.hook=true;
    if(!ready.load(std::memory_order_acquire)){original(toon,rez);return;}
    Native calls;std::string error;
    if(entered){
        Lease lease;
        try{if(!calls.pin(toon,lease,receipt.owned,error))receipt.cause=error;}
        catch(const std::exception& e){receipt.cause=std::string("Weapon nested admission: ")+e.what();}
        catch(...){receipt.cause="Weapon nested admission nonstandard exception";}
        if(!receipt.owned){receipt.forwarded=true;publish(toon,receipt);original(toon,rez);return;}
        if(receipt.cause.empty())receipt.cause="Weapon visual nested owned Equip rejected";
        publish(toon,receipt);return;
    }
    entered=true;
    struct Exit {~Exit(){entered=false;}} exit;
    const bool result=reconcile(calls,toon,receipt,error);
    if(!receipt.owned){receipt.forwarded=true;publish(toon,receipt);original(toon,rez);return;}
    if(!result&&receipt.cause.empty())receipt.cause=error;
    publish(toon,receipt);
}
}
bool verify_hook(std::string& error){
    if(!image||!original)return fail(error,"Weapon owned Equip hook missing");
    std::array<unsigned char,16> entry{};std::array<unsigned char,64> trampoline{};
    if(!memory.copy(image+0x3a1df0,entry.data(),entry.size(),error)||
       !memory.copy(reinterpret_cast<uintptr_t>(original),trampoline.data(),trampoline.size(),error))return false;
    if(entry!=installed||trampoline!=relocated||entry[0]!=0xe9)return fail(error,"Weapon owned Equip entry/trampoline changed");
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,4);
    const auto relay=image+0x3a1df0+5+static_cast<intptr_t>(displacement);
    std::array<unsigned char,14> jump{};if(!memory.copy(relay,jump.data(),jump.size(),error))return false;
    uintptr_t target=0;std::memcpy(&target,jump.data()+6,sizeof target);
    return (jump[0]==0xff&&jump[1]==0x25&&jump[2]==0&&jump[3]==0&&jump[4]==0&&jump[5]==0&&target==reinterpret_cast<uintptr_t>(&equip))||fail(error,"Weapon owned Equip relay target differs");
}
bool initialize(uintptr_t base,const Callbacks& provider,std::string& error){
    std::lock_guard lock(gate);
    if(image)return fail(error,"Weapon visual hook initialization already attempted");
    if(!base||!provider.owner||!provider.publish)return fail(error,"Weapon visual image/owner/receipt provider missing");
    image=base;callbacks=provider;
    std::vector<unsigned char> equip_before;
    for(const auto& body:bodies){std::vector<unsigned char> actual(body.size);
        if(!memory.copy(base+body.rva,actual.data(),actual.size(),error))return false;
        if(mt2mp::sha256(actual)!=body.sha)return fail(error,"Weapon original body unsupported RVA="+std::to_string(body.rva));
        if(body.rva==0x3a1df0)equip_before=actual;}
    if(equip_before.size()<16)return fail(error,"Weapon Equip full-body guard missing");
    const auto target=reinterpret_cast<void*>(base+0x3a1df0);
    auto result=MH_CreateHook(target,reinterpret_cast<void*>(&equip),reinterpret_cast<void**>(&original));
    if(result!=MH_OK)return fail(error,"Weapon MH_CreateHook status="+std::to_string(result));
    if(!memory.copy(reinterpret_cast<uintptr_t>(original),relocated.data(),relocated.size(),error))return false;
    result=MH_EnableHook(target);if(result!=MH_OK)return fail(error,"Weapon MH_EnableHook status="+std::to_string(result));
    std::vector<unsigned char> after(equip_before.size());
    if(!memory.copy(base+0x3a1df0,after.data(),after.size(),error))return false;
    std::copy_n(after.begin(),installed.size(),installed.begin());
    if(!std::equal(after.begin()+5,after.end(),equip_before.begin()+5))return fail(error,"Weapon Equip hook changed original body tail");
    if(!verify_hook(error))return false;
    ready.store(true,std::memory_order_release);return true;
}
bool inspect(uintptr_t toon,Observation& out,std::string& error){
    out={};
    if(!ready.load(std::memory_order_acquire)||!verify_hook(error))return fail(error,error.empty()?"Weapon visual reader is not initialized":error);
    Native calls;Plan plan;
    if(!read_plan(memory,image,toon,plan,error))return false;
    if(plan.actor&&!read_scene(memory,plan,error))return false;
    if(!read_observation(memory,plan,out,error))return false;
    if(out.attachment_present&&!plan.parts.empty()){
        out.matrix_matching=calls.matrix(plan,plan.parts.back(),out.matrix_call,out.matrix_cause);
        out.matrix_observed=out.matrix_call.returned;
        if(!out.matrix_observed)return fail(error,out.matrix_cause);
    }
    Plan after;
    if(!read_plan(memory,image,toon,after,error))return false;
    if(after.actor&&!read_scene(memory,after,error))return false;
    if(after!=plan)return fail(error,"Weapon probe actual plan owners or tuple changed during read");
    return true;
}
bool refresh(uintptr_t toon,Receipt& receipt,std::string& error){
    receipt={};Native calls;
    if(entered)return fail(error,"Weapon visual explicit refresh nested");
    entered=true;struct Exit {~Exit(){entered=false;}} exit;
    const bool result=reconcile(calls,toon,receipt,error);publish(toon,receipt);return result;
}
}

#include "dungeon_instance_runtime.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "mt2mp/game/native_engine.h"
#include "session_provider.h"
#include "typed_action_runtime.h"
#include "subscriber_lifecycle/client_world.h"
#include <Windows.h>
#include <algorithm>

namespace mt2mp::game::dungeon_instance_live {
namespace {
namespace host=mt2mp::build_actions::private_native::host;
struct Memory final:subscriber_lifecycle::Memory {
    host::ProcessMemory process;
    bool read(uintptr_t p,void* out,size_t bytes,std::string& error)override{return process.copy(p,out,bytes,error);}
    template<class T>bool at(uintptr_t p,size_t offset,T& value,std::string& error){
        if(!p||offset>UINTPTR_MAX-p||sizeof(T)>UINTPTR_MAX-(p+offset))return fail(error,"field span invalid");
        return read(p+offset,&value,sizeof(value),error);
    }
};
DWORD find_uid(uintptr_t image,uint64_t uid,uintptr_t& out){
    __try{out=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD reserve_call(uintptr_t image,uintptr_t instance,uintptr_t party){
    __try{reinterpret_cast<void(*)(void*,void*)>(image+0xb5fe0)(reinterpret_cast<void*>(instance),reinterpret_cast<void*>(party));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD store_call(uintptr_t address,const void* before,const void* target,size_t bytes,bool& matched){
    __try{matched=std::memcmp(reinterpret_cast<const void*>(address),before,bytes)==0;
        if(matched)std::memcpy(reinterpret_cast<void*>(address),target,bytes);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool same_world(const dungeon_scene_identity_runtime::World& a,const dungeon_scene_identity_runtime::World& b){
    return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&
        a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.uuid==b.uuid&&a.sha256==b.sha256;
}
bool session(const action_session::Context& expected,const action_session::Binding& binding,
             const dungeon_scene_identity_runtime::World& world,uintptr_t& image,std::string& error){
    action_session::Context current;action_session::Binding actual;uint32_t thread=0;std::string sha;
    if(!action_session::selected_session().current(current,error)||current!=expected||
       !typed_action_runtime::read_binding(expected.snapshot_uuid,actual,error)||actual!=binding)
        return fail(error,error.empty()?"current Session/binding differs":error);
    if(world.uuid.empty()||world.generation!=binding.native_generation||world.world_epoch!=binding.native_epoch)
        return fail(error,"current Scene world differs from Session binding: session_snapshot="+expected.snapshot_uuid+" native_snapshot="+world.uuid+
            " native_epoch="+std::to_string(world.world_epoch)+" binding_epoch="+std::to_string(binding.native_epoch)+
            " native_generation="+std::to_string(world.generation)+" binding_generation="+std::to_string(binding.native_generation));
    if(!native::read_resolver_receipt(image,thread,sha,error))return false;
    return (thread==GetCurrentThreadId()&&image==uintptr_t(GetModuleHandleW(nullptr))&&
        sha=="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")||fail(error,"actual image/Mainthread differs");
}
struct Parties {
    Memory& memory;uintptr_t image;party_identity::Scope scope;
    bool identify(uintptr_t pointer,std::string& key,std::string& error){
        uintptr_t ap=0,registered=0;uint64_t uid=0;
        if(!memory.at(pointer,0,ap,error)||ap!=image+0x174a370||!memory.at(pointer,0x18,uid,error)||!uid)
            return fail(error,error.empty()?"assigned Party class/full UID differs":error);
        const auto code=find_uid(image,uid,registered);
        if(code||registered!=pointer)return fail(error,"assigned Party original UID lookup differs SEH="+std::to_string(code));
        return party_identity::registry.identify(scope,pointer,uid,key,error);
    }
    bool resolve(const std::string& key,uintptr_t& pointer,std::string& error){
        party_identity::Entry row;pointer=0;
        if(!party_identity::registry.lookup(scope,key,row,error))return false;
        std::string actual;if(!identify(row.pointer,actual,error)||actual!=key)return fail(error,error.empty()?"assigned Party binding changed":error);
        pointer=row.pointer;return true;
    }
};
}
bool take_current(const action_session::Context& context,const action_session::Binding& binding,Census& out,std::string& error)try{
    out={};Census ready;std::vector<dungeon_scene_roster::Scene> scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(ready.world,scenes,error))return false;
    uintptr_t image=0;if(!session(context,binding,ready.world,image,error))return false;
    Memory memory;Parties parties{memory,image,{ready.world.uuid,ready.world.world_epoch,ready.world.generation,uintptr_t(ready.world.map)+0x1c0}};
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){return memory.at(p,offset,value,e);};
    auto identify=[&](uintptr_t p,std::string& key,std::string& e){return parties.identify(p,key,e);};
    for(const auto& scene:scenes)if(scene.owner.type==2){Local row;if(!read(at,identify,image,scene.owner,row,error))return false;ready.rows.push_back(std::move(row));}
    dungeon_scene_identity_runtime::World after;std::vector<dungeon_scene_roster::Scene> final_scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(after,final_scenes,error)||!same_world(ready.world,after)||scenes.size()!=final_scenes.size())
        return fail(error,error.empty()?"complete current Scene roster changed":error);
    for(size_t i=0;i<scenes.size();++i){const auto& a=scenes[i].owner;const auto& b=final_scenes[i].owner;
        if(a.key!=b.key||a.exterior!=b.exterior||a.exterior_uid!=b.exterior_uid||a.dungeon!=b.dungeon||a.instance!=b.instance||a.shard!=b.shard||a.region!=b.region)
            return fail(error,"complete current Instance owner changed");}
    if(!session(context,binding,after,image,error))return false;
    std::sort(ready.rows.begin(),ready.rows.end(),[](const auto& a,const auto& b){return a.state.scene<b.state.scene;});
    out=std::move(ready);error.clear();return true;
}catch(const std::exception& e){out={};return fail(error,std::string("current census: ")+e.what());}
bool adopt_held(const action_session::Context& context,const action_session::Binding& binding,const std::vector<State>& target,
                Receipt& receipt,std::string& error)try{
    receipt={};if(context.role!=action_session::Role::Client)return fail(error,"value adoption requires Client role");
    Census before;if(!take_current(context,binding,before,error))return false;
    Memory memory;subscriber_lifecycle::first_append::ClientWorld held;
    if(!subscriber_lifecycle::first_append::read_client_world_roots(memory,before.world.sha256,before.world.uuid,true,held,error,&context))return false;
    uintptr_t image=0;if(!session(context,binding,before.world,image,error))return false;
    const auto reject=[&](const std::string& cause){receipt.unknown=receipt.attempted;receipt.cause=cause;return fail(error,cause);};
    if(target.size()!=before.rows.size())return reject("membership requires its original Instance lifecycle before value adoption");
    for(size_t i=0;i<target.size();++i)if(!valid(target[i],error)||target[i].scene!=before.rows[i].state.scene)return reject(error.empty()?"current Instance membership/order differs":error);
    constexpr std::array<uint8_t,5> reserve_body{0x48,0x89,0x51,0x68,0xc3};std::array<uint8_t,5> actual{};
    if(!memory.read(image+0xb5fe0,actual.data(),actual.size(),error)||actual!=reserve_body)return reject(error.empty()?"original ReserveForParty body differs":error);
    Parties parties{memory,image,{before.world.uuid,before.world.world_epoch,before.world.generation,uintptr_t(before.world.map)+0x1c0}};
    auto resolve=[&](const std::string& key,uintptr_t& value,std::string& e){return parties.resolve(key,value,e);};
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){return memory.at(p,offset,value,e);};
    auto identify=[&](uintptr_t p,std::string& key,std::string& e){return parties.identify(p,key,e);};
    for(size_t i=0;i<target.size();++i){const auto& row=before.rows[i];
        const auto scene=[&](){dungeon_scene_key::Scene value;value.type=2;value.key=row.state.scene;value.exterior_key=row.state.exterior;
            value.instance_index=row.state.index;value.instance=row.instance;value.dungeon=row.dungeon;value.shard=row.shard;value.region=row.region;return value;}();
        auto local_read=[&](Local& value,std::string& e){return read(at,identify,image,scene,value,e);};
        auto store=[&](const Local& expected,size_t offset,const void* old_value,const void* new_value,size_t bytes,std::string& e){
            uintptr_t ap=0,owner=0;if(!memory.at(expected.instance,0,ap,e)||ap!=image+0x1719450||!memory.at(expected.instance,0x30,owner,e)||owner!=expected.dungeon)return fail(e,e.empty()?"store owner changed":e);
            bool matched=false;const auto code=store_call(expected.instance+offset,old_value,new_value,bytes,matched);
            return (!code&&matched)||fail(e,"guarded scalar store failed SEH="+std::to_string(code));
        };
        auto reserve=[&](const Local& expected,uintptr_t party,std::string& e){uintptr_t prior=0;
            if(!memory.at(expected.instance,0x68,prior,e)||prior!=expected.party)return fail(e,e.empty()?"assigned Party changed before original call":e);
            const auto code=reserve_call(image,expected.instance,party);return !code||fail(e,"original ReserveForParty SEH="+std::to_string(code));};
        Receipt part;const bool ok=adopt(row,target[i],resolve,store,reserve,local_read,part,error);
        receipt.attempted=receipt.attempted||part.attempted;receipt.attempted_stores+=part.attempted_stores;receipt.stores+=part.stores;receipt.party_calls+=part.party_calls;
        if(!ok)return reject(error);
    }
    Census after;if(!take_current(context,binding,after,error)||!same_world(before.world,after.world)||after.rows.size()!=target.size())return reject(error.empty()?"final complete census changed":error);
    for(size_t i=0;i<target.size();++i)if(after.rows[i].state!=target[i]||after.rows[i].instance!=before.rows[i].instance)return reject("final complete saved values differ");
    subscriber_lifecycle::first_append::ClientWorld final_guard;
    if(!subscriber_lifecycle::first_append::read_client_world_roots(memory,after.world.sha256,after.world.uuid,true,final_guard,error,&context)||
       !subscriber_lifecycle::first_append::same_client_owner(held,final_guard))return reject(error.empty()?"held native owner changed":error);
    receipt.confirmed=true;error.clear();return true;
}catch(const std::exception& e){receipt.unknown=receipt.attempted;receipt.cause=e.what();return fail(error,std::string("held adoption: ")+e.what());}
}

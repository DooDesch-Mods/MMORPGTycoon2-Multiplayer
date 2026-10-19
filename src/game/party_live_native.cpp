#include "party_live_native.h"
#include "party_live_codes.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "subscriber_lifecycle/append_calls.h"
#include "subscriber_lifecycle/capture.h"
#include "subscriber_lifecycle/save_scope.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <memory>

namespace mt2mp::game::party_live_native {
namespace {
namespace local=subscriber_lifecycle;
namespace decode=local::first_append;
constexpr const char* executable="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool same_target(const party_live::Record& a,const party_live::Record& b){
    return a.key==b.key&&a.sha==b.sha&&a.shard==b.shard&&a.region==b.region&&a.manager_region==b.manager_region&&
        a.toons==b.toons&&a.managers==b.managers;
}
struct Process final:local::Memory {
    bool read(uintptr_t pointer,void* out,size_t count,std::string& error)override{
        if(!pointer||pointer>UINTPTR_MAX-count)return fail(error,"Party live native read extent overflows");
        SIZE_T actual=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(pointer),out,count,&actual)||actual!=count)
            return fail(error,"Party live ReadProcessMemory failed error="+std::to_string(GetLastError()));
        return true;
    }
} memory;
template<class T>bool field(uintptr_t object,size_t offset,T& out,std::string& error){
    if(!object||offset>UINTPTR_MAX-object)return fail(error,"Party live field extent overflows");
    return memory.read(object+offset,&out,sizeof out,error);
}
std::atomic<bool> hook_ready=false,active=false;
local::LookupScope links;
struct Resource {
    decode::NativeTree record,reciprocal;
    decode::DecodeReceipt decoded,reciprocal_decoded;
    uintptr_t context=0,party=0;
    bool context_constructed=false,context_destroy_attempted=false,context_destroyed=false;
};
std::vector<std::unique_ptr<Resource>> retained;
uint32_t factory_call(uintptr_t function,uintptr_t* result){
    __try{*result=reinterpret_cast<uintptr_t(*)(const void*)>(function)(nullptr);return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t load_call(uintptr_t function,void* object,void* record,void* context){
    __try{reinterpret_cast<void(*)(void*,void*,void*)>(function)(object,record,context);return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t manager_call(uintptr_t function,void* manager,void* party){
    __try{reinterpret_cast<void(*)(void*,void*)>(function)(manager,party);return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t property_call(uintptr_t function,const void* property,void* object,void* record,void* context,bool* accepted){
    __try{*accepted=reinterpret_cast<bool(*)(const void*,void*,void*,void*)>(function)(property,object,record,context);return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t property_resolve_call(uintptr_t function,const void* property,void* object,void* context){
    __try{reinterpret_cast<void(*)(const void*,void*,void*)>(function)(property,object,context);return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Capture {
    local::Graph graph;
    static bool inspect(void* user,const void* record,const void* context,std::string& error){
        return local::read_graph(memory,uintptr_t(record),uintptr_t(context),{100000,1000000,32*1024*1024,100000},
                                 static_cast<Capture*>(user)->graph,error);
    }
};
struct ExternalScope {
    NativePort& owner;
    const party_live::References& references;
    std::vector<local::ExternalLink> expected;
    static bool fresh(void* user,const local::ExternalLink& link,std::string& error){
        auto& self=*static_cast<ExternalScope*>(user);uintptr_t current=0;
        if(!self.owner.owner(error)||!self.references.resolve(link.portable_key,current,error))return false;
        return current==link.object||fail(error,"Party original Resolve external canonical owner changed");
    }
};
bool pin_code(uintptr_t image,std::string& error){
    for(const auto& code:party_live_codes::helpers){
        std::vector<unsigned char> body(code.bytes);
        if(image>UINTPTR_MAX-code.rva||!memory.read(image+code.rva,body.data(),body.size(),error))return false;
        if(mt2mp::sha256(body)!=code.sha)return fail(error,"Party original native body differs RVA="+std::to_string(code.rva));
    }
    return true;
}
bool typed(uintptr_t image,uintptr_t object,uintptr_t expected,std::string& error){uintptr_t ap=0;
    return field(object,0,ap,error)&&(ap==image+expected||fail(error,"Party native owner addresspoint differs RVA="+std::to_string(expected)));
}
bool identity(uintptr_t image,uintptr_t pointer,uint64_t& uid,std::string& error){
    uintptr_t registered=0;
    if(!field(pointer,0x18,uid,error))return false;
    if(!uid)return fail(error,"Party native object has no actual full UID");
    return decode::registry_lookup(image,uid,registered,error)&&
        (registered==pointer||fail(error,"Party native full UID resolves another current object"));
}
bool members(uintptr_t image,uintptr_t party,std::vector<uintptr_t>& out,std::string& error){
    dungeon_party_census::Array array;
    if(!dungeon_party_census::array(memory,image,party+0x530,party_live_codes::member_array_ap,array,error))return false;
    std::set<uintptr_t> seen;std::vector<uintptr_t> ready;
    for(const auto row:array.members){uintptr_t toon=0;uint64_t uid=0;
        if(!typed(image,row,party_live_codes::member_ap,error)||!field(row,0x18,toon,error)||!typed(image,toon,0x1744e90,error)||
           !identity(image,toon,uid,error))return false;
        if(!seen.insert(toon).second)return fail(error,"Party original member array repeats a Toon");
        ready.push_back(toon);
    }
    dungeon_party_census::Array after;
    if(!dungeon_party_census::array(memory,image,party+0x530,party_live_codes::member_array_ap,after,error))return false;
    if(after!=array)return fail(error,"Party original member array changed during current capture");
    out=std::move(ready);return true;
}
bool scheduler(uintptr_t image,const party_live::Raw& current,bool held,std::string& error){
    uintptr_t tasks=0,ticks=0,updates=0,tasks_owned=0,ticks_owned=0,updates_owned=0;
    if(!field(image,0x13df340,tasks,error)||!field(image,0x13df350,ticks,error)||!field(image,0x13df4a0,updates,error)||
       !typed(image,tasks,0x16fdaf0,error)||!typed(image,ticks,0x16fdc40,error)||!typed(image,updates,0x1709650,error)||
       !field(uintptr_t(current.world.game),0x68,tasks_owned,error)||!field(uintptr_t(current.world.game),0x150,ticks_owned,error)||
       !field(uintptr_t(current.world.game),0x70,updates_owned,error))return false;
    if(tasks!=tasks_owned||ticks!=ticks_owned||updates!=updates_owned)return fail(error,"Party current Game scheduler singleton ownership differs");
    uint8_t task_paused=0,tick_paused=0,update_paused=0;
    if(!field(tasks,0xf8,task_paused,error)||!field(ticks,0x28,tick_paused,error)||!field(updates,0x68,update_paused,error))return false;
    if(task_paused!=uint8_t(held)||tick_paused!=uint8_t(held)||update_paused!=uint8_t(held))
        return fail(error,"Party original SaveScope scheduler pause state differs");
    if(held){int32_t running=-1,waiting=-1;
        if(!field(tasks,0xa8,running,error)||!field(tasks,0x90,waiting,error))return false;
        if(running||waiting)return fail(error,"Party original SaveScope tasks not drained running="+std::to_string(running)+" waiting="+std::to_string(waiting));
    }
    return true;
}
}
void set_lookup_hook_ready(bool value){hook_ready=value;}
bool lookup_active()noexcept{return active.load();}
void* lookup(void* context,int32_t id,local::Lookup original){return links.lookup(GetCurrentThreadId(),context,id,original);}
bool NativePort::owner(std::string& error){
    action_session::Context current;building_mixed_identity::opened_file::CurrentWorld world;
    uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!action_session::selected_session().current(current,error)||!native_open_runtime::read_current_world(world,error)||
       !native::read_resolver_receipt(image,thread,sha,error))return false;
    if(current!=context_||sha!=executable||image!=image_||thread!=thread_||thread!=GetCurrentThreadId()||
       world.uuid!=current_.scope.snapshot||world.world_epoch!=binding_.native_epoch||world.generation!=binding_.native_generation||
       world.game!=current_.world.game||world.mode!=current_.world.mode||world.state!=current_.world.state||world.world!=current_.world.world||
       world.map!=current_.world.map||uintptr_t(world.map)+0x1c0!=current_.scope.root)
        return fail(error,"Party native operation lost its actual Frame/Session/NativeOpen image/Mainthread owner");
    if(blocked_scope_==current_.scope&&!blocked_cause_.empty())return fail(error,blocked_cause_);
    native::WorldStatus status;if(!native::read_world_status(status,error))return false;
    if(status.active_mode_index!=3||status.in_game_state!=4||!status.world_present||status.quitting||status.title_transition_pending||
       status.map_task_pending||status.edit_operations||status.saves_in_progress!=(held_?1:0))
        return fail(error,"Party original active World/edit/save boundary differs");
    if(context_.role==action_session::Role::Client){native::ClockState clock;
        if(!native::read_clock(clock,error))return false;
        if(clock.time_scale!=0||!clock.paused)return fail(error,"Party native Client replay requires its actual paused authoritative Clock");
    }else if(context_.role!=action_session::Role::Host)return fail(error,"Party native boundary has no actual Host or Client role");
    return scheduler(image_,current_,held_,error);
}
bool NativePort::current(const action_session::Context& context,const action_session::Binding& binding,party_live::Raw& out,std::string& error){
    out={};uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,error))return false;
    if(sha!=executable||thread!=GetCurrentThreadId())return fail(error,"Party live requires exact original executable and Mainthread");
    action_session::Context actual;if(!action_session::selected_session().current(actual,error))return false;
    if(actual!=context)return fail(error,"Party live actual Session differs from caller");
    party_live::Raw current;if(!party_saved_identity_runtime::read_current_raw(current,error))return false;
    if(current.scope.snapshot!=current.world.uuid||current.scope.epoch!=current.world.world_epoch||current.scope.generation!=current.world.generation||
       current.scope.epoch!=binding.native_epoch||current.scope.generation!=binding.native_generation||current.scope.root!=uintptr_t(current.world.map)+0x1c0)
        return fail(error,"Party live raw census differs from actual Frame binding: session_snapshot="+context.snapshot_uuid+
            " native_snapshot="+current.world.uuid+" scope_snapshot="+current.scope.snapshot+
            " native_epoch="+std::to_string(current.world.world_epoch)+" scope_epoch="+std::to_string(current.scope.epoch)+" binding_epoch="+std::to_string(binding.native_epoch)+
            " native_generation="+std::to_string(current.world.generation)+" scope_generation="+std::to_string(current.scope.generation)+" binding_generation="+std::to_string(binding.native_generation)+
            " native_root="+std::to_string(uintptr_t(current.world.map)+0x1c0)+" scope_root="+std::to_string(current.scope.root));
    image_=image;thread_=thread;context_=context;binding_=binding;current_=std::move(current);
    if(!owner(error)||!pin_code(image_,error))return false;
    out=current_;return true;
}
bool NativePort::batch(const action_session::Context& context,const action_session::Binding& binding,
    const std::function<bool(std::string&)>& body,std::string& error){
    if(!body)return fail(error,"Party native batch body is absent");
    if(held_){if(context!=context_||binding!=binding_)return fail(error,"Party held batch has another actual owner");
        if(!batch_unknown_.empty())return fail(error,batch_unknown_);return owner(error)&&body(error);}
    party_live::Raw current;if(!this->current(context,binding,current,error))return false;
    batch_unknown_.clear();
    struct Work {NativePort& port;const std::function<bool(std::string&)>& body;};Work work{*this,body};
    const auto run=[](void* user,std::string& cause){auto& w=*static_cast<Work*>(user);w.port.held_=true;
        return w.port.owner(cause)&&w.body(cause);};
    const auto exit=[](void* user,std::string& cause){auto& port=static_cast<Work*>(user)->port;
        if(!port.batch_unknown_.empty())return fail(cause,port.batch_unknown_);
        if(!port.owner(cause))return false;
        for(const auto& reservation:port.reservations_)if(reservation.second.scope==port.current_.scope)
            return fail(cause,"Party original factory reservation did not finish before the shared batch ended");
        for(const auto& staged:port.prepared_)if(staged.second.scope==port.current_.scope)
            return fail(cause,"Party original prepared state did not finish before the shared batch ended");
        return true;
    };
    local::SaveScopeReceipt receipt;
    const local::SaveScopeApi api{reinterpret_cast<local::SaveScopeCall>(image_+0x4a03a0),reinterpret_cast<local::SaveScopeCall>(image_+0x49f6f0)};
    const bool ok=local::with_save_scope(api,current.world.mode,run,exit,&work,receipt,error);
    held_=false;
    if(receipt.unknown){blocked_scope_=current.scope;blocked_cause_=error.empty()?"Party original SaveScope outcome unknown":error;return fail(error,blocked_cause_);}
    if(!batch_unknown_.empty()){blocked_scope_=current.scope;blocked_cause_=batch_unknown_;return fail(error,blocked_cause_);}
    return ok&&owner(error);
}
bool NativePort::capture(const party_live::Member& member,const party_live::References& refs,party_live::Record& value,std::string& error){
    if(!held_)return fail(error,"Party complete original Record requires one held native batch");
    if(!refs.identify||!refs.resolve||!owner(error))return fail(error,error.empty()?"Party canonical current reference readers are absent":error);
    uint64_t uid=0;
    if(!typed(image_,member.pointer,dungeon_party_census::party_ap,error)||!identity(image_,member.pointer,uid,error))return false;
    if(uid!=member.uid)return fail(error,"Party current original Record full UID differs");
    if(!refs.identify(member.shard,value.shard,error)||!refs.identify(member.region,value.region,error))return false;
    value.manager_region.clear();if(member.manager_region&&!refs.identify(member.manager_region,value.manager_region,error))return false;
    value.managers.clear();
    for(const auto& pool:current_.pools)if(std::find(pool.parties.members.begin(),pool.parties.members.end(),member.pointer)!=pool.parties.members.end()){
        party_live::Manager owner;if(!refs.identify(pool.region,owner.region,error))return false;
        owner.removal=std::find(pool.removals.members.begin(),pool.removals.members.end(),member.pointer)!=pool.removals.members.end();
        owner.release=std::find(pool.releases.members.begin(),pool.releases.members.end(),member.pointer)!=pool.releases.members.end();value.managers.push_back(std::move(owner));
    }
    std::sort(value.managers.begin(),value.managers.end(),[](const auto& a,const auto& b){return a.region<b.region;});
    std::vector<uintptr_t> toons;if(!members(image_,member.pointer,toons,error))return false;
    value.toons.clear();
    for(const auto toon:toons){uintptr_t reciprocal=0;std::string key;
        if(!field(toon,0x950,reciprocal,error)||!refs.identify(toon,key,error))return false;
        if(reciprocal!=member.pointer)return fail(error,"Party current original Toon member backlink differs");
        value.toons.push_back(std::move(key));
    }
    Capture captured;local::CaptureReceipt cleanup;
    const local::CaptureApi calls{reinterpret_cast<local::Construct>(image_+0x9667c0),reinterpret_cast<local::Construct>(image_+0x9676a0),
        reinterpret_cast<local::Construct>(image_+0x89e580),reinterpret_cast<local::Construct>(image_+0x89e5f0),
        reinterpret_cast<local::PrepareSave>(image_+0x1103ca0),reinterpret_cast<local::SaveRecord>(image_+0x1103d00)};
    if(!local::capture_original(calls,reinterpret_cast<void*>(member.pointer),Capture::inspect,&captured,cleanup,error)||
       !party_live::captured(captured.graph,member.pointer,refs,value,error)||!owner(error))return false;
    uint64_t final_uid=0;return identity(image_,member.pointer,final_uid,error)&&
        (final_uid==uid||fail(error,"Party current complete original Record changed native lifetime"));
}
bool NativePort::find(uint64_t uid,uintptr_t& pointer,std::string& error){
    pointer=0;if(!uid||!owner(error))return fail(error,error.empty()?"Party full UID lookup is empty":error);
    return decode::registry_lookup(image_,uid,pointer,error);
}
bool NativePort::load_record(const party_live::Member* existing,const party_live::Record& target,const party_live::References& refs,party_live::Member& born,party_live::Receipt& receipt,std::string& error,bool defer_init)try{
    born={};
    if(!held_||context_.role!=action_session::Role::Client||!hook_ready||!refs.resolve||!refs.identify)
        return fail(error,"Party original birth requires held actual Client boundary and confirmed context lookup hook");
    if(!owner(error)||!party_live::validate(target,error))return false;
    bool expected=false;if(!active.compare_exchange_strong(expected,true))return fail(error,"Another dedicated Party native context is already active");
    decode::DecodeApi api;auto resource=std::make_unique<Resource>();bool ok=false,keep=false;ExternalScope borrowed{*this,refs};
    const auto execute=[&]()->bool{
        if(!decode::resolve_record_api(memory,image_,api,error))return false;
        for(const auto& external:target.external){uintptr_t object=0;
            if(!refs.resolve(external.key,object,error)||!object)return fail(error,error.empty()?"Party original external owner is absent":error);
            borrowed.expected.push_back({external.id,external.key,object});
        }
        std::vector<uintptr_t> toons;
        for(const auto& key:target.toons){uintptr_t toon=0;uint64_t uid=0;
            if(!refs.resolve(key,toon,error)||!typed(image_,toon,0x1744e90,error)||!identity(image_,toon,uid,error))return false;
            toons.push_back(toon);
        }
        std::vector<uintptr_t> old_toons;
        if(existing){uint64_t uid=0;
            if(!typed(image_,existing->pointer,dungeon_party_census::party_ap,error)||!identity(image_,existing->pointer,uid,error)||
               !members(image_,existing->pointer,old_toons,error))return false;
            if(uid!=existing->uid)return fail(error,"Party original state load lost its existing native full UID");
            for(const auto toon:old_toons){uintptr_t reciprocal=0;if(!field(toon,0x950,reciprocal,error))return false;
                if(reciprocal!=existing->pointer)return fail(error,"Party existing Toon member has a foreign reciprocal before state load");}
        }
        std::vector<const dungeon_party_census::Pool*> desired;
        for(const auto& manager:target.managers){uintptr_t region=0;
            if(manager.removal||manager.release)return fail(error,"Party target contains pending native manager work instead of a completed Frame boundary");
            if(!refs.resolve(manager.region,region,error))return false;
            const auto pool=std::find_if(current_.pools.begin(),current_.pools.end(),[&](const auto& row){return row.region==region;});
            if(pool==current_.pools.end()||!dungeon_party_census::settled(*pool))return fail(error,"Party target manager is absent or has active native update/deletion/release work");
            desired.push_back(&*pool);
        }
        uintptr_t desired_owner=0;
        if(!target.manager_region.empty()&&!refs.resolve(target.manager_region,desired_owner,error))return false;
        if(desired_owner&&std::none_of(desired.begin(),desired.end(),[&](const auto* row){return row->region==desired_owner;}))
            return fail(error,"Party original owning manager is absent from its complete target membership");
        if(!decode::decode_party_record(api,memory,target.records,resource->record,resource->decoded,error))return false;
        if(!decode::decode_integer_record(api,memory,"party",target.root_link,resource->reciprocal,resource->reciprocal_decoded,error))return false;
        uintptr_t property_ap=0,property_offset=0;const auto property=image_+party_live_codes::toon_party_property;
        if(!field(property,0,property_ap,error)||!field(property,0x38,property_offset,error))return false;
        if(property_ap!=image_+party_live_codes::toon_party_property_ap||property_offset!=0x950)
            return fail(error,"Party original Toon reciprocal property class/member offset differs");
        local::CallReceipt call;receipt.attempted=true;++receipt.calls.context;
        if(!decode::context_call(reinterpret_cast<decode::Unary>(image_+0x89e940),&resource->context,call,error))return false;
        resource->context_constructed=true;
        if(!links.arm(thread_,&resource->context,target.local_ids,borrowed.expected,ExternalScope::fresh,&borrowed,error))return false;
        uint32_t seh=0;
        if(existing){
            resource->party=existing->pointer;
            for(const auto toon:old_toons)if(std::find(toons.begin(),toons.end(),toon)==toons.end()){
                ++receipt.calls.toon_leave;
                if(!decode::context_call(reinterpret_cast<decode::Unary>(image_+0x3e66d0),reinterpret_cast<void*>(toon),call,error))return false;
            }
        }else{
            ++receipt.calls.factory;seh=factory_call(image_+0xd96cc0,&resource->party);
            if(seh||!resource->party)return fail(error,"Party original RTTI factory failed SEH="+std::to_string(seh));
        }
        if(!typed(image_,resource->party,dungeon_party_census::party_ap,error))return false;
        ++receipt.calls.load;seh=load_call(image_+0xceba00,reinterpret_cast<void*>(resource->party),reinterpret_cast<void*>(resource->record.root),&resource->context);
        if(seh)return fail(error,"Party original complete Record load failed SEH="+std::to_string(seh));
        for(const auto toon:toons){uintptr_t old=0;if(!field(toon,0x950,old,error))return false;
            if(old&&old!=resource->party){++receipt.calls.toon_leave;
                if(!decode::context_call(reinterpret_cast<decode::Unary>(image_+0x3e66d0),reinterpret_cast<void*>(toon),call,error))return false;}
            ++receipt.calls.toon_bind;bool accepted=false;
            seh=property_call(image_+0xe090d0,reinterpret_cast<void*>(property),reinterpret_cast<void*>(toon),
                reinterpret_cast<void*>(resource->reciprocal.root),&resource->context,&accepted);
            if(seh||!accepted)return fail(error,"Party original Toon reciprocal PropertyLoad failed SEH="+std::to_string(seh));
            seh=property_resolve_call(image_+0xe091e0,reinterpret_cast<void*>(property),reinterpret_cast<void*>(toon),&resource->context);
            if(seh)return fail(error,"Party original Toon reciprocal PropertyResolve failed SEH="+std::to_string(seh));
            uintptr_t actual=0;if(!field(toon,0x950,actual,error))return false;
            if(actual!=resource->party)return fail(error,"Party original Toon reciprocal property did not resolve the actual new Party");
        }
        resource->context_destroy_attempted=true;++receipt.calls.context;
        if(!decode::context_call(reinterpret_cast<decode::Unary>(image_+0x89e480),&resource->context,call,error))return false;
        resource->context_destroyed=true;
        const auto lookup_receipt=links.receipt();if(lookup_receipt.failed)return fail(error,lookup_receipt.cause);
        links.clear();
        born.pointer=resource->party;
        if(!identity(image_,born.pointer,born.uid,error)||!field(born.pointer,0x170,born.shard,error)||
           !field(born.pointer,0x178,born.region,error)||!field(born.pointer,0x620,born.manager_region,error)||!owner(error))return false;
        if(existing&&(born.pointer!=existing->pointer||born.uid!=existing->uid))return fail(error,"Party original state load changed its existing native lifetime");
        if(defer_init)return true;
        return finish_membership(born,target,refs,born,receipt,error);
    };
    try{ok=execute();}catch(const std::exception& e){error=std::string("Party original birth C++: ")+e.what();}
    catch(...){error="Party original birth unknown C++ exception";}
    std::string cause=error;
    if(resource->context_constructed&&!resource->context_destroyed&&!resource->context_destroy_attempted){
        local::CallReceipt abort,destroy;std::string cleanup;
        const bool aborted=decode::context_call(reinterpret_cast<decode::Unary>(image_+0x89dfb0),&resource->context,abort,cleanup);
        if(!aborted){keep=true;cause+="; Party context abort failed: "+cleanup;}
        else{resource->context_destroy_attempted=true;
            if(!decode::context_call(reinterpret_cast<decode::Unary>(image_+0x89e480),&resource->context,destroy,cleanup)){
                keep=true;cause+="; Party context cleanup failed: "+cleanup;
            }else resource->context_destroyed=true;
        }
    }
    if(resource->context_destroy_attempted&&!resource->context_destroyed)keep=true;
    links.clear();active=false;
    for(auto pair:{std::pair{&resource->reciprocal,&resource->reciprocal_decoded},std::pair{&resource->record,&resource->decoded}}){
        if(!pair.first->root&&pair.first->detached.empty())continue;
        std::string cleanup;if(!decode::destroy_records(api,*pair.first,*pair.second,cleanup)){keep=true;ok=false;cause+="; Party Record cleanup failed: "+cleanup;}
    }
    if(resource->decoded.unknown||resource->reciprocal_decoded.unknown){receipt.attempted=true;keep=true;ok=false;}
    if(!ok&&receipt.attempted&&(!resource->context_destroyed||resource->party))keep=true;
    if(keep)retained.push_back(std::move(resource));
    if(!ok){receipt.unknown=receipt.attempted;receipt.cause=cause;return fail(error,cause.empty()?"Party original birth failed without a returned complete native receipt":cause);}
    receipt.returned=true;error.clear();return true;
}catch(const std::exception& e){active=false;links.clear();receipt.unknown=receipt.attempted;return fail(error,std::string("Party native birth boundary: ")+e.what());}
catch(...){active=false;links.clear();receipt.unknown=receipt.attempted;return fail(error,"Party native birth boundary: unknown C++ exception");}
bool NativePort::finish_membership(const party_live::Member& original,const party_live::Record& target,const party_live::References& refs,
    party_live::Member& born,party_live::Receipt& receipt,std::string& error){
    if(!held_||!owner(error))return fail(error,error.empty()?"Party final native membership requires its held actual transaction":error);
    const auto pointer=original.pointer;const auto uid=original.uid;uint64_t registered_uid=0;
    if(!typed(image_,pointer,dungeon_party_census::party_ap,error)||!identity(image_,pointer,registered_uid,error))return false;
    if(registered_uid!=uid)return fail(error,"Party final native membership lost its prepared original full UID");
    std::vector<const dungeon_party_census::Pool*> desired;
    for(const auto& manager:target.managers){uintptr_t region=0;
        if(manager.removal||manager.release)return fail(error,"Party final manager still has pending native work");
        if(!refs.resolve(manager.region,region,error))return false;
        const auto pool=std::find_if(current_.pools.begin(),current_.pools.end(),[&](const auto& row){return row.region==region;});
        if(pool==current_.pools.end()||!dungeon_party_census::settled(*pool))return fail(error,"Party final current manager is absent or not settled");
        desired.push_back(&*pool);
    }
    uintptr_t desired_owner=0;
    if(!target.manager_region.empty()&&!refs.resolve(target.manager_region,desired_owner,error))return false;
    uint32_t seh=0;local::CallReceipt call;
        uintptr_t old_owner=0;
        if(!field(pointer,0x620,old_owner,error))return false;
        if(desired_owner&&old_owner!=desired_owner){
            seh=load_call(image_+0x3ccdb0,reinterpret_cast<void*>(pointer),reinterpret_cast<void*>(old_owner),reinterpret_cast<void*>(desired_owner));
            if(seh)return fail(error,"Party original NotifyRegionChanged failed SEH="+std::to_string(seh));
        }
        if(!decode::context_call(reinterpret_cast<decode::Unary>(image_+0x3d3550),reinterpret_cast<void*>(pointer),call,error))return false;
        // InitMapData performs the original owner/advertiser registration. The
        // target may also retain a second native manager during a transfer.
        party_live::Raw actual;
        if(!party_saved_identity_runtime::read_current_raw(actual,error))return false;
        for(const auto& pool:actual.pools){
            const bool contains=std::find(pool.parties.members.begin(),pool.parties.members.end(),pointer)!=pool.parties.members.end();
            const bool wanted=std::any_of(desired.begin(),desired.end(),[&](const auto* row){return row->manager==pool.manager;});
            if(contains==wanted)continue;
            if(!dungeon_party_census::settled(pool))return fail(error,"Party manager changed to active work during original state load");
            if(wanted){seh=manager_call(image_+0x2b41a0,reinterpret_cast<void*>(pool.manager),reinterpret_cast<void*>(pointer));}
            else{++receipt.calls.release;seh=manager_call(image_+0x2b3060,reinterpret_cast<void*>(pool.manager),reinterpret_cast<void*>(pointer));}
            if(seh)return fail(error,"Party original manager membership reconciliation failed SEH="+std::to_string(seh));
        }
        born.pointer=pointer;
        if(!identity(image_,born.pointer,born.uid,error)||!field(born.pointer,0x170,born.shard,error)||
           !field(born.pointer,0x178,born.region,error)||!field(born.pointer,0x620,born.manager_region,error)||!owner(error))return false;
        return born.pointer==pointer&&born.uid==uid||fail(error,"Party final original InitMapData changed its native lifetime");
}
bool NativePort::birth(const party_live::Record& target,const party_live::References& refs,party_live::Member& born,party_live::Receipt& receipt,std::string& error){
    const auto pending=reservations_.find(target.key);
    if(pending==reservations_.end())return load_record(nullptr,target,refs,born,receipt,error);
    if(!held_||pending->second.scope!=current_.scope||!owner(error))return fail(error,error.empty()?"Party pending birth lost its held native owner":error);
    const auto original=pending->second.member;
    receipt.reserved=true;
    const auto staged=prepared_.find(target.key);
    if(staged!=prepared_.end()){
        if(staged->second.scope!=current_.scope||staged->second.member.pointer!=original.pointer||staged->second.member.uid!=original.uid||
           !party_live::validate(target,error)||!same_target(staged->second.target,target))
            return fail(error,error.empty()?"Party prepared birth differs from the original pending target":error);
        receipt.attempted=true;
        if(!ready_to_init(staged->second.member,target,refs,error)||!finish_membership(staged->second.member,target,refs,born,receipt,error)){
            receipt.unknown=true;receipt.cause=error;batch_unknown_=error;return false;
        }
        prepared_.erase(staged);
    }else if(!load_record(&original,target,refs,born,receipt,error))return false;
    reservations_.erase(pending);receipt.returned=true;return true;
}
bool NativePort::adopt(const party_live::Member& existing,const party_live::Record& target,const party_live::References& refs,party_live::Receipt& receipt,std::string& error){
    party_live::Member actual;const auto staged=prepared_.find(target.key);
    if(staged==prepared_.end())return load_record(&existing,target,refs,actual,receipt,error);
    if(staged->second.scope!=current_.scope||staged->second.member.pointer!=existing.pointer||staged->second.member.uid!=existing.uid||
       !party_live::validate(target,error)||!same_target(staged->second.target,target))
        return fail(error,error.empty()?"Party prepared current state differs from its original target/lifetime":error);
    receipt.attempted=true;
    if(!ready_to_init(existing,target,refs,error)||!finish_membership(existing,target,refs,actual,receipt,error)){
        receipt.unknown=true;receipt.cause=error;batch_unknown_=error;return false;
    }
    prepared_.erase(staged);receipt.returned=true;return true;
}
bool NativePort::prepared(const std::string& key)const{
    const auto found=prepared_.find(key);return found!=prepared_.end()&&found->second.scope==current_.scope;
}
bool NativePort::ready_to_init(const party_live::Member& party,const party_live::Record& target,const party_live::References& refs,std::string& error){
    if(!held_||!refs.resolve||!owner(error))return fail(error,error.empty()?"Party prepared Init requires its held actual reference owner":error);
    std::vector<uintptr_t> toons;
    if(!members(image_,party.pointer,toons,error))return false;
    if(toons.size()!=target.toons.size())return fail(error,"Party prepared members changed before deferred InitMapData");
    for(size_t i=0;i<toons.size();++i){uintptr_t expected=0,backlink=0,region=0,subscriber=0,main=0;
        if(!refs.resolve(target.toons[i],expected,error)||!field(toons[i],0x950,backlink,error)||
           !field(toons[i],0x178,region,error)||!field(toons[i],0x940,subscriber,error))return false;
        if(toons[i]!=expected||backlink!=party.pointer)return fail(error,"Party prepared Toon order/reciprocal differs before deferred InitMapData");
        // The original transform callback reads Region and Subscriber fields.
        // Their saved IDs must have passed through Subscriber ObjectContext D1.
        if(!typed(image_,region,dungeon_party_census::region_ap,error)||!typed(image_,subscriber,0x16e75f0,error)||
           !field(subscriber,0x80,main,error))return false;
        if(main!=toons[i])return fail(error,"Party prepared Toon Subscriber reciprocal is unresolved before deferred InitMapData");
    }
    return true;
}
bool NativePort::prepare_record(const party_live::Member& existing,const party_live::Record& target,const party_live::References& refs,
    party_live::Receipt& receipt,std::string& error)try{
    receipt={};
    if(!batch_unknown_.empty())return fail(error,batch_unknown_);
    if(!held_||context_.role!=action_session::Role::Client||!party_live::validate(target,error)||!owner(error))return fail(error,error.empty()?"Party pre-Resolve prepare requires its held actual Client transaction":error);
    const auto staged=prepared_.find(target.key);
    if(staged!=prepared_.end()){
        if(staged->second.scope!=current_.scope||staged->second.member.pointer!=existing.pointer||staged->second.member.uid!=existing.uid||
           !same_target(staged->second.target,target))return fail(error,"Party pre-Resolve prepare has another actual target/lifetime");
        uint64_t uid=0;
        if(!identity(image_,existing.pointer,uid,error))return false;
        if(uid!=existing.uid)return fail(error,"Party repeated prepare lost its actual full UID");
        receipt.member=staged->second.member;receipt.existing=true;receipt.returned=true;return true;
    }
    const auto pending=reservations_.find(target.key);party_identity::Entry confirmed;
    if(pending!=reservations_.end()){
        if(pending->second.scope!=current_.scope||pending->second.member.pointer!=existing.pointer||pending->second.member.uid!=existing.uid)
            return fail(error,"Party pre-Resolve prepare lost its private original factory reservation");
    }else if(!party_identity::registry.lookup(current_.scope,target.key,confirmed,error)||confirmed.pointer!=existing.pointer||confirmed.uid!=existing.uid)
        return fail(error,error.empty()?"Party pre-Resolve prepare has no exact existing canonical lifetime":error);
    party_live::Member loaded;
    if(!load_record(&existing,target,refs,loaded,receipt,error,true)){
        if(receipt.attempted){receipt.unknown=true;receipt.cause=error;batch_unknown_=error.empty()?"Party original pre-Resolve load outcome unknown":error;}
        return false;
    }
    prepared_.emplace(target.key,Prepared{current_.scope,loaded,target});
    if(pending!=reservations_.end())pending->second.member=loaded;
    receipt.member=loaded;receipt.returned=true;return true;
}catch(const std::exception& e){receipt.unknown=receipt.attempted;const auto cause=std::string("Party original pre-Resolve prepare C++: ")+e.what();
    if(receipt.unknown)batch_unknown_=cause;return fail(error,cause);}
catch(...){receipt.unknown=receipt.attempted;const std::string cause="Party original pre-Resolve prepare unknown C++ exception";
    if(receipt.unknown)batch_unknown_=cause;return fail(error,cause);}
bool NativePort::prepare_reserved(const party_live::Record& target,const party_live::References& refs,party_live::Receipt& receipt,std::string& error){
    party_live::Member actual;if(!reserved(target.key,actual,error))return false;
    return prepare_record(actual,target,refs,receipt,error);
}
bool NativePort::reserved(const std::string& key,party_live::Member& member,std::string& error){
    member={};
    if(!held_||context_.role!=action_session::Role::Client||!owner(error))return fail(error,error.empty()?"Party pending factory lookup requires its held actual Client transaction":error);
    const auto found=reservations_.find(key);
    if(found==reservations_.end()||found->second.scope!=current_.scope)return fail(error,"Party key has no actual factory reservation in this native transaction");
    uint64_t uid=0;
    if(!typed(image_,found->second.member.pointer,dungeon_party_census::party_ap,error)||!identity(image_,found->second.member.pointer,uid,error))return false;
    if(uid!=found->second.member.uid)return fail(error,"Party pending factory reservation changed native full UID");
    member=found->second.member;return true;
}
bool NativePort::reserve(const std::string& key,party_live::Member& member,party_live::Receipt& receipt,std::string& error)try{
    member={};receipt={};
    if(!batch_unknown_.empty())return fail(error,batch_unknown_);
    if(!held_||context_.role!=action_session::Role::Client||!party_identity::valid_key(key)||!owner(error))
        return fail(error,error.empty()?"Party original factory reservation requires its held actual Client transaction":error);
    const auto found=reservations_.find(key);
    if(found!=reservations_.end()){
        if(!reserved(key,member,error))return false;
        receipt.member=member;receipt.existing=true;receipt.returned=true;return true;
    }
    receipt.attempted=true;++receipt.calls.factory;
    uintptr_t actual=0;const auto seh=factory_call(image_+0xd96cc0,&actual);
    if(seh||!actual){receipt.unknown=true;batch_unknown_="Party original factory reservation failed SEH="+std::to_string(seh);return fail(error,batch_unknown_);}
    uint64_t uid=0;
    if(!typed(image_,actual,dungeon_party_census::party_ap,error)||!identity(image_,actual,uid,error)){
        receipt.unknown=true;batch_unknown_=error;return false;
    }
    member={actual,uid,0,0,0};
    reservations_.emplace(key,party_live::Reservation{current_.scope,key,member});
    receipt.member=member;receipt.returned=true;return true;
}catch(const std::exception& e){receipt.unknown=receipt.attempted;const auto cause=std::string("Party original factory reservation C++: ")+e.what();
    if(receipt.unknown)batch_unknown_=cause;return fail(error,cause);}
catch(...){receipt.unknown=receipt.attempted;const std::string cause="Party original factory reservation unknown C++ exception";
    if(receipt.unknown)batch_unknown_=cause;return fail(error,cause);}
bool NativePort::rearm_toon_party(uintptr_t toon,uint64_t uid,int32_t saved_id,void* context,party_live::Receipt& receipt,std::string& error)try{
    receipt={};
    if(!batch_unknown_.empty())return fail(error,batch_unknown_);
    if(!held_||context_.role!=action_session::Role::Client||!context||saved_id<0||!uid||!owner(error))
        return fail(error,error.empty()?"Party Toon reciprocal re-arm requires its held actual Client context and nonnegative saved ID":error);
    uint64_t actual_uid=0;
    if(!typed(image_,toon,0x1744e90,error)||!identity(image_,toon,actual_uid,error))return false;
    if(actual_uid!=uid)return fail(error,"Party Toon reciprocal re-arm lost the actual pending Toon full UID");
    if(!saved_id){uintptr_t path=0,task=0;
        if(!field(toon,0x2e8,path,error)||!field(toon,0x308,task,error))return false;
        if(path||task)return fail(error,"Pending Toon has nonempty native path/task state before Party PostResolve");
        uintptr_t controlled=0;
        if(!field(image_+0x1744e90,0x3f8,controlled,error))return false;
        if(controlled!=image_+0x39aa80)return fail(error,"Pending Toon original Party-controlled virtual target differs");
    }
    const auto property=image_+party_live_codes::toon_party_property;uintptr_t ap=0,offset=0;
    if(!field(property,0,ap,error)||!field(property,0x38,offset,error))return false;
    if(ap!=image_+party_live_codes::toon_party_property_ap||offset!=0x950)return fail(error,"Party Toon reciprocal re-arm original property differs");
    decode::DecodeApi api;auto resource=std::make_unique<Resource>();bool ok=false;
    const auto apply=[&](){
        if(!decode::resolve_record_api(memory,image_,api,error)||
           !decode::decode_integer_record(api,memory,"party",saved_id,resource->reciprocal,resource->reciprocal_decoded,error))return false;
        receipt.attempted=true;++receipt.calls.toon_bind;bool accepted=false;
        const auto seh=property_call(image_+0xe090d0,reinterpret_cast<void*>(property),reinterpret_cast<void*>(toon),reinterpret_cast<void*>(resource->reciprocal.root),context,&accepted);
        if(seh||!accepted)return fail(error,"Party original pending Toon PropertyLoad re-arm failed SEH="+std::to_string(seh)+" accepted="+std::to_string(accepted));
        uint64_t stored=0;
        if(!field(toon,0x950,stored,error)||!owner(error))return false;
        return stored==uint64_t(saved_id)||fail(error,"Party original pending Toon PropertyLoad did not retain its exact original saved ID");
    };
    try{ok=apply();}catch(const std::exception& e){error=std::string("Party pending Toon PropertyLoad C++: ")+e.what();}
    catch(...){error="Party pending Toon PropertyLoad unknown C++ exception";}
    std::string cause=error;bool keep=resource->reciprocal_decoded.unknown;
    if(resource->reciprocal.root||!resource->reciprocal.detached.empty()){
        std::string cleanup;
        if(!decode::destroy_records(api,resource->reciprocal,resource->reciprocal_decoded,cleanup)){
            ok=false;keep=true;if(!cause.empty())cause+="; ";cause+="Party re-arm Record cleanup failed: "+cleanup;
        }
    }
    if(keep)retained.push_back(std::move(resource));
    if(!ok){receipt.unknown=receipt.attempted||keep;receipt.cause=cause;if(receipt.unknown)batch_unknown_=cause;return fail(error,cause);}
    receipt.returned=true;error.clear();return true;
}catch(const std::exception& e){receipt.unknown=receipt.attempted;const auto cause=std::string("Party pending Toon re-arm C++: ")+e.what();
    if(receipt.unknown)batch_unknown_=cause;return fail(error,cause);}
catch(...){receipt.unknown=receipt.attempted;const std::string cause="Party pending Toon re-arm unknown C++ exception";
    if(receipt.unknown)batch_unknown_=cause;return fail(error,cause);}
bool NativePort::remove(const party_live::Raw& before,const party_live::Member& member,party_live::Receipt& receipt,std::string& error)try{
    if(!held_||context_.role!=action_session::Role::Client||!owner(error))return fail(error,error.empty()?"Party original removal requires held actual Client boundary":error);
    if(before.scope!=current_.scope||before.members!=current_.members||!member.manager_region)
        return fail(error,"Party original removal lost complete current membership/owner Region");
    uintptr_t deleting_manager=0;std::vector<uintptr_t> release;
    for(const auto& pool:before.pools)if(std::find(pool.parties.members.begin(),pool.parties.members.end(),member.pointer)!=pool.parties.members.end()){
        if(!dungeon_party_census::settled(pool))return fail(error,"Party original removal manager has active native update/deletion/release work");
        if(pool.region==member.manager_region)deleting_manager=pool.manager;else release.push_back(pool.manager);
    }
    if(!deleting_manager)return fail(error,"Party original removal has no exact actual owning manager");
    std::vector<uintptr_t> toons;if(!members(image_,member.pointer,toons,error))return false;
    for(const auto toon:toons){uintptr_t reciprocal=0;if(!field(toon,0x950,reciprocal,error))return false;
        if(reciprocal!=member.pointer)return fail(error,"Party original removal Toon member has another current reciprocal Party");}
    receipt.attempted=true;
    for(const auto manager:release){++receipt.calls.release;
        const auto seh=manager_call(image_+0x2b3060,reinterpret_cast<void*>(manager),reinterpret_cast<void*>(member.pointer));
        if(seh)return fail(error,"Party original transfer ReleaseParty failed SEH="+std::to_string(seh));}
    ++receipt.calls.remove;
    const auto seh=manager_call(image_+0x2b3820,reinterpret_cast<void*>(deleting_manager),reinterpret_cast<void*>(member.pointer));
    if(seh)return fail(error,"Party original owning RemoveParty failed SEH="+std::to_string(seh));
    // RemoveParty performs the one original deleting destructor. Its Party and
    // member pointers must not be dereferenced after this call.
    receipt.returned=true;return owner(error);
}catch(const std::exception& e){receipt.unknown=receipt.attempted;return fail(error,std::string("Party original removal C++: ")+e.what());}
catch(...){receipt.unknown=receipt.attempted;return fail(error,"Party original removal unknown C++ exception");}
}

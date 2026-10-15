#include "runtime_internal.h"
#include "building_capture_identity.h"
#include "native_open_runtime.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include "fingerprints.h"
#include <Windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <array>
#include <atomic>
namespace mt2mp::game::bug_authority {
namespace {
using Property=bool(*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
using Save=void(*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
using RecordLoad=void(*)(uintptr_t,uintptr_t,uintptr_t);
using Random=uintptr_t(*)(uintptr_t);
using Specific=uintptr_t(*)(uintptr_t,int,float);
using Remove=void(*)(uintptr_t,uintptr_t);
Property original_save=nullptr,original_load=nullptr;
Save original_rtti=nullptr;RecordLoad original_record=nullptr;
Random original_random=nullptr;Specific original_specific=nullptr;
Remove original_remove=nullptr;creation::SetAnchor original_anchor=nullptr;
thread_local bool replica_birth=false;
struct Scope {Children children;bool save=false,record=false;Scope* previous=nullptr;};
thread_local Scope* scope=nullptr;
struct Scoped {Scope value;Scoped(){value.previous=scope;scope=&value;}~Scoped(){scope=value.previous;}};
struct Hook {uintptr_t rva=0,detour=0,trampoline=0;size_t size=0;std::vector<unsigned char> installed;std::array<unsigned char,64> relocated{};};
std::vector<Hook> hooks;
void caught(const std::string& cause){std::string error;reject(cause,error);}
bool world_collection(uintptr_t collection,std::string& error){
    auto& r=runtime();uintptr_t parent=0,shard=0,ap=0;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,error)||!field(collection,0,ap,error)||!field(collection,0x48,parent,error)||!field(parent,0x170,shard,error))return false;
    return (ap==r.image+0x1705640&&world.world_epoch==r.binding.native_epoch&&world.generation==r.binding.native_generation&&
        shard==reinterpret_cast<uintptr_t>(world.map)+0x1c0)||fail(error,"Bug creation receiver is outside exact current root World");
}
uintptr_t add_random(uintptr_t collection){
    try{
        auto& r=runtime();std::lock_guard lock(r.gate);Context context;Binding binding;uint64_t publication=0;bool active=false;std::string error;
        if(!active_world(context,binding,publication,active,error)){caught(error);return original_random(collection);}
        if(!active)return original_random(collection);
        if(!world_collection(collection,error)){caught(error);return original_random(collection);}
        if(context.role==action_session::Role::Client)return 0;
        const auto result=original_random(collection);
        if(result&&!witness_birth(collection,result,1,error))caught(error);
        return result;
    }catch(const std::exception& error){caught(std::string("Original AddRandomBug exception: ")+error.what());throw;}
    catch(...){caught("Original AddRandomBug nonstandard exception");throw;}
}
uintptr_t add_specific(uintptr_t collection,int type,float reproduction){
    const auto result=original_specific(collection,type,reproduction);
    if(!replica_birth){try{auto& r=runtime();std::lock_guard lock(r.gate);std::string error;
        if(result&&!r.snapshot_armed&&!witness_birth(collection,result,2,error))caught(error);
    }catch(const std::exception& error){caught(std::string("Original specific Bug birth: ")+error.what());}
    catch(...){caught("Original specific Bug birth nonstandard exception");}}
    return result;
}
void set_anchor(uintptr_t bug,uintptr_t parent){
    if(creation::NativeServices::dispatch_anchor(bug,parent,reinterpret_cast<uintptr_t>(_ReturnAddress())))return;
    original_anchor(bug,parent);
}
void remove_tracker(uintptr_t tracker,uintptr_t bug){
    original_remove(tracker,bug);
    try{auto& r=runtime();std::lock_guard lock(r.gate);std::string error;
        if(!witness_removal(tracker,bug,error))caught(error);
    }catch(const std::exception& error){caught(std::string("Original Bug removal witness: ")+error.what());}
    catch(...){caught("Original Bug removal witness nonstandard exception");}
}
bool property_owner(uintptr_t property,uintptr_t collection,std::string& error){
    uint64_t offset=0;uintptr_t ap=0;
    if(!field(property,0x38,offset,error)||!field(collection,0,ap,error))return false;
    return (offset==0x18&&ap==runtime().image+0x1705640)||fail(error,"Bug original serde property/collection offset differs");
}
bool retain(Scope& frame,std::string& error){
    std::vector<uintptr_t> after;if(!local_members(frame.children.collection,after,error))return false;
    if(frame.record)frame.children.declared=static_cast<int32_t>(frame.children.witnessed.size());
    if(!frame.children.finish(after,error))return false;
    auto& r=runtime();
    if(r.snapshot_armed){
        if(std::any_of(r.saved.begin(),r.saved.end(),[&](const SavedChildren& x){return x.collection==frame.children.collection;}))return fail(error,"Bug collection serialized twice in owned snapshot");
        r.saved.push_back({frame.children.collection,std::move(after)});return true;
    }
    for(auto bug:after)if(!witness_birth(frame.children.collection,bug,3,error))return false;
    return true;
}
bool save_property(uintptr_t property,uintptr_t collection,uintptr_t writer,uintptr_t context){
    auto& r=runtime();bool owned=false;building_capture_identity::StreamOwner owner;std::string error;
    if(!r.snapshot_armed||!r.snapshot_host)return original_save(property,collection,writer,context);
    if(!building_capture_identity::stream_owner(reinterpret_cast<void*>(writer),owner,owned,error)){caught(error);return original_save(property,collection,writer,context);}
    if(!owned)return original_save(property,collection,writer,context);
    Scoped active;active.value.save=true;auto& frame=active.value.children;frame.collection=collection;frame.stream=writer;frame.context=context;
    std::vector<uintptr_t> before;
    {std::lock_guard lock(r.gate);if(owner.capture_uuid!=r.snapshot_uuid||!property_owner(property,collection,error)||!local_members(collection,before,error)){caught(error.empty()?"Bug snapshot writer UUID differs":error);}}
    frame.declared=static_cast<int32_t>(before.size());
    bool result=false;
    try{result=original_save(property,collection,writer,context);}
    catch(const std::exception& e){caught(std::string("Original Bug SaveStream: ")+e.what());throw;}
    catch(...){caught("Original Bug SaveStream nonstandard exception");throw;}
    {std::lock_guard lock(r.gate);if(!result||!retain(active.value,error))caught(error.empty()?"Original Bug SaveStream returned false":error);}
    return result;
}
void saved_child(uintptr_t caller,uintptr_t object,uintptr_t writer,uintptr_t context){
    if(scope&&scope->save&&caller==0xdc19e1){
        try{std::lock_guard lock(runtime().gate);std::string error;uintptr_t ap=0;
            if(!field(object,0,ap,error)||ap!=runtime().image+0x1742200||!scope->children.child(writer,context,object,error))caught(error.empty()?"Bug original serialized child class differs":error);
        }catch(const std::exception& e){caught(std::string("Bug original serialized child witness: ")+e.what());}
        catch(...){caught("Bug original serialized child witness nonstandard exception");}
    }
}
void save_rtti(uintptr_t type,uintptr_t object,uintptr_t writer,uintptr_t context){
    saved_child(reinterpret_cast<uintptr_t>(_ReturnAddress())-runtime().image,object,writer,context);
    original_rtti(type,object,writer,context);
}
bool load_property(uintptr_t property,uintptr_t collection,uintptr_t reader,uintptr_t context){
    auto& r=runtime();if(!r.snapshot_armed||r.snapshot_host)return original_load(property,collection,reader,context);
    Scoped active;auto& frame=active.value.children;frame.collection=collection;frame.stream=reader;frame.context=context;
    std::string error;std::vector<uintptr_t> before;
    {std::lock_guard lock(r.gate);if(!property_owner(property,collection,error)||!local_members(collection,before,error)||!before.empty())caught(error.empty()?"Bug original snapshot load requires empty collection":error);}
    bool result=false;
    try{result=original_load(property,collection,reader,context);}
    catch(const std::exception& e){caught(std::string("Original Bug LoadStream: ")+e.what());throw;}
    catch(...){caught("Original Bug LoadStream nonstandard exception");throw;}
    {std::lock_guard lock(r.gate);if(!result||!retain(active.value,error))caught(error.empty()?"Original Bug LoadStream returned false":error);}
    return result;
}
void load_record(uintptr_t array,uintptr_t record,uintptr_t context){
    auto& r=runtime();if(r.action.empty()){original_record(array,record,context);return;}
    Scoped active;active.value.record=true;auto& frame=active.value.children;frame.collection=array-0x18;frame.stream=record;frame.context=context;frame.declared=INT32_MAX;
    try{original_record(array,record,context);}
    catch(const std::exception& e){caught(std::string("Original Bug history LoadRecord: ")+e.what());throw;}
    catch(...){caught("Original Bug history LoadRecord nonstandard exception");throw;}
    std::lock_guard lock(r.gate);std::string error;if(!retain(active.value,error))caught(error);
}
bool remove_call(uintptr_t entry,uintptr_t collection,uintptr_t bug,uint32_t& code){
    __try{reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(entry)(collection,bug);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();return false;}
}
}
void original_children(uintptr_t caller,void* reader,int32_t count)noexcept try{
    if(!scope||scope->save||scope->record||caller!=0xbae563)return;
    std::lock_guard lock(runtime().gate);std::string error;
    if(scope->children.stream!=reinterpret_cast<uintptr_t>(reader)||scope->children.declared!=-1||count<0||count>1024*1024){caught("Bug original BeginChildren reader/count differs");return;}
    scope->children.declared=count;
}catch(const std::exception& e){caught(std::string("Bug original child count: ")+e.what());}catch(...){caught("Bug original child count nonstandard exception");}
void original_registered(uintptr_t caller,void* context,int32_t id,void* object)noexcept try{
    if(!scope||scope->save)return;
    const bool linked=scope->record?caller==0xbae1df:caller==0xbae8b5;
    const bool inline_child=scope->record?caller==0xbae470:caller==0xbaea15;
    if(!linked&&!inline_child)return;
    std::lock_guard lock(runtime().gate);std::string error;uintptr_t ap=0,actual_context=0;const auto ptr=reinterpret_cast<uintptr_t>(object);
    if((linked?id<=0:id!=0)||!field(ptr,0,ap,error)||!field(ptr,8,actual_context,error)||ap!=runtime().image+0x1742200||actual_context!=reinterpret_cast<uintptr_t>(context)||
       !scope->children.child(scope->children.stream,reinterpret_cast<uintptr_t>(context),ptr,error))caught(error.empty()?"Bug original child registration class/context/link differs":error);
}catch(const std::exception& e){caught(std::string("Bug original registration: ")+e.what());}catch(...){caught("Bug original registration nonstandard exception");}
creation::SetAnchor anchor_original(){return original_anchor;}
bool native_remove(uintptr_t collection,uintptr_t bug,bool& attempted,std::string& error){
    attempted=false;
    std::vector<uintptr_t> before,after;if(!local_members(collection,before,error))return false;
    const auto found=std::find(before.begin(),before.end(),bug);if(found==before.end())return fail(error,"Selected Bug retirement lacks original collection member");before.erase(found);
    uint32_t code=0;attempted=true;if(!remove_call(runtime().image+0x59970,collection,bug,code))return fail(error,"Original selected Bug retirement SEH="+std::to_string(code));
    if(!local_members(collection,after,error))return false;
    return (before==after&&!runtime().lifetimes.pointer(bug))||fail(error,"Selected Bug retirement order/Tracker lifetime readback differs");
}
bool native_code(std::string& error){
    auto& r=runtime();if(!r.ready)return fail(error,"Bug native hooks are not published");
    for(const auto& hook:hooks){std::vector<unsigned char> bytes(hook.size);std::array<unsigned char,64> relocated{};
        if(!r.memory.copy(r.image+hook.rva,bytes.data(),bytes.size(),error)||!r.memory.copy(hook.trampoline,relocated.data(),relocated.size(),error))return false;
        if(bytes!=hook.installed||relocated!=hook.relocated)return fail(error,"Bug installed native hook owner/body/trampoline changed RVA="+std::to_string(hook.rva));}
    for(const auto& body:original_bodies){if(std::any_of(hooks.begin(),hooks.end(),[&](const Hook& h){return h.rva==body.rva;}))continue;
        std::vector<unsigned char> bytes(body.size);if(!r.memory.copy(r.image+body.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=body.sha)return fail(error,"Bug original complete native body changed RVA="+std::to_string(body.rva));}
    return true;
}
bool install_native(std::string& error){
    auto& r=runtime();if(!hooks.empty())return fail(error,"Bug native hook installation already attempted");
    for(const auto& body:original_bodies){std::vector<unsigned char> bytes(body.size);if(!r.memory.copy(r.image+body.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=body.sha)return fail(error,"Bug original complete body unsupported RVA="+std::to_string(body.rva));}
    struct Install {uintptr_t rva,detour;void** original;};
    const Install entries[]={
        {0x59fb0,reinterpret_cast<uintptr_t>(&add_random),reinterpret_cast<void**>(&original_random)},
        {0x59a00,reinterpret_cast<uintptr_t>(&add_specific),reinterpret_cast<void**>(&original_specific)},
        {0x58740,reinterpret_cast<uintptr_t>(&set_anchor),reinterpret_cast<void**>(&original_anchor)},
        {0x5d100,reinterpret_cast<uintptr_t>(&remove_tracker),reinterpret_cast<void**>(&original_remove)},
        {0xdc1800,reinterpret_cast<uintptr_t>(&save_property),reinterpret_cast<void**>(&original_save)},
        {0xdc17d0,reinterpret_cast<uintptr_t>(&load_property),reinterpret_cast<void**>(&original_load)},
        {0x8acc80,reinterpret_cast<uintptr_t>(&save_rtti),reinterpret_cast<void**>(&original_rtti)},
        {0xbaded0,reinterpret_cast<uintptr_t>(&load_record),reinterpret_cast<void**>(&original_record)}};
    for(const auto& entry:entries){const auto body=std::find_if(original_bodies.begin(),original_bodies.end(),[&](const auto& b){return b.rva==entry.rva;});
        if(body==original_bodies.end())return fail(error,"Bug hook full-body metadata absent");
        const auto target=reinterpret_cast<void*>(r.image+entry.rva);auto result=MH_CreateHook(target,reinterpret_cast<void*>(entry.detour),entry.original);
        if(result!=MH_OK)return fail(error,"Bug MH_CreateHook RVA="+std::to_string(entry.rva)+" status="+std::to_string(result));
        result=MH_EnableHook(target);if(result!=MH_OK)return fail(error,"Bug MH_EnableHook RVA="+std::to_string(entry.rva)+" status="+std::to_string(result));
        Hook owned{entry.rva,entry.detour,reinterpret_cast<uintptr_t>(*entry.original),body->size};owned.installed.resize(body->size);
        if(!r.memory.copy(r.image+entry.rva,owned.installed.data(),owned.installed.size(),error)||!r.memory.copy(owned.trampoline,owned.relocated.data(),owned.relocated.size(),error))return false;
        hooks.push_back(std::move(owned));
    }
    r.ready=true;return native_code(error);
}
// Only the selected authoritative insertion suppresses its local birth observer.
struct ReplicaBirthGuard {ReplicaBirthGuard(){replica_birth=true;}~ReplicaBirthGuard(){replica_birth=false;}};
bool create_replica(creation::NativeServices& services,const creation::Lease& owner,const creation::Value& value,creation::Receipt& receipt){
    if(replica_birth){receipt.cause="Bug replica birth scope nested";return false;}ReplicaBirthGuard guard;return creation::create(services,owner,value,receipt);
}
}

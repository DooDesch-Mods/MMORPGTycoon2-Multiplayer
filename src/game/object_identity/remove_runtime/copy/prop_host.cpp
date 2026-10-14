#include "prop_host.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <exception>
#include <limits>

namespace mt2mp::prop_lease {
namespace {
bool equal_api(const Api& a,const Api& b){return a.create==b.create&&a.assign==b.assign&&a.destroy==b.destroy&&a.link_addresspoint==b.link_addresspoint&&a.weak_addresspoint==b.weak_addresspoint;}
bool same_binding(const PrivateLease& a,const PrivateLease& b){return a.holder==b.holder&&a.object==b.object&&a.uid==b.uid&&a.world==b.world&&a.generation==b.generation&&a.class_addresspoint==b.class_addresspoint;}
struct Busy {bool& flag;explicit Busy(bool& value):flag(value){flag=true;}~Busy(){flag=false;}};
}
bool NativeHost::validate_exact_api(const Api& api,std::string& error){
    if(!main_thread_||GetCurrentThreadId()!=main_thread_){error="error: typed Prop API preflight is outside bound Mainthread";return false;}
    if(!equal_api(api,api_)){error="error: typed Prop API differs from bound native contract";return false;}
    return services_.validate_api(api,error);
}
bool NativeHost::current(Proof& out,std::string& error){
    if(!main_thread_||GetCurrentThreadId()!=main_thread_){out={};error="error: typed Prop current proof is outside bound Mainthread";return false;}
    out={};RuntimeProof actual;
    if(!services_.runtime(actual,error))return false;
    if(actual.exe_sha!=native_uid::required_exe_sha){error="error: actual Prop executable SHA differs";return false;}
    if(!actual.actual_entryguards){error="error: actual Prop code/vtable guards absent";return false;}
    if(!actual.actual_crt_receipt){error="error: actual resolved CRT/IAT/module receipt absent";return false;}
    if(!actual.actual_hook_receipt||!observer_.enabled()){error="error: actual typed Freeobserver installation receipt absent";return false;}
    const auto& p=actual.proof;
    if(!p.main_thread){error="error: typed Prop lease is outside Mainthread";return false;}
    if(!p.current_native_open){error="error: typed Prop NativeOpen witness is stale";return false;}
    if(!p.effects_idle){error="info: typed Prop effects are not idle";return false;}
    if(!p.world.epoch||!p.world.origin_generation||!p.world.world||!p.world.map){error="error: typed Prop actual world witness is incomplete";return false;}
    out=p;return true;
}
bool NativeHost::read_prop(void* object,uint64_t uid,Prop& out,std::string& error){if(!main_thread_||GetCurrentThreadId()!=main_thread_){out={};error="error: typed Prop primary reader is outside bound Mainthread";return false;}return services_.read_prop(object,uid,out,error);}
bool NativeHost::reserve_generation(uint64_t& out,std::string& error){
    if(!main_thread_||GetCurrentThreadId()!=main_thread_){out=0;error="error: typed Prop generation reservation is outside bound Mainthread";return false;}
    out=0;uint64_t generation=0;
    if(!services_.reserve_own_holder_generation(generation,error))return false;
    if(!generation||!reserved_.insert(generation).second){error="error: own-holder generation is zero or replayed";return false;}
    out=generation;return true;
}
bool NativeHost::allocation_returned(const PrivateLease& lease,std::string& error){
    if(!main_thread_||GetCurrentThreadId()!=main_thread_){error="error: typed Prop allocation receipt is outside bound Mainthread";return false;}
    if(!lease.holder||!lease.object||!lease.uid||!lease.class_addresspoint||!reserved_.contains(lease.generation)||allocations_.contains(lease.generation)){error="error: own Prop factory-returned allocation binding invalid or replayed";return false;}
    for(const auto& item:allocations_)if(item.second.lease.holder==lease.holder&&!item.second.consumed){error="error: own Prop holder address is already continuously owned";return false;}
    Allocation fresh;fresh.lease=lease;allocations_.emplace(lease.generation,fresh);return true;
}
bool NativeHost::same_allocation(const PrivateLease& lease,std::string& error)const{
    if(!main_thread_||GetCurrentThreadId()!=main_thread_){error="error: typed Prop allocation binding is outside bound Mainthread";return false;}
    const auto it=allocations_.find(lease.generation);
    if(it==allocations_.end()||!same_binding(it->second.lease,lease)){error="error: current own Prop holder generation/UID/world/class binding differs";return false;}
    return true;
}
bool NativeHost::validate_retirement(const PrivateLease& lease,const Retirement& retired,std::string& error){
    if(!same_allocation(lease,error))return false;
    if(retired.old_world!=lease.world||!retired.receipt||retired.snapshot_receipt.empty()){error="error: current own Prop retirement receipt incomplete";return false;}
    return services_.validate_retirement(lease,retired,error);
}
bool NativeHost::invoke_native_delete_once(DeleteFn genuine,const PrivateLease& lease,const Retirement& retired,std::string& error,uint32_t& code){
    code=0;
    if(genuine!=api_.destroy||!validate_exact_api(api_,error)||!same_allocation(lease,error)||!validate_retirement(lease,retired,error))return false;
    Proof actual;if(!current(actual,error))return false;
    auto& allocation=allocations_.at(lease.generation);
    if(allocation.attempted||allocation.consumed){error="error: own Prop D0 invocation already attempted; no retry";return false;}
    uint64_t call=0;if(!services_.next_free_invocation(call,error)||!call){if(error.empty())error="error: actual global Freeobserver invocation ID absent";return false;}
    allocation.permit={call,lease.generation,actual.world.origin_generation,retired.receipt,lease.holder,56,true};
    allocation.retirement=retired;
    if(!observer_.begin(allocation.permit)){error="error: own Prop Freeobserver permit rejected before D0";return false;}
    allocation.attempted=true;
    const bool confirmed=observer_.invoke_delete(genuine,allocation.permit);
    allocation.free=observer_.observe(allocation.permit);
    if(!confirmed){error=allocation.free.cause.data();code=allocation.free.seh_code;return false;}
    return true;
}
bool NativeHost::confirm_native_delete(const PrivateLease& lease,const Retirement& retired,std::string& error){
    if(!same_allocation(lease,error))return false;
    auto& allocation=allocations_.at(lease.generation);const auto& r=allocation.free;
    if(allocation.consumed||!allocation.attempted||allocation.retirement.old_world!=retired.old_world||allocation.retirement.snapshot_receipt!=retired.snapshot_receipt||allocation.retirement.receipt!=retired.receipt||r.phase!=native_free::Phase::Confirmed||r.permit!=allocation.permit||r.permit.holder!=lease.holder||r.permit.allocation_generation!=lease.generation||r.permit.retirement_receipt!=retired.receipt||r.delete_attempted!=1||r.delete_returned!=1||r.sized_attempted!=1||r.sized_returned!=1||r.free_attempted!=1||r.free_returned!=1){error="error: current own Prop actual D0/Sized/Free receipt absent, consumed or mismatched";return false;}
    allocation.consumed=true;return true;
}
native_free::Receipt NativeHost::free_receipt(uint64_t generation)const{const auto it=allocations_.find(generation);return it==allocations_.end()?native_free::Receipt{}:it->second.free;}
bool NativeHost::enumerate_private_allocations(std::vector<PrivateAllocationRecord>& out,std::string& error)const{
    out.clear();error.clear();if(!main_thread_||GetCurrentThreadId()!=main_thread_){error="error: own allocation enumeration requires actual Mainthread";return false;}
    const auto availability=observer_.availability();if(!availability.enabled||availability.active||availability.in_delete||availability.unknown_locked){error="error: own allocation enumeration Freeobserver not quiescent";return false;}
    try{std::vector<PrivateAllocationRecord> snapshot;snapshot.reserve(allocations_.size());for(const auto& [generation,r]:allocations_){if(generation!=r.lease.generation||!reserved_.contains(generation)){error="error: own allocation reservation map differs";return false;}snapshot.push_back({r.lease,r.retirement,r.free,r.attempted,r.consumed});}out=std::move(snapshot);return true;}
    catch(const std::exception& ex){error=std::string("error: own allocation enumeration: ")+ex.what();return false;}catch(...){error="error: own allocation enumeration nonstandard exception";return false;}
}
bool RemoveLeaseServices::same_context(const remove::RemoveContext& a,const remove::RemoveContext& b){return a.epoch==b.epoch&&a.thread==b.thread&&a.world==b.world&&a.selection==b.selection&&a.target==b.target&&a.exact_build==b.exact_build&&a.active_world==b.active_world&&a.saves_idle==b.saves_idle&&a.lease_current==b.lease_current&&a.main_thread==b.main_thread;}
bool RemoveLeaseServices::context(const remove::RemoveContext& expected,World& world,std::string& error){
    remove::RemoveContext actual;if(!host_.services().remove_current(actual,error))return false;
    if(!same_context(actual,expected)||!actual.exact_build||!actual.active_world||!actual.saves_idle||!actual.lease_current||!actual.main_thread||!actual.thread||!actual.selection||!actual.target){error="error: current Remove context/selection/target/thread differs";return false;}
    Proof proof;if(!host_.current(proof,error))return false;
    if(proof.world.epoch!=actual.epoch||uintptr_t(proof.world.world)!=actual.world){error="error: Remove context is outside retained native world";return false;}
    world=proof.world;return true;
}
bool RemoveLeaseServices::current(remove::RemoveContext& out,std::string& error){
    out={};if(busy_){error="error: Prop Remove current reentrancy rejected";return false;}Busy busy(busy_);try{if(!host_.services().remove_current(out,error))return false;World world;return context(out,world,error);}
    catch(const std::exception& ex){error=ex.what();return false;}catch(...){error="error: non-standard Prop Remove current exception";return false;}
}
bool RemoveLeaseServices::retain(const remove::RemoveContext& bound,const std::vector<uintptr_t>& objects,std::vector<remove::PropLease>& out,std::string& error){
    out.clear();if(busy_||observing_){error="error: Prop Remove retain reentrancy rejected";return false;}Busy busy(busy_);
    try{
        World world;if(!context(bound,world,error))return false;
        if(objects.empty()||std::set<uintptr_t>(objects.begin(),objects.end()).size()!=objects.size()){error="error: Prop retain requires unique nonempty selection plus target";return false;}
        if(std::find(objects.begin(),objects.end(),bound.target)==objects.end()){error="error: Prop retain batch omitted Remove target";return false;}
        if(batch_sequence_==std::numeric_limits<uint64_t>::max()){error="error: Prop retain batch sequence exhausted";return false;}
        ++batch_sequence_;batch_.clear();bool has_target=false;
        for(const auto object:objects){
            if(!object){error="error: Prop retain contains null object";return false;}
            Prop prop;if(!host_.read_prop(reinterpret_cast<void*>(object),0,prop,error))return false;
            if(!prop.uid){error="error: Prop actual full UID missing";return false;}
            const auto key="prop:"+std::to_string(batch_sequence_)+":"+std::to_string(batch_.size());
            owned_keys_.push_back(key);
            const auto receipt=pool_.acquire(key,reinterpret_cast<void*>(object),prop.uid,world,host_);
            PrivateLease own;Receipt saved;
            if(pool_.inspect(key,own,saved)){const remove::PropLease token{object,own.generation};bindings_.emplace(own.generation,Binding{bound,world,key,token,0});}
            if(receipt.phase!=Phase::Held){error=receipt.cause.data();return false;}
            const remove::PropLease token{object,own.generation};batch_.push_back(token);out.push_back(token);has_target|=object==bound.target;
        }
        if(!has_target){error="error: Prop retain batch omitted Remove target";return false;}
        World after;if(!context(bound,after,error))return false;if(after!=world){error="error: actual native world changed after mixed Prop retention";return false;}return true;
    }catch(const std::exception& ex){error=ex.what();return false;}catch(...){error="error: non-standard Prop Remove retain exception";return false;}
}
bool RemoveLeaseServices::inspect(const remove::RemoveContext& bound,const remove::PropLease& token,uintptr_t graph,remove::Life& life,std::string& error){
    life=remove::Life::Unknown;if(busy_){error="error: Prop Remove inspect reentrancy rejected";return false;}Busy busy(busy_);
    try{
        const auto it=bindings_.find(token.token);if(it==bindings_.end()||it->second.lease.object!=token.object||!same_context(it->second.context,bound)){error="error: Prop lease token/context binding differs";return false;}
        World actual;if(!context(bound,actual,error)||actual!=it->second.world){if(error.empty())error="error: Prop retained world changed";return false;}
        PrivateLease lease;if(pool_.read(it->second.key,lease,host_,error)){life=remove::Life::Alive;return true;}
        const auto original=error;
        if(!graph||!host_.services().observed_destruction(bound,graph,token,error)){if(error.empty())error=original;return false;}
        auto& binding=it->second;if(binding.retired_graph&&binding.retired_graph!=graph){error="error: Prop retirement was observed under another graph";return false;}
        const auto tombstone=pool_.tombstone(binding.key,host_);
        if(tombstone.phase!=Phase::Tombstone){error=original+"; "+tombstone.cause.data();return false;}
        binding.retired_graph=graph;life=remove::Life::RetiredByThisCommit;error.clear();return true;
    }catch(const std::exception& ex){error=ex.what();return false;}catch(...){error="error: non-standard Prop Remove inspect exception";return false;}
}
bool RemoveLeaseServices::observe_begin(const remove::RemoveContext& bound,std::string& error){
    if(busy_||observing_||batch_.empty()){error="error: Prop Remove graph observer reentrancy or missing retained batch";return false;}Busy busy(busy_);
    try{World world;if(!context(bound,world,error))return false;for(const auto& token:batch_){const auto it=bindings_.find(token.token);if(it==bindings_.end()||!same_context(it->second.context,bound)||it->second.world!=world){error="error: retained Prop batch differs from observer context";return false;}}
        if(!host_.services().observe_begin(bound,batch_,error))return false;observing_=true;return true;
    }catch(const std::exception& ex){error=ex.what();return false;}catch(...){error="error: non-standard Prop graph begin exception";return false;}
}
bool RemoveLeaseServices::observe_end(uintptr_t& graph,std::string& error){
    graph=0;if(busy_||!observing_){error="error: Prop graph observer is not active";return false;}Busy busy(busy_);observing_=false;
    try{if(!host_.services().observe_end(graph,error))return false;if(!graph){error="error: actual committed Remove graph absent";return false;}return true;}catch(const std::exception& ex){error=ex.what();return false;}catch(...){error="error: non-standard Prop graph end exception";return false;}
}
std::vector<Receipt> RemoveLeaseServices::retire_all(const Retirement& retired){
    if(retirement_attempted_)return retirement_receipts_;
    std::vector<Receipt> out;if(busy_||observing_){Receipt r;r.phase=Phase::Failed;const char* text="error: active Prop callbacks prohibit retirement";std::copy(text,text+std::char_traits<char>::length(text)+1,r.cause.begin());out.push_back(r);return out;}
    Busy busy(busy_);return detail::release_batch_once(owned_keys_,retirement_attempted_,retirement_receipts_,[&](const auto& key){return pool_.release(key,retired,host_);});
}
bool RemoveLeaseServices::enumerate_private(std::vector<PrivateRemoveRecord>& out,std::vector<PrivateAllocationRecord>& allocations,std::string& error){
    out.clear();allocations.clear();error.clear();if(!ownership_scopes_idle()){error="error: private Remove enumeration while ownership scope active";return false;}
    try{
        std::vector<PrivatePoolRecord> pools;if(!host_.enumerate_private_allocations(allocations,error)||!pool_.enumerate_private(pools,error))return false;
        std::set<std::string> keys(owned_keys_.begin(),owned_keys_.end());if(keys.size()!=owned_keys_.size()||keys.size()!=pools.size()||bindings_.size()!=pools.size()){error="error: private Remove pool/key/binding coverage differs";return false;}
        std::vector<PrivateRemoveRecord> records;records.reserve(pools.size());
        std::map<std::string,const PrivatePoolRecord*> indexed;for(const auto& p:pools)indexed.emplace(p.key,&p);
        for(const auto& key:owned_keys_){const auto found=indexed.find(key);if(found==indexed.end()){error="error: private Remove owned key missing from Pool";return false;}const auto& p=*found->second;const auto it=bindings_.find(p.lease.generation);if(!keys.contains(p.key)||it==bindings_.end()||it->second.key!=p.key||it->second.lease.object!=uintptr_t(p.lease.object)||it->second.lease.token!=p.lease.generation||it->second.world!=p.lease.world){error="error: private Remove allocation key/token/world binding differs";return false;}const auto& b=it->second;records.push_back({p,b.context,b.world,b.lease,b.retired_graph});}
        out=std::move(records);return true;
    }catch(const std::exception& ex){error=std::string("error: private Remove enumeration: ")+ex.what();return false;}catch(...){error="error: private Remove enumeration nonstandard exception";return false;}
}
}

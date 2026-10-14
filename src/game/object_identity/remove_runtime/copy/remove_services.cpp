#include "remove_services.h"
#include <algorithm>
#include <set>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace mt2mp::build_actions::private_native::host {
namespace {
bool fail(std::string& error,const char* cause){if(error.empty())error=cause;return false;}
// Keep SEH outside frames that own C++ objects; C++ exceptions retain their cause.
bool reserve_root(RequiredHostServices& root,uint64_t& value,std::string& error,uint32_t& seh){
    __try {return root.allocation_generation(value,error);}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){seh=GetExceptionCode();return false;}
}
struct Busy {bool& flag;explicit Busy(bool& b):flag(b){flag=true;}~Busy(){flag=false;}};
bool same(const RemoveContext& a,const RemoveContext& b){return a.epoch==b.epoch&&a.thread==b.thread&&a.world==b.world&&a.selection==b.selection&&a.target==b.target;}
}
Services::Services(Memory& memory,Origin& origin,RequiredHostServices& required,uintptr_t image,uint32_t thread,LookupApi api,std::string sha)
    :memory_(memory),required_(required),image_(image),thread_(thread),sha_(std::move(sha)),reader_(memory,origin,image,thread,api){}
bool Services::bind_target(void* object,uint64_t uid,std::string& error){
    error.clear();if(busy_||bound_target_)return fail(error,"remove_host_target_already_bound_or_busy");Busy busy(busy_);
    if(sha_!=mt2mp::native_uid::required_exe_sha)return fail(error,"remove_host_exe_hash");
    mt2mp::prop_lease::Prop prop;WorldState world;
    if(!reader_.prop(object,uid,prop,error)||!reader_.current(world,true,error))return false;
    target_=reinterpret_cast<uintptr_t>(object);target_uid_=prop.uid;bound_=world.origin;bound_target_=true;return true;
}
bool Services::validate_api(const mt2mp::prop_lease::Api& api,std::string& error){
    if(GetCurrentThreadId()!=thread_)return fail(error,"remove_host_api_wrong_thread");
    if(sha_!=mt2mp::native_uid::required_exe_sha)return fail(error,"remove_host_exe_hash");
    return mt2mp::prop_lease::validate_native_api(image_,api,error);
}
bool Services::runtime(mt2mp::prop_lease::RuntimeProof& out,std::string& error){
    out={};error.clear();if(busy_)return fail(error,"remove_host_runtime_reentrant");Busy busy(busy_);
    try {
        WorldState world;if(!reader_.current(world,true,error))return false;
        bool crt=false,free=false;uint32_t mask=0;if(!required_.installation(crt,mask,free,error))return false;
        if(observer_.status().active)return fail(error,"info: remove_host_callbacks_active");
        LookupApi current;if(!resolve_lookup(memory_,image_,sha_,current,error))return false;
        out.exe_sha=sha_;out.actual_entryguards=true;out.actual_crt_receipt=crt;out.actual_hook_receipt=free&&mask==15;
        out.proof.world={world.origin.world_epoch,world.origin.generation,world.origin.world,world.origin.map};
        out.proof.main_thread=true;out.proof.current_native_open=true;out.proof.effects_idle=true;return true;
    }catch(const std::exception& x){error=std::string("remove_host_runtime_exception: ")+x.what();return false;}
    catch(...){return fail(error,"remove_host_runtime_unknown_exception");}
}
bool Services::read_prop(void* object,uint64_t uid,mt2mp::prop_lease::Prop& out,std::string& error){
    error.clear();if(busy_)return fail(error,"remove_host_prop_reentrant");Busy busy(busy_);
    WorldState world;if(!reader_.current(world,true,error))return false;
    if(bound_target_&&!same_world(bound_,world.origin))return fail(error,"remove_host_prop_world_retired");
    return reader_.prop(object,uid,out,error);
}
bool Services::reserve_own_holder_generation(uint64_t& out,std::string& error){
    out=0;error.clear();
    if(!thread_||GetCurrentThreadId()!=thread_)return fail(error,"remove_host_generation_wrong_thread");
    if(!allocation_unknown_.empty()){error=allocation_unknown_+"; no replay";return false;}
    if(busy_)return fail(error,"remove_host_generation_reentrant");Busy busy(busy_);
    uint64_t candidate=0;uint32_t seh=0;
    try {
        if(!reserve_root(required_,candidate,error,seh)){
            if(seh)error="remove_host_allocation_generation_SEH: "+std::to_string(seh);
            else if(error.empty())error="remove_host_allocation_generation_failed_without_cause";
        }else if(!candidate)error="remove_host_allocation_generation_zero";
        else if(!allocation_generations_.insert(candidate).second)error="remove_host_allocation_generation_replayed";
        else {out=candidate;error.clear();return true;}
    }catch(const std::exception& x){error=std::string("remove_host_allocation_generation_exception: ")+x.what();}
    catch(...){error="remove_host_allocation_generation_unknown_exception";}
    allocation_unknown_="remove_host_allocation_generation_unknown: "+error;
    error=allocation_unknown_;return false;
}
bool Services::validate_retirement(const mt2mp::prop_lease::PrivateLease& lease,const mt2mp::prop_lease::Retirement& receipt,std::string& error){
    if(GetCurrentThreadId()!=thread_||observer_.status().active)return fail(error,"remove_host_retirement_callbacks_or_thread");
    if(lease.world!=receipt.old_world||!receipt.receipt||receipt.snapshot_receipt.empty())return fail(error,"remove_host_retirement_binding");
    return required_.retirement(lease,receipt,error);
}
bool Services::next_free_invocation(uint64_t& out,std::string& error){
    out=0;if(GetCurrentThreadId()!=thread_||observer_.status().active)return fail(error,"remove_host_free_callbacks_or_thread");
    if(!required_.free_invocation(out,error))return false;
    return out!=0||fail(error,"remove_host_free_invocation_zero");
}
bool Services::context(RemoveContext& out,bool idle,std::string& error){
    out={};if(!bound_target_)return fail(error,"remove_host_target_not_bound");
    WorldState world;if(!reader_.current(world,idle,error))return false;
    if(!same_world(bound_,world.origin))return fail(error,"remove_host_context_world_retired");
    bool current_lease=false;
    if(retained_.empty()){
        mt2mp::prop_lease::Prop prop;if(!reader_.prop(reinterpret_cast<void*>(target_),target_uid_,prop,error))return false;current_lease=true;
    }else current_lease=std::any_of(retained_.begin(),retained_.end(),[&](const auto& lease){return lease.object==target_&&lease.token!=0;});
    out={world.origin.world_epoch,thread_,reinterpret_cast<uintptr_t>(world.origin.world),world.selection,target_,sha_==mt2mp::native_uid::required_exe_sha,true,world.saves==0,current_lease,true};return true;
}
bool Services::remove_current(RemoveContext& out,std::string& error){
    error.clear();if(busy_)return fail(error,"remove_host_current_reentrant");Busy busy(busy_);return context(out,true,error);
}
bool Services::observe_begin(const RemoveContext& expected,const std::vector<PropLease>& leases,std::string& error){
    error.clear();if(busy_)return fail(error,"remove_host_observe_reentrant");Busy busy(busy_);
    RemoveContext actual;if(!context(actual,true,error)||!same(actual,expected))return fail(error,"remove_host_observe_context_changed");
    std::set<uintptr_t> objects;std::set<uint64_t> tokens;std::vector<uintptr_t> retained;
    for(const auto& lease:leases){if(!lease.object||!lease.token||!objects.insert(lease.object).second||!tokens.insert(lease.token).second)return fail(error,"remove_host_lease_set_invalid");retained.push_back(lease.object);}
    if(!objects.contains(target_))return fail(error,"remove_host_target_lease_missing");
    auto stable_leases=leases;
    if(!observer_.begin(actual,retained,error))return false;retained_=std::move(stable_leases);hook_error_.clear();return true;
}
bool Services::observe_end(uintptr_t& graph,std::string& error){
    error.clear();if(busy_)return fail(error,"remove_host_end_reentrant");Busy busy(busy_);
    const bool ended=observer_.finish(graph,error);if(!hook_error_.empty()){error=hook_error_;return false;}return ended;
}
bool Services::observed_destruction(const RemoveContext& expected,uintptr_t graph,const PropLease& lease,std::string& error){
    error.clear();RemoveContext actual;if(!context(actual,true,error)||!same(actual,expected))return fail(error,"remove_host_retired_context_changed");
    if(!hook_error_.empty()){error=hook_error_;return false;}
    if(graph!=observer_.status().graph||!observer_.observed_destruction(lease.object))return fail(error,"remove_host_own_destructor_receipt_missing");
    if(std::none_of(retained_.begin(),retained_.end(),[&](const auto& held){return held.object==lease.object&&held.token==lease.token;}))return fail(error,"remove_host_destructor_lease_generation_mismatch");return true;
}
bool Services::hook_context(RemoveContext& out){
    if(GetCurrentThreadId()!=thread_){++worker_hooks_;return false;}
    if(!observer_.status().active)return false;
    std::string error;
    try {if(context(out,false,error))return true;}
    catch(const std::exception& x){error=std::string("remove_hook_context_exception: ")+x.what();}
    catch(...){error="remove_hook_context_unknown_exception";}
    if(hook_error_.empty())hook_error_=error.empty()?"remove_hook_context_failed_without_cause":error;return false;
}
void Services::undo_open_returned(uintptr_t caller,uintptr_t graph){RemoveContext c;if(hook_context(c))observer_.undo_open_returned(c,caller,graph);}
void Services::graph_lock_enter(uintptr_t caller,uintptr_t graph){RemoveContext c;if(hook_context(c))observer_.graph_lock_enter(c,caller,graph,memory_,image_);}
void Services::graph_lock_returned(uintptr_t graph,bool returned){RemoveContext c;if(hook_context(c))observer_.graph_lock_returned(c,graph,returned,memory_,image_);}
void Services::prop_destructor_enter(uintptr_t prop){RemoveContext c;if(hook_context(c))observer_.prop_destructor_enter(c,prop,memory_,image_);}
}

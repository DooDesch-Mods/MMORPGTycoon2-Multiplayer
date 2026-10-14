#include "retirement_journal.h"
#include <set>
#include <algorithm>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace mt2mp::remove_runtime {
namespace {
bool fail(std::string& error,const char* cause){error=cause;return false;}
bool factory_binding(const lease::PrivateLease& factory,const lease::PrivateLease& own){return factory.holder==own.holder&&factory.object==own.object&&factory.uid==own.uid&&factory.world==own.world&&factory.generation==own.generation&&factory.class_addresspoint==own.class_addresspoint;}
}
bool UnavailableRootBindingRetirement::read_actual_revocation(const PrivateRequest&,BindingRevocation& out,std::string& error){out={};return fail(error,"info: actual Root stable-binding revocation registry unavailable; holders retained");}
bool UnavailableRootBindingRetirement::verify_actual_revocation(const PrivateRequest&,const BindingRevocation&,std::string& error){return fail(error,"info: actual Root stable-binding revocation registry unavailable; holders retained");}
bool read_root_revocation_guarded(RootBindingRetirement& root,const PrivateRequest& request,BindingRevocation& result,std::string& error,uint32_t& seh){
    seh=0;__try {return root.read_actual_revocation(request,result,error);}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:(seh=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER)){return false;}
}
bool verify_root_revocation_guarded(RootBindingRetirement& root,const PrivateRequest& request,const BindingRevocation& result,std::string& error,uint32_t& seh){
    seh=0;__try {return root.verify_actual_revocation(request,result,error);}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:(seh=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER)){return false;}
}
bool match_revocation(const PrivateRequest& request,const BindingRevocation& root,std::string& error){
    if(!root.registry_generation||!root.entry_id||root.sequence!=request.sequence||root.full_uid!=request.full_uid||root.world!=request.world||root.stable_snapshot_key!=request.snapshot_binding||root.immutable_revocation_key.empty())return fail(error,"error: actual Root revocation sequence/UID/world/stable-key record differs");return true;
}
bool RetirementJournal::seal(const RetirementRecord& record,std::string& error){
    error.clear();if(sealed_||effects_.attempted)return fail(error,"error: own quiescence journal is immutable; seal never replays");
    if(!record.id)return fail(error,"error: own quiescence journal ID absent");
    if(!record.request.sequence||!record.request.object||!record.request.full_uid||!record.request.world.epoch||!record.request.world.origin_generation||!record.request.world.world||!record.request.world.map||record.request.snapshot_binding.empty())return fail(error,"error: own quiescence complete private binding absent");
    if(!match_revocation(record.request,record.root,error))return false;
    const auto& q=record.quiescence;
    if(!q.bound_thread||q.observed_thread!=q.bound_thread||GetCurrentThreadId()!=q.bound_thread)return fail(error,"error: own quiescence requires actual bound OS Mainthread");
    if(!q.world.epoch||!q.world.origin_generation||!q.world.world||!q.world.map||q.saves||q.edits||q.active_graph)return fail(error,"error: actual quiescence world/Save/Edit/Undo differs");
    if(q.dispatch_inflight||q.active_operation||q.backend_busy||q.adapter_busy||q.observer_active)return fail(error,"error: actual quiescence dispatch/operation/backend/adapter/observer scope active");
    const auto& commit=record.commit;
    if(commit.sequence!=record.request.sequence||!commit.attempted||!commit.returned||!commit.confirmed||commit.unknown_locked||!commit.target_retired||!commit.undo_confirmed||!commit.selection_confirmed||!record.committed_graph)return fail(error,"error: own Remove/graph/destructor/selection completion absent");
    if(record.pools.empty()||record.pools.size()!=record.factories.size()||record.pools.size()!=record.ownership.size()||commit.retained!=record.pools.size())return fail(error,"error: private allocation/key/factory/ownership coverage incomplete");
    std::set<std::string> keys;std::set<uint64_t> generations;std::set<void*> holders;
    size_t retired=0;bool target_retired=false;
    for(size_t i=0;i<record.pools.size();++i){
        const auto& p=record.pools[i];const auto& own=p.pool.lease;const auto& receipt=p.pool.receipt;const auto& read=record.ownership[i];
        if(p.pool.key.empty()||!keys.insert(p.pool.key).second||!own.holder||!own.object||!own.proxy||!own.uid||!own.generation||!own.class_addresspoint||!generations.insert(own.generation).second||!holders.insert(own.holder).second||own.world!=record.request.world||p.world!=own.world||p.token.object!=uintptr_t(own.object)||p.token.token!=own.generation)return fail(error,"error: private allocation key/holder/generation/class/UID/world/token differs");
        if(p.context.epoch!=own.world.epoch||p.context.world!=uintptr_t(own.world.world)||p.context.thread!=q.bound_thread||p.context.target!=record.request.object||!p.context.main_thread||!p.context.exact_build||!p.context.active_world||!p.context.lease_current)return fail(error,"error: private allocation operation context differs");
        if(receipt.factory_attempted!=1||receipt.factory_returned!=1||receipt.retain_attempted!=1||receipt.retain_confirmed!=1||receipt.release_attempted||receipt.delete_attempted||(receipt.phase!=lease::Phase::Held&&receipt.phase!=lease::Phase::Tombstone))return fail(error,"error: private allocation own factory/retain receipt is not unconsumed Held/Tombstone");
        if(read.lease!=own||read.phase!=receipt.phase||read.proxy_references<=0)return fail(error,"error: actual continuously retained ownership differs");
        if(receipt.phase==lease::Phase::Tombstone){if(read.proxy_target||p.retired_graph!=record.committed_graph)return fail(error,"error: own destructor/graph/WeakTargetNULL receipt differs");++retired;target_retired|=uintptr_t(own.object)==record.request.object&&own.uid==record.request.full_uid;}
        else if((read.proxy_target&&read.proxy_target!=uintptr_t(own.object)+0x20)||p.retired_graph)return fail(error,"error: survivor held weak target/retired graph differs");
        const auto it=std::find_if(record.factories.begin(),record.factories.end(),[&](const auto& f){return f.factory.generation==own.generation;});
        if(it==record.factories.end()||it->attempted||it->consumed||!factory_binding(it->factory,own))return fail(error,"error: private actual factory-allocation binding absent/replayed");
    }
    std::set<uint64_t> factory_generations;for(const auto& f:record.factories)if(!factory_generations.insert(f.factory.generation).second||!generations.contains(f.factory.generation))return fail(error,"error: actual factory enumeration has extra or duplicate allocation");
    if(!target_retired||retired!=commit.retired)return fail(error,"error: actual target/cascade tombstone coverage differs");
    sealed_=record;return true;
}
bool RetirementJournal::begin_run(std::string& error){if(!sealed_||effects_.attempted||effects_.unknown)return fail(error,"error: own retirement run absent/already attempted; no replay");effects_.attempted=true;return true;}
bool RetirementJournal::validate_allocation(const lease::PrivateLease& lease,const lease::Retirement& receipt,std::string& error)const{
    if(!sealed_||!effects_.attempted||effects_.unknown||effects_.confirmed)return fail(error,"error: own actual retirement journal is not in its single active run");
    if(receipt.old_world!=sealed_->request.world||receipt.receipt!=sealed_->id||receipt.snapshot_receipt!=sealed_->root.immutable_revocation_key)return fail(error,"error: own retirement ID/world/Root record differs");
    const auto it=std::find_if(sealed_->pools.begin(),sealed_->pools.end(),[&](const auto& row){return row.pool.lease==lease;});
    return it!=sealed_->pools.end()||fail(error,"error: own retirement allocation not in complete sealed coverage");
}
void RetirementJournal::unknown(const std::string& cause){effects_.unknown=true;effects_.confirmed=false;if(effects_.cause.empty())effects_.cause=cause.empty()?"error: own retirement Unknown without cause":cause;}
bool RetirementJournal::confirm_run(std::vector<lease::Receipt> releases,std::vector<native_free::Receipt> frees,std::string& error){
    if(!sealed_||!effects_.attempted||effects_.confirmed||effects_.unknown)return fail(error,"error: own retirement confirmation not current; no replay");
    effects_.releases=std::move(releases);effects_.frees=std::move(frees);
    auto reject=[&](const char* cause){unknown(cause);error=effects_.cause;return false;};
    if(effects_.releases.size()!=sealed_->pools.size()||effects_.frees.size()!=sealed_->pools.size()){
        if(!effects_.releases.empty()&&effects_.releases.back().phase!=lease::Phase::Released){unknown(effects_.releases.back().cause.data());error=effects_.cause;return false;}
        return reject("error: own per-allocation release/FreeACK coverage incomplete; no more stores");
    }
    std::set<uint64_t> calls;
    for(size_t i=0;i<sealed_->pools.size();++i){const auto& row=sealed_->pools[i];const auto& r=effects_.releases[i];const auto& f=effects_.frees[i];const auto& p=f.permit;
        if(r.seh_code){unknown("error: current own release receipt SEH="+std::to_string(r.seh_code)+" cause="+r.cause.data());error=effects_.cause;return false;}
        if(f.seh_code){unknown("error: current own FreeACK SEH="+std::to_string(f.seh_code)+" cause="+f.cause.data());error=effects_.cause;return false;}
        if(r.phase!=lease::Phase::Released||r.seh_code||r.release_attempted!=1||r.release_confirmed!=1||r.delete_attempted!=1||r.delete_confirmed!=1){unknown(r.cause.data());error=effects_.cause;return false;}
        if(f.phase!=native_free::Phase::Confirmed||f.seh_code||p.allocation_generation!=row.pool.lease.generation||p.holder!=row.pool.lease.holder||p.world_generation!=sealed_->quiescence.world.origin_generation||p.retirement_receipt!=sealed_->id||p.bytes!=56||!p.root_binding_confirmed||!p.call||!calls.insert(p.call).second||f.delete_attempted!=1||f.delete_returned!=1||f.sized_attempted!=1||f.sized_returned!=1||f.free_attempted!=1||f.free_returned!=1)return reject("error: current own per-allocation D0/Sized/FreeACK differs; no historical ACK");
    }
    effects_.confirmed=true;effects_.cause="info: every enumerated own allocation released with its current FreeACK";return true;
}
}

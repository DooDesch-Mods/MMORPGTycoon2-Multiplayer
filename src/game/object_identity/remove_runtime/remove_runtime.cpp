#include "remove_runtime.h"
#include "owned_undo_open.h"
#include "mt2mp/hooks/minhook_backend.h"
#include <algorithm>
#include <cstring>
#include <exception>
#include <intrin.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace mt2mp::remove_runtime {
namespace {
std::atomic<HookDispatch*> published_dispatch{nullptr};std::atomic<uintptr_t> published_image{0};
HookDispatch& dispatch(){auto* p=published_dispatch.load(std::memory_order_acquire);if(!p)RaiseFailFastException(nullptr,nullptr,0);return *p;}
uintptr_t caller_rva(void* p)noexcept{const auto image=published_image.load(std::memory_order_acquire),pc=uintptr_t(p);return image&&pc>=image?pc-image:UINTPTR_MAX;}
void clear_entry(void* p){dispatch().clear(p,caller_rva(_ReturnAddress()));}
void* open_entry(const void* p){return dispatch().open(p,caller_rva(_ReturnAddress()));}
void lock_entry(void* p){dispatch().lock(p,caller_rva(_ReturnAddress()));}
void destructor_entry(void* p){dispatch().destructor(p);}
bool executable(uintptr_t address,MEMORY_BASIC_INFORMATION& info,std::string& error){
    if(!address||!VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))||!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){error="remove_runtime_actual_route_not_executable";return false;}return true;
}
bool route(host::ProcessMemory& memory,uintptr_t target,uintptr_t original,uintptr_t hook,std::string& error){
    MEMORY_BASIC_INFORMATION a{},b{};if(!executable(target,a,error)||!executable(original,b,error))return false;
    std::array<uint8_t,5> code{};if(!memory.copy(target,code.data(),code.size(),error))return false;
    if(code[0]!=0xe9){error="remove_runtime_actual_target_not_own_E9";return false;}int32_t relative=0;std::memcpy(&relative,code.data()+1,4);
    const auto relay=uintptr_t(intptr_t(target+5)+relative);
    if(relay==hook)return true;
    MEMORY_BASIC_INFORMATION relay_info{};if(!executable(relay,relay_info,error)||relay_info.AllocationBase!=b.AllocationBase){error="remove_runtime_actual_relay_not_own_trampoline_allocation";return false;}
    std::array<uint8_t,14> jump{};if(!memory.copy(relay,jump.data(),jump.size(),error))return false;uintptr_t destination=0;std::memcpy(&destination,jump.data()+6,8);
    if(jump[0]!=0xff||jump[1]!=0x25||jump[2]||jump[3]||jump[4]||jump[5]||destination!=hook){error="remove_runtime_actual_relay_not_own_typed_hook";return false;}return true;
}
struct Busy {bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}};
OperationStatus rejected(uint64_t sequence,const std::string& error){OperationStatus r;r.sequence=sequence;r.phase=OperationPhase::Failed;r.removal.sequence=sequence;r.removal.error=error;return r;}
}
struct Runtime::Operation {
    PrivateRequest request;ActualHostServices required;host::Services services;
    lease::NativeHost prop_host;lease::RemoveLeaseServices adapter;native::RemoveBackend backend;
    native::RemoveContext context;host::opened::CurrentWorld origin;
    RetirementJournal retirement;
    Operation(Runtime& r,const PrivateRequest& req,native_free::Observer& observer)
        :request(req),required(r,req.sequence),services(r.memory_,r.origin_,required,r.installation_.image,r.main_thread_.load(),r.lookup_,r.sha_),
        prop_host(lease::for_module(r.installation_.image),services,observer,r.main_thread_.load()),
        adapter(lease::for_module(r.installation_.image),prop_host,r.pool_log_),
        backend(r.installation_.image,r.memory_,adapter,r.entries_.destroy,r.entries_.highlights){}
};
Runtime::Runtime():dispatch_(*this){}
Runtime& Runtime::instance(){static Runtime* value=new Runtime;return *value;}
void Runtime::PoolLog::error_unretired_native_ownership(size_t,bool unknown)noexcept{native_free_runtime::Runtime::instance().journal().error("error:remove-unretired-ownership",unknown?"own Prop pool has Unknown ownership":"own Prop holders were not actually retired",0);}
bool Runtime::fail(RuntimePhase phase,const std::string& cause,std::string& error){installation_.phase=phase;installation_.cause=cause.empty()?"remove_runtime_failed_without_cause":cause;error=installation_.cause;free_runtime().journal().error("error:remove-runtime",error.c_str(),0);return false;}
bool ActualHostServices::installation(bool& crt,uint32_t& remove_mask,bool& free,std::string& error){
    crt=false;remove_mask=0;free=false;native_free_runtime::Receipt actual;
    if(!runtime_.read_actual_hook_mask(remove_mask,error)||!runtime_.free_runtime().read_actual_receipt(actual,error))return false;
    if(actual.main_thread!=GetCurrentThreadId()||actual.phase!=native_free_runtime::Phase::Enabled){error="remove_runtime_actual_Free_receipt_thread_or_phase";return false;}
    crt=actual.crt_confirmed&&actual.heap.actual_exports&&actual.originals_published&&actual.iat_protection_restored&&actual.trampoline_route;
    free=actual.hooks_confirmed&&actual.hook_mask==3&&runtime_.free_runtime().actual_observer()!=nullptr;
    if(!crt||!free){error="remove_runtime_actual_CRT_or_Free_routes_incomplete";return false;}return true;
}
bool ActualHostServices::allocation_generation(uint64_t& value,std::string& error){return runtime_.free_runtime().reserve_holder_generation(value,error);}
bool ActualHostServices::free_invocation(uint64_t& value,std::string& error){return runtime_.free_runtime().next_free_invocation(value,error);}
bool ActualHostServices::retirement(const lease::PrivateLease& own,const lease::Retirement& retired,std::string& error){return runtime_.validate_own_retirement(sequence_,own,retired,error);}
bool Runtime::prepare_and_enable(std::string& error)try{
    native_free_runtime::Receipt free;if(!free_runtime().read_actual_receipt(free,error))return false;
    if(installation_.phase!=RuntimePhase::Idle){error="remove_runtime_installation_never_replays";return false;}
    if(!free.image||free.main_thread!=GetCurrentThreadId()||std::strcmp(free.executable.sha256.data(),native_uid::required_exe_sha))return fail(RuntimePhase::Failed,"remove_runtime_actual_executable_thread_binding",error);
    main_thread_.store(free.main_thread,std::memory_order_release);installation_.image=free.image;installation_.main_thread=free.main_thread;sha_=free.executable.sha256.data();
    if(!host::resolve_lookup(memory_,free.image,sha_,lookup_,error)||!native::resolve_remove(memory_,free.image,sha_,entries_,error))return fail(RuntimePhase::Failed,error,error);
    installation_.targets={uintptr_t(entries_.clear),uintptr_t(entries_.open),uintptr_t(entries_.lock),uintptr_t(entries_.prop_destructor)};
    const std::array<uintptr_t,4> hooks{uintptr_t(&clear_entry),uintptr_t(&open_entry),uintptr_t(&lock_entry),uintptr_t(&destructor_entry)};
    for(size_t i=0;i<4;++i){void* original=nullptr;installation_.create_attempted[i]=true;const auto rc=MH_CreateHook(reinterpret_cast<void*>(installation_.targets[i]),reinterpret_cast<void*>(hooks[i]),&original);installation_.create_status[i]=rc;
        if(rc!=MH_OK)return fail(RuntimePhase::Failed,"remove_runtime_MinHook_create index="+std::to_string(i)+" status="+std::to_string(rc),error);
        installation_.created_mask|=uint32_t(1)<<i;
        installation_.trampolines[i]=uintptr_t(original);MEMORY_BASIC_INFORMATION info{};
        if(!executable(uintptr_t(original),info,error))return fail(RuntimePhase::Failed,error,error);
    }
    native::RemoveEntrypoints originals=entries_;originals.clear=reinterpret_cast<native::ClearSelectionFn>(installation_.trampolines[0]);originals.open=reinterpret_cast<native::UndoOpenFn>(installation_.trampolines[1]);originals.lock=reinterpret_cast<native::GraphLockFn>(installation_.trampolines[2]);originals.prop_destructor=reinterpret_cast<native::PropDestructorFn>(installation_.trampolines[3]);
    if(!dispatch_.publish_disabled(originals))return fail(RuntimePhase::Failed,"remove_runtime_typed_original_publication_failed",error);
    published_image.store(free.image,std::memory_order_release);published_dispatch.store(&dispatch_,std::memory_order_release);installation_.originals_published=true;installation_.phase=RuntimePhase::Prepared;
    for(size_t i=0;i<4;++i){installation_.enable_attempted[i]=true;const auto rc=MH_EnableHook(reinterpret_cast<void*>(installation_.targets[i]));installation_.enable_status[i]=rc;
        if(rc!=MH_OK)return fail(RuntimePhase::Unknown,"remove_runtime_MinHook_enable index="+std::to_string(i)+" status="+std::to_string(rc)+"; inspect visibility, no replay",error);
        if(!route(memory_,installation_.targets[i],installation_.trampolines[i],hooks[i],error))return fail(RuntimePhase::Unknown,error,error);
        installation_.hook_mask|=uint32_t(1)<<i;
    }
    installation_.phase=RuntimePhase::Enabled;installation_.cause="info: four actual Remove routes and typed originals published";return true;
}catch(const std::exception& ex){return fail(installation_.originals_published?RuntimePhase::Unknown:RuntimePhase::Failed,ex.what(),error);}catch(...){return fail(installation_.originals_published?RuntimePhase::Unknown:RuntimePhase::Failed,"remove_runtime_nonstandard_installation_exception",error);}
bool Runtime::read_actual_hook_mask(uint32_t& mask,std::string& error){
    mask=0;if(!main_thread_.load()||GetCurrentThreadId()!=main_thread_.load()){error="remove_runtime_hook_receipt_wrong_thread";return false;}
    if(original_fault_.load(std::memory_order_acquire)||installation_.phase!=RuntimePhase::Enabled||!installation_.originals_published||!dispatch_.published()){error="remove_runtime_actual_Remove_routes_unavailable";return false;}
    const std::array<uintptr_t,4> hooks{uintptr_t(&clear_entry),uintptr_t(&open_entry),uintptr_t(&lock_entry),uintptr_t(&destructor_entry)};
    for(size_t i=0;i<4;++i)if(!route(memory_,installation_.targets[i],installation_.trampolines[i],hooks[i],error))return fail(RuntimePhase::Unknown,error,error);
    mask=15;return true;
}
bool Runtime::read_owned_undo_open(uintptr_t image,native::UndoOpenFn& out,std::string& error){
    out=nullptr;
    uint32_t mask=0;if(!read_actual_hook_mask(mask,error))return false;
    if(image!=installation_.image||mask!=15||installation_.hook_mask!=15||installation_.created_mask!=15||
       published_dispatch.load(std::memory_order_acquire)!=&dispatch_||published_image.load(std::memory_order_acquire)!=image){error="remove_runtime_owned_Open_owner_binding_differs";return false;}
    if(busy_||active_||retiring_||dispatch_.inflight()){error="remove_runtime_owned_Open_scope_not_idle";return false;}
    for(size_t i=0;i<4;++i)if(!installation_.create_attempted[i]||!installation_.enable_attempted[i]||
       installation_.create_status[i]!=MH_OK||installation_.enable_status[i]!=MH_OK){error="remove_runtime_owned_Open_installation_incomplete";return false;}
    if(installation_.targets[1]!=image+0x539990||uintptr_t(dispatch_.originals().open)!=installation_.trampolines[1]){error="remove_runtime_owned_Open_typed_original_differs";return false;}
    if(!validate_owned_undo_open_bytes(memory_,installation_.targets[1],installation_.trampolines[1],error))return fail(RuntimePhase::Unknown,error,error);
    out=reinterpret_cast<native::UndoOpenFn>(installation_.targets[1]);return true;
}
bool read_owned_undo_open(uintptr_t image,native::UndoOpenFn& out,std::string& error){
    return Runtime::instance().read_owned_undo_open(image,out,error);
}
OperationStatus Runtime::execute(const PrivateRequest& request){
    if(!main_thread_.load()||GetCurrentThreadId()!=main_thread_.load())return rejected(request.sequence,"remove_runtime_execute_wrong_thread");
    if(busy_)return rejected(request.sequence,"remove_runtime_execute_reentrant");Busy busy(busy_);
    if(installation_.phase!=RuntimePhase::Enabled&&installation_.phase!=RuntimePhase::Unknown)return rejected(request.sequence,"remove_runtime_hooks_not_enabled");
    std::string error;auto admission=ledger_.admit(request,error);
    if(admission==OperationLedger::Admission::Existing)return *ledger_.find(request.sequence);
    if(admission==OperationLedger::Admission::Rejected)return rejected(request.sequence,error);
    try{
        uint32_t mask=0;native_free_runtime::Receipt free;
        if(!read_actual_hook_mask(mask,error)||!free_runtime().read_actual_receipt(free,error)){auto result=rejected(request.sequence,error);ledger_.record(request.sequence,result.removal,{});if(installation_.phase==RuntimePhase::Unknown||free.phase==native_free_runtime::Phase::Unknown||original_fault_.load())ledger_.lock_unknown(request.sequence,error);return *ledger_.find(request.sequence);}
        host::Reader reader(memory_,origin_,installation_.image,main_thread_.load(),lookup_);host::WorldState world;
        if(!reader.current(world,true,error)){auto result=rejected(request.sequence,error);ledger_.record(request.sequence,result.removal,{});return *ledger_.find(request.sequence);}
        const lease::World actual{world.origin.world_epoch,world.origin.generation,world.origin.world,world.origin.map};
        if(actual!=request.world||free.world.epoch!=actual.epoch||free.world.generation!=actual.origin_generation||free.world.world!=uintptr_t(actual.world)||free.world.map!=uintptr_t(actual.map)){auto result=rejected(request.sequence,"remove_runtime_actual_world_differs_from_snapshot_binding");ledger_.record(request.sequence,result.removal,{});return *ledger_.find(request.sequence);}
        auto* observer=free_runtime().actual_observer();if(!observer){auto result=rejected(request.sequence,"remove_runtime_actual_observer_unavailable");ledger_.record(request.sequence,result.removal,{});return *ledger_.find(request.sequence);}
        auto operation=std::make_unique<Operation>(*this,request,*observer);auto* current=operation.get();operations_.emplace(request.sequence,std::move(operation));current->origin=world.origin;
        if(!current->services.bind_target(reinterpret_cast<void*>(request.object),request.full_uid,error)||!current->services.remove_current(current->context,error)){auto result=rejected(request.sequence,error);ledger_.record(request.sequence,result.removal,current->adapter.status());return *ledger_.find(request.sequence);}
        current->backend.set_hook_mask(uint8_t(mask));hook_error_.fill(0);active_=current;
        const auto result=current->backend.execute(request.sequence,current->context);active_=nullptr;
        ledger_.record(request.sequence,result,current->adapter.status());
        if(hook_error_[0]||original_fault_.load(std::memory_order_acquire))ledger_.lock_unknown(request.sequence,hook_error_[0]?hook_error_.data():"remove_runtime_actual_original_failure");
        if(result.error.find("remove_host_allocation_generation_unknown")!=std::string::npos)ledger_.lock_unknown(request.sequence,result.error);
        if(ledger_.unknown_locked())installation_.phase=RuntimePhase::Unknown;
        return *ledger_.find(request.sequence);
    }catch(const std::exception& ex){active_=nullptr;const auto it=operations_.find(request.sequence);auto result=rejected(request.sequence,ex.what());ledger_.record(request.sequence,result.removal,it==operations_.end()?lease::Status{}:it->second->adapter.status());if(it!=operations_.end())ledger_.lock_unknown(request.sequence,ex.what());return *ledger_.find(request.sequence);}
    catch(...){active_=nullptr;const auto it=operations_.find(request.sequence);auto result=rejected(request.sequence,"remove_runtime_nonstandard_execute_exception");ledger_.record(request.sequence,result.removal,it==operations_.end()?lease::Status{}:it->second->adapter.status());if(it!=operations_.end())ledger_.lock_unknown(request.sequence,result.removal.error);return *ledger_.find(request.sequence);}
}
bool Runtime::retire_completed(uint64_t sequence,std::string& error){
    error.clear();if(!main_thread_.load()||GetCurrentThreadId()!=main_thread_.load()){error="remove_runtime_retirement_wrong_thread";return false;}
    const auto it=operations_.find(sequence);if(it==operations_.end()){error="remove_runtime_retirement_operation_missing";return false;}auto& own=*it->second;
    if(own.retirement.effects().confirmed)return true;
    auto blocked=[&](const std::string& cause){error=cause.empty()?"remove_runtime_retirement_failed_without_cause":cause;ledger_.retention_blocked(sequence,own.adapter.status(),error);free_runtime().journal().error("info:remove-retirement-blocked",error.c_str(),0);return false;};
    if(own.retirement.effects().attempted||own.retirement.effects().unknown)return blocked(own.retirement.effects().cause.empty()?"remove_runtime_retirement_already_attempted; no replay":own.retirement.effects().cause);
    if(busy_||active_||dispatch_.inflight())return blocked("info: measured Remove ownership/dispatch scope is not idle");Busy busy(busy_);
    const auto* completed=ledger_.find(sequence);if(!completed||completed->phase!=OperationPhase::Confirmed||!completed->removal.confirmed||ledger_.unknown_locked()||own.adapter.status().unknown_lock)return blocked("remove_runtime_retirement_requires_confirmed_nonUnknown_operation");
    if(!root_retirement_)return blocked("info: actual Root stable-binding retirement service not bound; holders retained");
    try {
        uint32_t mask=0;if(!read_actual_hook_mask(mask,error))return blocked(error);
        native_free_runtime::Receipt free;if(!free_runtime().read_actual_receipt(free,error))return blocked(error);
        host::Reader reader(memory_,origin_,installation_.image,main_thread_.load(),lookup_);host::WorldState world;if(!reader.current(world,true,error))return blocked(error);
        const auto& observation=own.services.observation();
        if(!observation.complete||observation.failed||observation.active||observation.in_lock||!observation.graph||own.backend.busy()||!own.adapter.ownership_scopes_idle())return blocked("remove_runtime_retirement_actual_graph/backend/adapter_scope_not_complete");
        BindingRevocation root;uint32_t seh=0;
        if(!read_root_revocation_guarded(*root_retirement_,own.request,root,error,seh))return blocked(seh?"remove_runtime_actual_Root_revocation_read_SEH="+std::to_string(seh):error);
        if(!match_revocation(own.request,root,error))return blocked(error);
        RetirementRecord record;record.request=own.request;record.root=root;record.commit=completed->removal;record.committed_graph=observation.graph;
        if(!own.adapter.enumerate_private(record.pools,record.factories,error))return blocked(error);
        record.ownership.reserve(record.pools.size());
        for(const auto& row:record.pools){lease::PrivateOwnershipRead read;if(!own.adapter.read_owned_private(row.pool.key,read,error))return blocked(error);record.ownership.push_back(read);}
        if(!verify_root_revocation_guarded(*root_retirement_,own.request,root,error,seh))return blocked(seh?"remove_runtime_actual_Root_revocation_verify_SEH="+std::to_string(seh):error);
        host::WorldState after;if(!reader.current(after,true,error)||!host::same_world(world.origin,after.origin))return blocked(error.empty()?"remove_runtime_actual_world_changed_during_quiescence":error);
        if(next_retirement_==UINT64_MAX)return blocked("remove_runtime_own_retirement_journal_exhausted");record.id=++next_retirement_;
        record.quiescence={main_thread_.load(),GetCurrentThreadId(),{after.origin.world_epoch,after.origin.generation,after.origin.world,after.origin.map},after.saves,after.edits,after.active_graph,uintptr_t(active_),dispatch_.inflight(),own.backend.busy(),!own.adapter.ownership_scopes_idle(),own.services.observation().active};
        std::vector<native_free::Receipt> frees;frees.reserve(record.pools.size());
        if(!own.retirement.seal(record,error))return blocked(error);
        if(!own.retirement.begin_run(error))return blocked(error);retiring_=&own;
        const lease::Retirement receipt{own.request.world,root.immutable_revocation_key,record.id};
        auto releases=own.adapter.retire_all(receipt);retiring_=nullptr;
        for(size_t i=0;i<releases.size()&&i<record.pools.size();++i)frees.push_back(own.prop_host.free_receipt(record.pools[i].pool.lease.generation));
        if(!own.retirement.confirm_run(std::move(releases),std::move(frees),error)){ledger_.lock_unknown(sequence,error);installation_.phase=RuntimePhase::Unknown;return blocked(error);}
        ledger_.retention_confirmed(sequence,own.adapter.status());return true;
    }catch(const std::exception& ex){retiring_=nullptr;if(own.retirement.effects().attempted){own.retirement.unknown(ex.what());ledger_.lock_unknown(sequence,ex.what());installation_.phase=RuntimePhase::Unknown;}return blocked(std::string("remove_runtime_retirement_exception: ")+ex.what());}
    catch(...){retiring_=nullptr;const std::string cause="remove_runtime_retirement_nonstandard_exception";if(own.retirement.effects().attempted){own.retirement.unknown(cause);ledger_.lock_unknown(sequence,cause);installation_.phase=RuntimePhase::Unknown;}return blocked(cause);}
}
bool Runtime::bind_root_retirement_service(RootBindingRetirement& service,std::string& error){
    if(!main_thread_.load()||GetCurrentThreadId()!=main_thread_.load()){error="remove_runtime_Root_retirement_binding_wrong_thread";return false;}
    if(busy_||active_||retiring_){error="remove_runtime_Root_retirement_binding_scope_active";return false;}
    if(root_retirement_&&root_retirement_!=&service){error="remove_runtime_Root_retirement_service_already_bound";return false;}root_retirement_=&service;return true;
}
const RetirementJournal* Runtime::private_retirement_status(uint64_t sequence)const{if(!main_thread_.load()||GetCurrentThreadId()!=main_thread_.load())return nullptr;const auto it=operations_.find(sequence);return it==operations_.end()?nullptr:&it->second->retirement;}
bool Runtime::validate_own_retirement(uint64_t sequence,const lease::PrivateLease& allocation,const lease::Retirement& receipt,std::string& error){
    if(!main_thread_.load()||GetCurrentThreadId()!=main_thread_.load()){error="remove_runtime_actual_retirement_validation_wrong_thread";return false;}
    const auto it=operations_.find(sequence);if(it==operations_.end()||retiring_!=it->second.get()||active_||dispatch_.inflight()){error="remove_runtime_actual_retirement_not_own_single_scope";return false;}auto& own=*it->second;
    const auto* record=own.retirement.record();if(!record||!root_retirement_){error="remove_runtime_actual_retirement_record_or_Root_service_missing";return false;}
    if(own.backend.busy()||!own.adapter.observation_scope_done()||own.services.observation().active){error="remove_runtime_actual_retirement_backend/adapter/observer_scope_active";return false;}
    if(!own.retirement.validate_allocation(allocation,receipt,error))return false;
    try {
        uint32_t seh=0;if(!verify_root_revocation_guarded(*root_retirement_,own.request,record->root,error,seh)){if(seh)error="remove_runtime_actual_Root_retirement_validation_SEH="+std::to_string(seh);else if(error.empty())error="remove_runtime_actual_Root_retirement_validation_failed_without_cause";return false;}
        host::Reader reader(memory_,origin_,installation_.image,main_thread_.load(),lookup_);host::WorldState world;if(!reader.current(world,true,error))return false;
        const lease::World current{world.origin.world_epoch,world.origin.generation,world.origin.world,world.origin.map};
        if(current!=record->quiescence.world){error="remove_runtime_actual_current_world_changed_after_sealed_quiescence";return false;}return true;
    }catch(const std::exception& ex){error=std::string("remove_runtime_actual_retirement_validation_exception: ")+ex.what();return false;}catch(...){error="remove_runtime_actual_retirement_validation_nonstandard_exception";return false;}
}
bool Runtime::callback_operation()const noexcept{return main_thread_.load()&&GetCurrentThreadId()==main_thread_.load()&&active_!=nullptr;}
void Runtime::observation_failed(const char* cause)noexcept{
    if(!hook_error_[0]){const auto n=(std::min)(std::strlen(cause),hook_error_.size()-1);std::memcpy(hook_error_.data(),cause,n);hook_error_[n]=0;}
    free_runtime().journal().error("error:remove-observation",cause,0);
}
bool Runtime::forward_clear(uintptr_t receiver,uintptr_t caller)noexcept{
    if(!callback_operation())return true;
    if(receiver!=active_->context.selection||(caller!=0x2d4351&&caller!=0x2d4b62))return true;
    try{
        host::Reader reader(memory_,origin_,installation_.image,main_thread_.load(),lookup_);host::WorldState world;std::string error;
        if(!reader.current(world,false,error)){observation_failed(error.c_str());return true;}
        const auto pool=active_->adapter.status();
        if(!host::same_world(active_->origin,world.origin)||!active_->services.observation().active||pool.unknown_lock||!(pool.held+pool.tombstones)){observation_failed("remove_runtime_actual_Clear_scope_or_continuous_lease_changed");return true;}
        native::RemoveContext context{world.origin.world_epoch,GetCurrentThreadId(),uintptr_t(world.origin.world),world.selection,active_->request.object,sha_==native_uid::required_exe_sha,true,world.saves==0,true,true};
        return active_->backend.should_forward_clear(context,receiver,caller);
    }catch(const std::exception& ex){observation_failed(ex.what());return true;}catch(...){observation_failed("remove_runtime_Clear_observation_nonstandard_exception");return true;}
}
void Runtime::open_returned(uintptr_t caller,uintptr_t graph)noexcept{if(!callback_operation())return;try{active_->services.undo_open_returned(caller,graph);}catch(const std::exception& ex){observation_failed(ex.what());}catch(...){observation_failed("remove_runtime_Open_observation_nonstandard_exception");}}
void Runtime::lock_enter(uintptr_t caller,uintptr_t graph)noexcept{if(!callback_operation())return;try{active_->services.graph_lock_enter(caller,graph);}catch(const std::exception& ex){observation_failed(ex.what());}catch(...){observation_failed("remove_runtime_Lock_enter_observation_nonstandard_exception");}}
void Runtime::lock_returned(uintptr_t graph,bool returned)noexcept{if(!callback_operation())return;try{active_->services.graph_lock_returned(graph,returned);}catch(const std::exception& ex){observation_failed(ex.what());}catch(...){observation_failed("remove_runtime_Lock_return_observation_nonstandard_exception");}}
void Runtime::destructor_enter(uintptr_t prop)noexcept{if(!callback_operation())return;try{active_->services.prop_destructor_enter(prop);}catch(const std::exception& ex){observation_failed(ex.what());}catch(...){observation_failed("remove_runtime_Dtor_observation_nonstandard_exception");}}
void Runtime::original_failed(const char* operation,const char* cause,uint32_t seh)noexcept{original_fault_.store(true,std::memory_order_release);free_runtime().journal().error(operation,cause,seh);}
InstallationReceipt Runtime::status()const{const auto bound=main_thread_.load();if(!bound)return {};if(GetCurrentThreadId()!=bound){InstallationReceipt out;out.phase=RuntimePhase::Failed;out.cause="remove_runtime_status_wrong_thread";return out;}auto out=installation_;out.inflight=dispatch_.inflight();return out;}
const OperationStatus* Runtime::operation_status(uint64_t sequence)const{if(!main_thread_.load()||GetCurrentThreadId()!=main_thread_.load())return nullptr;return ledger_.find(sequence);}
bool Runtime::can_reclaim_global_minhook(std::string& error)const{
    const auto bound=main_thread_.load();if(!bound)return true;
    if(GetCurrentThreadId()!=bound){error="remove_runtime_reclaim_wrong_thread";return false;}
    if(installation_.created_mask){error="remove_runtime_created_MinHook_records_forbid_global_MH_cleanup";return false;}
    for(auto trampoline:installation_.trampolines)if(trampoline){error="remove_runtime_process_lifetime_originals_and_retained_pool_forbid_global_MH_cleanup";return false;}
    if(dispatch_.published()||dispatch_.inflight()||!operations_.empty()){error="remove_runtime_published_or_retained_ownership_forbids_reclamation";return false;}return true;
}
}

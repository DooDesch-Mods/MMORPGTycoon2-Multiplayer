#include "native_free.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cstring>
#include <exception>

namespace mt2mp::native_free {
namespace {
struct ThreadScope {Observer* sized_owner=nullptr;uint32_t sized_depth=0,free_depth=0;};
thread_local ThreadScope scope;
int record_fault(Observer* observer,EXCEPTION_POINTERS* ex,const char* operation,bool owned)noexcept{
    observer->original_failed(operation,ex->ExceptionRecord->ExceptionCode,uintptr_t(ex->ExceptionRecord->ExceptionAddress),owned);
    return EXCEPTION_CONTINUE_SEARCH;
}
int delete_fault(Observer* observer,EXCEPTION_POINTERS* ex)noexcept{
    const auto code=ex->ExceptionRecord->ExceptionCode;
    observer->original_failed("own-D0",code,uintptr_t(ex->ExceptionRecord->ExceptionAddress),true);
    return code==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER;
}
__declspec(noinline) bool run_delete(Observer* observer,DeleteFn fn,void* p,uintptr_t* origin){
    __try{mt2mp_free_delete_bridge(fn,p,origin);return true;}
    __except(delete_fault(observer,GetExceptionInformation())){return false;}
}
__declspec(noinline) void run_sized(Observer* observer,SizedFn fn,void* p,uint64_t bytes,uintptr_t* origin,bool owned){
    const auto previous=scope;
    ++scope.sized_depth;if(owned)scope.sized_owner=observer;else scope.sized_owner=nullptr;
    __try{
        __try{mt2mp_free_sized_bridge(fn,p,bytes,origin);observer->sized_returned(owned);}
        __except(record_fault(observer,GetExceptionInformation(),"sized-delete-original",owned)){
            observer->original_failed("error: continue-search filter entered handler",ERROR_INVALID_STATE,0,owned);
            RaiseFailFastException(nullptr,nullptr,0);
        }
    }__finally{scope=previous;observer->original_scope_ended();}
}
__declspec(noinline) void run_free(Observer* observer,FreeFn fn,void* p,bool owned){
    const auto previous=scope.free_depth;++scope.free_depth;
    __try{
        __try{fn(p);observer->free_returned(owned);}
        __except(record_fault(observer,GetExceptionInformation(),"CRT-free-original",owned)){
            observer->original_failed("error: continue-search filter entered handler",ERROR_INVALID_STATE,0,owned);
            RaiseFailFastException(nullptr,nullptr,0);
        }
    }__finally{scope.free_depth=previous;observer->original_scope_ended();}
}
}
void Observer::cause(Receipt& r,const char* text)noexcept{
    if(!text)text="error: original cause absent";const auto count=std::strlen(text);r.cause_truncated=count>=r.cause.size();const auto n=count<r.cause.size()-1?count:r.cause.size()-1;std::memcpy(r.cause.data(),text,n);r.cause[n]=0;
}
void Observer::unknown(const char* text,uint32_t code,uintptr_t address)noexcept{
    current_.phase=Phase::Unknown;current_.seh_code=code;current_.fault_address=address;cause(current_,text);unknown_lock_=true;
}
bool Observer::publish_disabled(SizedFn sized,FreeFn free)noexcept{
    if(enabled()||active_||GetCurrentThreadId()!=main_thread_||!sized||!free){diagnostics_.error("publish-disabled","error: original trampoline publication preconditions failed",0);return false;}
    if(sized_.load(std::memory_order_acquire)||free_.load(std::memory_order_acquire)){diagnostics_.error("publish-disabled","error: original pointers already published; replacement forbidden",0);return false;}
    sized_.store(sized,std::memory_order_release);free_.store(free,std::memory_order_release);return true;
}
bool Observer::confirm_hooks(uint32_t mask)noexcept{
    if(GetCurrentThreadId()!=main_thread_||enabled()||active_||!sized_.load(std::memory_order_acquire)||!free_.load(std::memory_order_acquire)||mask!=required_hooks){diagnostics_.error("confirm-hooks","error: typed sized/free hooks are not fully confirmed before enable",0);return false;}
    hook_mask_=mask;return true;
}
bool Observer::enable()noexcept{
    if(GetCurrentThreadId()!=main_thread_||active_||hook_mask_!=required_hooks||!sized_.load(std::memory_order_acquire)||!free_.load(std::memory_order_acquire)){diagnostics_.error("enable","error: native free originals and complete hook mask required",0);return false;}
    enabled_.store(true,std::memory_order_release);return true;
}
bool Observer::disable()noexcept{
    if(GetCurrentThreadId()!=main_thread_||active_||in_delete_){diagnostics_.error("disable","error: current owned native delete is not retired",0);return false;}
    enabled_.store(false,std::memory_order_release);return true;
}
bool Observer::begin(const Permit& permit)noexcept{
    if(GetCurrentThreadId()!=main_thread_||!enabled()||active_||in_delete_||unknown_lock_||!permit.root_binding_confirmed||!permit.holder||permit.bytes!=56||!permit.call||permit.call<=last_call_||!permit.allocation_generation||!permit.world_generation||!permit.retirement_receipt||permit.world_generation<last_world_){diagnostics_.error("begin-free-permit","error: current own holder56/allocation/world/call/retirement binding rejected",0);return false;}
    current_={};current_.permit=permit;current_.phase=Phase::Prepared;last_call_=permit.call;last_world_=permit.world_generation;expected_sized_origin_=expected_free_origin_=0;active_=true;return true;
}
bool Observer::invoke_delete(DeleteFn fn,const Permit& permit)noexcept{
    if(!fn||!active_||in_delete_||GetCurrentThreadId()!=main_thread_||current_.permit!=permit||current_.delete_attempted){diagnostics_.error("invoke-own-D0","error: matching current own delete permit unavailable; native D0 never ran",0);return false;}
    in_delete_=true;current_.phase=Phase::Attempted;current_.delete_attempted=1;
    bool returned=false;
    try{returned=run_delete(this,fn,permit.holder,&expected_sized_origin_);}
    catch(const std::exception& ex){original_failed(ex.what(),0,0,true);}
    catch(...){original_failed("error: non-standard own-D0 original exception",0,0,true);}
    in_delete_=false;active_=false;
    if(!returned)return false;
    current_.delete_returned=1;
    if(current_.phase==Phase::Unknown)return false;
    if(current_.sized_attempted!=1||current_.sized_returned!=1||current_.free_attempted!=1||current_.free_returned!=1){unknown("error: own D0 returned without exactly one current typed sized/free OriginalOnce receipt");return false;}
    current_.phase=Phase::Confirmed;cause(current_,"info: current own native holder56 sized-delete and CRT-free receipts confirmed");return true;
}
void Observer::sized_event(void* p,uint64_t bytes,uintptr_t origin){
    original_inflight_.fetch_add(1,std::memory_order_acq_rel);
    const auto original=sized_.load(std::memory_order_acquire);
    if(!original){original_scope_ended();diagnostics_.error("sized-delete-original","error: original unavailable; call never ran because hook installation violated publication contract",0);RaiseException(0xe123fa01,0,0,nullptr);return;}
    const bool owned=GetCurrentThreadId()==main_thread_&&enabled()&&scope.sized_depth==0&&active_&&in_delete_&&current_.phase!=Phase::Unknown&&p==current_.permit.holder&&bytes==56&&origin==expected_sized_origin_&&expected_sized_origin_!=0;
    if(owned){++current_.sized_attempted;current_.sized_origin=origin;if(current_.sized_attempted!=1)unknown("error: multiple matching sized deletes in one current permit");}
    try{run_sized(this,original,p,bytes,owned?&expected_free_origin_:nullptr,owned);}
    catch(const std::exception& ex){original_failed(ex.what(),0,0,owned);throw;}
    catch(...){original_failed("error: non-standard sized-delete original exception",0,0,owned);throw;}
}
void Observer::free_event(void* p,uintptr_t origin){
    original_inflight_.fetch_add(1,std::memory_order_acq_rel);
    const auto original=free_.load(std::memory_order_acquire);
    if(!original){original_scope_ended();diagnostics_.error("CRT-free-original","error: original unavailable; call never ran because hook installation violated publication contract",0);RaiseException(0xe123fa02,0,0,nullptr);return;}
    const bool owned=GetCurrentThreadId()==main_thread_&&enabled()&&scope.free_depth==0&&scope.sized_depth==1&&scope.sized_owner==this&&active_&&in_delete_&&current_.phase!=Phase::Unknown&&p==current_.permit.holder&&origin==expected_free_origin_&&expected_free_origin_!=0;
    if(owned){++current_.free_attempted;current_.free_origin=origin;if(current_.free_attempted!=1)unknown("error: multiple matching CRT frees in one current permit");}
    try{run_free(this,original,p,owned);}
    catch(const std::exception& ex){original_failed(ex.what(),0,0,owned);throw;}
    catch(...){original_failed("error: non-standard CRT-free original exception",0,0,owned);throw;}
}
void Observer::original_failed(const char* text,uint32_t code,uintptr_t address,bool owned)noexcept{
    diagnostics_.error("native-free-original",text,code);
    if(owned){
        if(current_.phase!=Phase::Unknown)unknown(text,code,address);
        else if(!code)cause(current_,text);
        else if(!current_.seh_code){current_.seh_code=code;current_.fault_address=address;}
    }
}
void Observer::sized_returned(bool owned)noexcept{if(owned)++current_.sized_returned;}
void Observer::free_returned(bool owned)noexcept{if(owned)++current_.free_returned;}
Availability Observer::availability()const noexcept{
    Availability out;out.main_thread=GetCurrentThreadId()==main_thread_;
    if(!out.main_thread)return out;
    out.enabled=enabled();out.active=active_;out.in_delete=in_delete_;out.unknown_locked=unknown_lock_;
    if(unknown_lock_){out.seh_code=current_.seh_code;out.fault_address=current_.fault_address;out.cause=current_.cause;}
    return out;
}
Receipt Observer::observe(const Permit& permit)const noexcept{if(GetCurrentThreadId()!=main_thread_||current_.permit!=permit)return {};return current_;}
bool Observer::resync(uint64_t generation,bool retired)noexcept{
    if(GetCurrentThreadId()!=main_thread_||active_||in_delete_||!retired||generation<=last_world_){diagnostics_.error("free-observer-resync","error: actual new snapshot world and old callback retirement required",0);return false;}
    current_={};unknown_lock_=false;last_world_=generation;return true;
}
}

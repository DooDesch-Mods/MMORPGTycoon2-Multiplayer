#include "prop_lease.h"
#include "native_contract_bytes.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>

namespace mt2mp::prop_lease {
namespace {
struct Holder {uintptr_t vtable=0,context=0,bound=0;uintptr_t unused=0,weak_vtable=0,raw=0,proxy=0;};
static_assert(sizeof(Holder)==0x38);
struct Proxy {uintptr_t target=0;int32_t references=0;uint32_t padding=0;};
struct Busy {bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}};
// Separate POD-only frames allow Windows SEH without C++ unwind objects.
__declspec(noinline) bool copy_native(const void* from,void* to,size_t bytes,uint32_t& fault)noexcept{
    __try{std::memcpy(to,from,bytes);return true;}
    __except((fault=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER)){return false;}
}
__declspec(noinline) bool create_native(FactoryFn fn,void*& result,uint32_t& fault){
    __try{result=fn(nullptr);return true;}
    __except((GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:(fault=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER))){return false;}
}
__declspec(noinline) bool assign_native(AssignFn fn,void* holder,void* object,uint32_t& fault){
    __try{fn(holder,object);return true;}
    __except((GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:(fault=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER))){return false;}
}
__declspec(noinline) bool delete_via_host(Host* host,DeleteFn genuine,const PrivateLease* lease,const Retirement* retired,std::string* error,uint32_t& fault){
    __try{return host->invoke_native_delete_once(genuine,*lease,*retired,*error,fault);}
    __except((GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:(fault=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER))){return false;}
}
template<class T>bool at(void* base,size_t offset,T& out,uint32_t& fault)noexcept{
    const auto address=uintptr_t(base);if(!address||address>std::numeric_limits<uintptr_t>::max()-offset){fault=ERROR_ARITHMETIC_OVERFLOW;return false;}
    return copy_native(reinterpret_cast<const void*>(address+offset),&out,sizeof out,fault);
}
bool same_world(const Proof& p,const World* w){return p.world.epoch&&p.world.origin_generation&&p.world.world&&p.world.map&&(!w||p.world==*w);}
}
Api for_module(uintptr_t base)noexcept{
    if(!base||base>std::numeric_limits<uintptr_t>::max()-0x17119f0)return {};
    return {reinterpret_cast<FactoryFn>(base+0xd8a940),reinterpret_cast<AssignFn>(base+0x68310),reinterpret_cast<DeleteFn>(base+0xbedc00),base+0x17119f0,base+0x16f9e60};
}
bool validate_native_api(uintptr_t base,const Api& api,std::string& error){
    const auto expected=for_module(base);
    if(!expected.create||api.create!=expected.create||api.assign!=expected.assign||api.destroy!=expected.destroy||api.link_addresspoint!=expected.link_addresspoint||api.weak_addresspoint!=expected.weak_addresspoint){error="error: native lease API addresses differ from exact module ABI";return false;}
    for(const auto& entry:contract::entries){
        const auto address=base+entry.rva;
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof region)||region.AllocationBase!=reinterpret_cast<void*>(base)||region.Type!=MEM_IMAGE||region.State!=MEM_COMMIT||region.Protect!=PAGE_EXECUTE_READ){error="error: native lease code is not immutable committed executable image";return false;}
        unsigned char actual[256]{};uint32_t fault=0;
        if(!copy_native(reinterpret_cast<const void*>(address),actual,entry.count,fault)){error="error: native lease full code guard read SEH="+std::to_string(fault);return false;}
        if(std::memcmp(actual,entry.bytes,entry.count)){error="error: native lease factory/assign/destructor entrybytes differ";return false;}
    }
    for(auto ap:{api.link_addresspoint,api.weak_addresspoint}){
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<const void*>(ap),&region,sizeof region)||region.AllocationBase!=reinterpret_cast<void*>(base)||region.Type!=MEM_IMAGE||region.State!=MEM_COMMIT||region.Protect!=PAGE_READONLY){error="error: native weak vtable is not immutable committed image metadata";return false;}
        uintptr_t slots[2]{};uint32_t fault=0;
        if(!copy_native(reinterpret_cast<const void*>(ap),slots,sizeof slots,fault)){error="error: native weak vtable read SEH="+std::to_string(fault);return false;}
        const auto d1=base+(ap==api.link_addresspoint?contract::link_d1:contract::weak_d1);
        const auto d0=base+(ap==api.link_addresspoint?contract::link_d0:contract::weak_d0);
        if(slots[0]!=d1||slots[1]!=d0){error="error: native weak vtable destructor slots differ";return false;}
    }
    return true;
}
void Pool::cause(Receipt& r,const char* text)noexcept{
    if(!text)text="error: absent original failure cause";
    const auto bytes=std::strlen(text);r.cause_truncated=bytes>=r.cause.size();
    const auto count=(bytes<r.cause.size()-1)?bytes:r.cause.size()-1;std::memcpy(r.cause.data(),text,count);r.cause[count]=0;
}
Receipt Pool::error_receipt(Phase phase,const char* text)noexcept{Receipt r;r.phase=phase;cause(r,text);return r;}
void Pool::unknown(Record& r,const char* text,uint32_t seh)noexcept{unknown_lock_=true;r.receipt.phase=Phase::Unknown;r.receipt.seh_code=seh;cause(r.receipt,text);}
bool Pool::proof(Host& host,const World* expected,std::string& error){
    if(!api_.create||!api_.assign||!api_.destroy||!api_.link_addresspoint||!api_.weak_addresspoint){error="error: native factory/assign/delete ABI is incomplete";return false;}
    if(!host.validate_exact_api(api_,error))return false;
    Proof p;if(!host.current(p,error))return false;
    if(!p.main_thread){error="error: native lease operation is outside bound Mainthread";return false;}
    if(!p.current_native_open){error="error: NativeOpen is not current";return false;}
    if(!p.effects_idle){error="info: native effects are not idle";return false;}
    if(!same_world(p,expected)){error="error: native lease world witness differs";return false;}
    return true;
}
bool Pool::checked_live(Record& r,Host& host,bool allow_detached,std::string& error){
    Holder holder;Proxy proxy;uint32_t fault=0;
    if(!at(r.lease.holder,0,holder,fault)){error="error: native holder read failed SEH="+std::to_string(fault);return false;}
    if(holder.vtable!=api_.link_addresspoint||holder.weak_vtable!=api_.weak_addresspoint||holder.context||holder.bound||holder.raw!=uintptr_t(r.lease.object)||holder.proxy!=uintptr_t(r.lease.proxy)){
        error="error: retained native holder fields differ";return false;
    }
    if(!at(r.lease.proxy,0,proxy,fault)){error="error: native retained proxy read failed SEH="+std::to_string(fault);return false;}
    if(proxy.references<=0){error="error: native retained proxy references actual="+std::to_string(proxy.references)+" required_min=1";return false;}
    if(!proxy.target){if(allow_detached)return true;error="error: retained native lifetime is detached";return false;}
    if(uintptr_t(r.lease.object)>std::numeric_limits<uintptr_t>::max()-0x20||proxy.target!=uintptr_t(r.lease.object)+0x20){error="error: retained native weak target differs";return false;}
    Prop b;if(!host.read_prop(r.lease.object,r.lease.uid,b,error))return false;
    if(!b.typed_prop||!b.complete_ownership||b.uid!=r.lease.uid||b.registry_object!=r.lease.object||b.class_addresspoint!=r.lease.class_addresspoint||b.membership_receipt.empty()){error="error: native full64 UID/class/membership/registry binding differs";return false;}
    uintptr_t current_proxy=0,actual_class=0;uint64_t actual_uid=0;
    if(!at(r.lease.object,0x28,current_proxy,fault)||!at(r.lease.object,0x18,actual_uid,fault)||!at(r.lease.object,0,actual_class,fault)){error="error: native prop field read failed SEH="+std::to_string(fault);return false;}
    if(current_proxy!=holder.proxy||actual_uid!=r.lease.uid||actual_class!=r.lease.class_addresspoint){error="error: current prop UID/class/proxy differs from retained lifetime";return false;}
    return true;
}
Receipt Pool::acquire(const std::string& key,void* object,uint64_t uid,const World& world,Host& host){
    if(busy_)return error_receipt(Phase::Failed,"error: native lease acquisition reentrancy rejected before callbacks");Busy busy(busy_);
    Record* record=nullptr;
    try{
        const auto old=records_.find(key);
        if(old!=records_.end()){
            if(old->second.lease.object!=object||old->second.lease.uid!=uid||old->second.lease.world!=world)return error_receipt(Phase::Failed,"error: native lease key payload conflict; original receipt retained");
            return old->second.receipt;
        }
        if(unknown_lock_)return error_receipt(Phase::Failed,"error: unknown native effect blocks lease acquisition");
        if(key.empty()||!object||!uid)return error_receipt(Phase::Failed,"error: native lease key/object/UID missing");
        std::string error;if(!proof(host,&world,error))return error_receipt(Phase::Failed,error.c_str());
        Prop b;if(!host.read_prop(object,uid,b,error))return error_receipt(Phase::Failed,error.c_str());
        if(!b.typed_prop||!b.complete_ownership||b.registry_object!=object||b.uid!=uid||!b.class_addresspoint||b.membership_receipt.empty())return error_receipt(Phase::Failed,"error: preflight native UID/class/membership/registry differs");
        uintptr_t initial_proxy=0,initial_class=0;uint64_t initial_uid=0;uint32_t fault=0;Proxy before;
        if(!at(object,0x28,initial_proxy,fault)||!at(object,0x18,initial_uid,fault)||!at(object,0,initial_class,fault)){auto r=error_receipt(Phase::Failed,"error: native object preflight read failed");r.seh_code=fault;return r;}
        if(initial_uid!=uid||initial_class!=b.class_addresspoint)return error_receipt(Phase::Failed,"error: full64 preflight native UID/class differs");
        if(initial_proxy){
            if(!at(reinterpret_cast<void*>(initial_proxy),0,before,fault)){auto r=error_receipt(Phase::Failed,"error: native proxy preflight read failed");r.seh_code=fault;return r;}
            if(uintptr_t(object)>std::numeric_limits<uintptr_t>::max()-0x20||before.target!=uintptr_t(object)+0x20||before.references<0||before.references==INT32_MAX)
                return error_receipt(Phase::Failed,"error: native proxy target/refcount preflight invalid or overflow");
        }
        uint64_t generation=0;if(!host.reserve_generation(generation,error)||!generation)return error_receipt(Phase::Failed,error.empty()?"error: unique own-holder generation absent":error.c_str());
        for(const auto& item:records_)if(item.second.lease.generation==generation)return error_receipt(Phase::Failed,"error: own-holder generation was already reserved");
        Record fresh;fresh.lease={nullptr,object,nullptr,uid,world,generation,b.class_addresspoint};fresh.receipt.phase=Phase::Prepared;
        record=&records_.emplace(key,std::move(fresh)).first->second;
        record->receipt.factory_attempted=1;
        if(!create_native(api_.create,record->lease.holder,fault)){unknown(*record,"error: native factory SEH after attempted allocation",fault);return record->receipt;}
        record->receipt.factory_returned=1;record->receipt.phase=Phase::Allocated;
        Holder allocated;
        if(!at(record->lease.holder,0,allocated,fault)||allocated.vtable!=api_.link_addresspoint||allocated.weak_vtable!=api_.weak_addresspoint||allocated.context||allocated.bound||allocated.raw||allocated.proxy){
            unknown(*record,"error: native factory constructor readback differs or unreadable",fault);return record->receipt;
        }
        if(!host.allocation_returned(record->lease,error)||!proof(host,&world,error)){unknown(*record,error.c_str());return record->receipt;}
        Prop immediate;if(!host.read_prop(object,uid,immediate,error)){unknown(*record,error.c_str());return record->receipt;}
        if(!immediate.typed_prop||!immediate.complete_ownership||immediate.registry_object!=object||immediate.uid!=uid||immediate.class_addresspoint!=b.class_addresspoint||immediate.membership_receipt.empty()){unknown(*record,"error: Prop UID/class/membership/registry changed before first retain");return record->receipt;}
        uintptr_t immediate_proxy=0,immediate_class=0;uint64_t immediate_uid=0;
        if(!at(object,0x28,immediate_proxy,fault)||!at(object,0,immediate_class,fault)||!at(object,0x18,immediate_uid,fault)){unknown(*record,"error: Prop pre-retain field read failed",fault);return record->receipt;}
        if(immediate_proxy!=initial_proxy||immediate_class!=initial_class||immediate_uid!=initial_uid){unknown(*record,"error: Prop UID/class/proxy changed before first retain");return record->receipt;}
        if(immediate_proxy){Proxy current;if(!at(reinterpret_cast<void*>(immediate_proxy),0,current,fault)){unknown(*record,"error: Prop pre-retain proxy read failed",fault);return record->receipt;}
            if(current.target!=before.target||current.references!=before.references){unknown(*record,"error: Prop proxy lifetime/refcount changed before first retain");return record->receipt;}}
        record->receipt.retain_attempted=1;
        if(!assign_native(api_.assign,reinterpret_cast<void*>(uintptr_t(record->lease.holder)+0x20),object,fault)){unknown(*record,"error: native weak assignment SEH after attempted retain",fault);return record->receipt;}
        Holder held;Proxy after;
        if(!at(record->lease.holder,0,held,fault)||!held.proxy||!at(reinterpret_cast<void*>(held.proxy),0,after,fault)){
            unknown(*record,"error: native retained holder/proxy readback failed",fault);return record->receipt;
        }
        record->lease.proxy=reinterpret_cast<void*>(held.proxy);
        if((initial_proxy&&held.proxy!=initial_proxy)||after.references!=before.references+1||!checked_live(*record,host,false,error)||!proof(host,&world,error)){
            unknown(*record,error.empty()?"error: native retain did not increment exactly one owned reference":error.c_str());return record->receipt;
        }
        record->receipt.retain_confirmed=1;record->receipt.phase=Phase::Held;cause(record->receipt,"info: native factory allocation and own weak retain confirmed");return record->receipt;
    }catch(const std::exception& ex){if(record){unknown(*record,ex.what());return record->receipt;}return error_receipt(Phase::Failed,ex.what());}
    catch(...){if(record){unknown(*record,"error: unknown native lease acquisition exception");return record->receipt;}return error_receipt(Phase::Failed,"error: unknown native lease preflight exception");}
}
bool Pool::read(const std::string& key,PrivateLease& lease,Host& host,std::string& error){
    lease={};if(busy_){error="error: native lease read reentrancy rejected";return false;}Busy busy(busy_);
    try{
        const auto it=records_.find(key);if(it==records_.end()||it->second.receipt.phase!=Phase::Held||unknown_lock_){error="error: own native lease is not held or authority is locked";return false;}
        if(!proof(host,&it->second.lease.world,error)||!checked_live(it->second,host,false,error)||!proof(host,&it->second.lease.world,error))return false;
        lease=it->second.lease;return true;
    }catch(const std::exception& ex){error=std::string("error: native lease read exception: ")+ex.what();return false;}
    catch(...){error="error: unknown native lease read exception";return false;}
}
Receipt Pool::tombstone(const std::string& key,Host& host){
    if(busy_)return error_receipt(Phase::Failed,"error: native lease tombstone reentrancy rejected");Busy busy(busy_);
    try{
        const auto it=records_.find(key);if(it==records_.end())return error_receipt(Phase::Failed,"error: tombstone has no owned native lease");auto& r=it->second;
        if(unknown_lock_||(r.receipt.phase!=Phase::Held&&r.receipt.phase!=Phase::Tombstone))return error_receipt(Phase::Failed,"error: tombstone native lease is not held");
        std::string error;if(!proof(host,&r.lease.world,error)||!checked_live(r,host,true,error))return error_receipt(Phase::Failed,error.c_str());
        if(!proof(host,&r.lease.world,error))return error_receipt(Phase::Failed,error.c_str());
        Proxy proxy;uint32_t fault=0;
        if(!at(r.lease.proxy,0,proxy,fault)){auto failure=error_receipt(Phase::Failed,"error: removed lifetime detached proxy read failed");failure.seh_code=fault;return failure;}
        if(proxy.target)return error_receipt(Phase::Failed,"error: removed lifetime proxy target is still alive");
        if(!proof(host,&r.lease.world,error))return error_receipt(Phase::Failed,error.c_str());
        r.receipt.phase=Phase::Tombstone;cause(r.receipt,"info: own native weak lease retained for detached tombstone");return r.receipt;
    }catch(const std::exception& ex){return error_receipt(Phase::Failed,ex.what());}
    catch(...){return error_receipt(Phase::Failed,"error: unknown tombstone reader exception");}
}
Receipt Pool::release(const std::string& key,const Retirement& retired,Host& host){
    if(busy_)return error_receipt(Phase::Failed,"error: native lease release reentrancy rejected");Busy busy(busy_);Record* record=nullptr;
    try{
        const auto it=records_.find(key);if(it==records_.end())return error_receipt(Phase::Failed,"error: release has no owned native lease");record=&it->second;auto& r=*record;
        if(r.receipt.phase==Phase::Released)return r.receipt;
        if(unknown_lock_)return error_receipt(Phase::Failed,"error: Prop unknown native effect blocks further release or deletion stores");
        if(r.receipt.phase!=Phase::Held&&r.receipt.phase!=Phase::Tombstone)return error_receipt(Phase::Failed,"error: release requires a known continuously owned native lease");
        std::string error;if(!retired.receipt||retired.snapshot_receipt.empty()||retired.old_world!=r.lease.world||!host.validate_retirement(r.lease,retired,error))return error_receipt(Phase::Failed,error.empty()?"error: actual snapshot and all old operations are not retired":error.c_str());
        if(!proof(host,nullptr,error))return error_receipt(Phase::Failed,error.c_str());
        Holder held;Proxy proxy;uint32_t fault=0;
        if(!at(r.lease.holder,0,held,fault)||held.vtable!=api_.link_addresspoint||held.weak_vtable!=api_.weak_addresspoint||held.context||held.bound||held.raw!=uintptr_t(r.lease.object)||held.proxy!=uintptr_t(r.lease.proxy)||!at(r.lease.proxy,0,proxy,fault)||proxy.references<=0)
            {auto failure=error_receipt(Phase::Failed,"error: own native lease release preflight readback failed");failure.seh_code=fault;return failure;}
        r.receipt.release_attempted=1;
        if(!assign_native(api_.assign,reinterpret_cast<void*>(uintptr_t(r.lease.holder)+0x20),nullptr,fault)){unknown(r,"error: native weak release SEH after attempted decrement",fault);return r.receipt;}
        Holder detached;
        // Only holder storage is read here. The last release can already free the proxy.
        if(!at(r.lease.holder,0,detached,fault)||detached.vtable!=api_.link_addresspoint||detached.weak_vtable!=api_.weak_addresspoint||detached.raw||detached.proxy){unknown(r,"error: native release holder readback differs or unreadable",fault);return r.receipt;}
        r.receipt.release_confirmed=1;r.receipt.phase=Phase::Detached;
        if(!proof(host,nullptr,error)){unknown(r,error.c_str());return r.receipt;}
        r.receipt.delete_attempted=1;
        if(!delete_via_host(&host,api_.destroy,&r.lease,&retired,&error,fault)){unknown(r,error.empty()?"error: own native D0 backend did not confirm current invocation":error.c_str(),fault);return r.receipt;}
        // Native free acknowledgement is an event receipt, never a read of freed storage.
        if(!host.confirm_native_delete(r.lease,retired,error)){unknown(r,error.c_str());return r.receipt;}
        r.receipt.delete_confirmed=1;r.receipt.phase=Phase::Released;cause(r.receipt,"info: own native weak reference and native holder deletion confirmed");return r.receipt;
    }catch(const std::exception& ex){if(record&&(record->receipt.release_attempted||record->receipt.delete_attempted)){unknown(*record,ex.what());return record->receipt;}return error_receipt(Phase::Failed,ex.what());}
    catch(...){if(record&&(record->receipt.release_attempted||record->receipt.delete_attempted)){unknown(*record,"error: unknown native lease release exception");return record->receipt;}return error_receipt(Phase::Failed,"error: unknown native release preflight exception");}
}
Receipt Pool::observe(const std::string& key)const{const auto it=records_.find(key);return it==records_.end()?Receipt{}:it->second.receipt;}
bool Pool::inspect(const std::string& key,PrivateLease& lease,Receipt& receipt)const{
    lease={};receipt={};const auto it=records_.find(key);if(it==records_.end())return false;lease=it->second.lease;receipt=it->second.receipt;return true;
}
bool Pool::enumerate_private(std::vector<PrivatePoolRecord>& out,std::string& error)const{
    out.clear();error.clear();if(busy_){error="error: private Prop pool enumeration while busy";return false;}
    try {std::vector<PrivatePoolRecord> snapshot;snapshot.reserve(records_.size());for(const auto& [key,r]:records_)snapshot.push_back({key,r.lease,r.receipt});out=std::move(snapshot);return true;}
    catch(const std::exception& ex){error=std::string("error: private Prop enumeration: ")+ex.what();return false;}
    catch(...){error="error: private Prop enumeration nonstandard exception";return false;}
}
bool Pool::read_owned_private(const std::string& key,PrivateOwnershipRead& out,Host& host,std::string& error){
    out={};error.clear();if(busy_){error="error: private Prop ownership read while busy";return false;}Busy busy(busy_);
    try {
        const auto it=records_.find(key);if(it==records_.end()||unknown_lock_){error="error: private Prop ownership absent or Unknown";return false;}
        auto& r=it->second;const bool dead=r.receipt.phase==Phase::Tombstone;
        if(!dead&&r.receipt.phase!=Phase::Held){error="error: private Prop ownership is not continuously held";return false;}
        if(!proof(host,nullptr,error))return false;
        PrivateOwnershipRead actual;if(!detail::read_owned_fields(api_,r.lease,r.receipt.phase,actual,error))return false;
        if(!proof(host,nullptr,error))return false;
        out=actual;return true;
    }catch(const std::exception& ex){error=std::string("error: private Prop ownership read: ")+ex.what();return false;}
    catch(...){error="error: private Prop ownership read nonstandard exception";return false;}
}
bool detail::read_owned_fields(const Api& api,const PrivateLease& lease,Phase phase,PrivateOwnershipRead& out,std::string& error){
    out={};error.clear();if(phase!=Phase::Held&&phase!=Phase::Tombstone){error="error: private Prop ownership field read is not Held/Tombstone";return false;}
    Holder holder;Proxy proxy;uint32_t fault=0;
    if(!at(lease.holder,0,holder,fault)||holder.vtable!=api.link_addresspoint||holder.weak_vtable!=api.weak_addresspoint||holder.context||holder.bound||holder.raw!=uintptr_t(lease.object)||holder.proxy!=uintptr_t(lease.proxy)||!at(lease.proxy,0,proxy,fault)){error="error: private Prop ownership holder/proxy read SEH="+std::to_string(fault);return false;}
    if(proxy.references<=0||(phase==Phase::Tombstone&&proxy.target)||(proxy.target&&(uintptr_t(lease.object)>UINTPTR_MAX-0x20||proxy.target!=uintptr_t(lease.object)+0x20))){error="error: private Prop ownership target or positive own reference changed";return false;}
    out={lease,phase,proxy.target,proxy.references};return true;
}
Status Pool::status()const{Status s;s.leases=records_.size();s.unknown_lock=unknown_lock_;for(const auto& [key,r]:records_){s.held+=r.receipt.phase==Phase::Held;s.tombstones+=r.receipt.phase==Phase::Tombstone;s.released+=r.receipt.phase==Phase::Released;}return s;}
Pool::~Pool()noexcept{
    size_t unretired=0;for(const auto& [key,r]:records_)unretired+=r.receipt.factory_attempted&&r.receipt.phase!=Phase::Released;
    if(unretired)diagnostics_.error_unretired_native_ownership(unretired,unknown_lock_);
}
}

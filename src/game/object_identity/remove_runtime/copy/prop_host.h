#pragma once
#include "prop_lease.h"
#include "../../native_free_runtime/observer_delta/native_free.h"
#include "remove_backend.h"
#include <set>

namespace mt2mp::prop_lease {
namespace remove=mt2mp::build_actions::private_native;
struct RuntimeProof {
    Proof proof;
    std::string exe_sha;
    bool actual_entryguards=false,actual_crt_receipt=false,actual_hook_receipt=false;
};
struct PrivateAllocationRecord {PrivateLease factory;Retirement retirement;native_free::Receipt free;bool attempted=false,consumed=false;};
struct PrivateRemoveRecord {PrivatePoolRecord pool;remove::RemoveContext context;World world;remove::PropLease token;uintptr_t retired_graph=0;};
namespace detail {
template<class Release>const std::vector<Receipt>& release_batch_once(const std::vector<std::string>& keys,bool& attempted,std::vector<Receipt>& receipts,Release release){
    if(attempted)return receipts;
    receipts.reserve(keys.size());attempted=true;
    for(const auto& key:keys){receipts.push_back(release(key));if(receipts.back().phase!=Phase::Released)break;}
    return receipts;
}
}
// Root supplies pure readers and current graph/callback receipts. These readers
// cannot pump callbacks or mutate object lifetimes between preflight and assign.
struct Services {
    virtual ~Services()=default;
    virtual bool validate_api(const Api&,std::string&)=0;
    virtual bool runtime(RuntimeProof&,std::string&)=0;
    virtual bool read_prop(void*,uint64_t expected_uid,Prop&,std::string&)=0;
    virtual bool reserve_own_holder_generation(uint64_t&,std::string&)=0;
    virtual bool validate_retirement(const PrivateLease&,const Retirement&,std::string&)=0;
    virtual bool next_free_invocation(uint64_t&,std::string&)=0;
    virtual bool remove_current(remove::RemoveContext&,std::string&)=0;
    virtual bool observe_begin(const remove::RemoveContext&,const std::vector<remove::PropLease>&,std::string&)=0;
    virtual bool observe_end(uintptr_t&,std::string&)=0;
    virtual bool observed_destruction(const remove::RemoveContext&,uintptr_t graph,const remove::PropLease&,std::string&)=0;
};
class NativeHost final:public Host {
public:
    NativeHost(Api api,Services& services,native_free::Observer& observer,uint32_t main_thread):api_(api),services_(services),observer_(observer),main_thread_(main_thread){}
    NativeHost(const NativeHost&)=delete;NativeHost& operator=(const NativeHost&)=delete;
    bool validate_exact_api(const Api&,std::string&)override;
    bool current(Proof&,std::string&)override;
    bool read_prop(void*,uint64_t,Prop&,std::string&)override;
    bool reserve_generation(uint64_t&,std::string&)override;
    bool allocation_returned(const PrivateLease&,std::string&)override;
    bool validate_retirement(const PrivateLease&,const Retirement&,std::string&)override;
    bool invoke_native_delete_once(DeleteFn,const PrivateLease&,const Retirement&,std::string&,uint32_t&)override;
    bool confirm_native_delete(const PrivateLease&,const Retirement&,std::string&)override;
    native_free::Receipt free_receipt(uint64_t generation)const;
    bool enumerate_private_allocations(std::vector<PrivateAllocationRecord>&,std::string&)const;
    Services& services()noexcept{return services_;}
private:
    struct Allocation {PrivateLease lease;Retirement retirement;native_free::Permit permit;native_free::Receipt free;bool attempted=false,consumed=false;};
    Api api_;Services& services_;native_free::Observer& observer_;uint32_t main_thread_=0;
    std::set<uint64_t> reserved_;
    std::map<uint64_t,Allocation> allocations_;
    bool same_allocation(const PrivateLease&,std::string&)const;
};
// This adapter owns only native weak holders. It never changes selection or highlights.
class RemoveLeaseServices final:public remove::RemoveServices {
public:
    RemoveLeaseServices(Api api,NativeHost& host,Diagnostics& diagnostics):host_(host),pool_(api,diagnostics){}
    RemoveLeaseServices(const RemoveLeaseServices&)=delete;RemoveLeaseServices& operator=(const RemoveLeaseServices&)=delete;
    bool current(remove::RemoveContext&,std::string&)override;
    bool retain(const remove::RemoveContext&,const std::vector<uintptr_t>&,std::vector<remove::PropLease>&,std::string&)override;
    bool inspect(const remove::RemoveContext&,const remove::PropLease&,uintptr_t,remove::Life&,std::string&)override;
    bool observe_begin(const remove::RemoveContext&,std::string&)override;
    bool observe_end(uintptr_t&,std::string&)override;
    // Root calls this only after the actual snapshot and all graph callbacks retire.
    std::vector<Receipt> retire_all(const Retirement&);
    Status status()const{return pool_.status();}
    bool enumerate_private(std::vector<PrivateRemoveRecord>&,std::vector<PrivateAllocationRecord>&,std::string&);
    bool read_owned_private(const std::string& key,PrivateOwnershipRead& out,std::string& error){return pool_.read_owned_private(key,out,host_,error);}
    bool ownership_scopes_idle()const noexcept{return !busy_&&!observing_&&!pool_.busy();}
    bool observation_scope_done()const noexcept{return !observing_;}
private:
    struct Binding {remove::RemoveContext context;World world;std::string key;remove::PropLease lease;uintptr_t retired_graph=0;};
    NativeHost& host_;Pool pool_;std::map<uint64_t,Binding> bindings_;
    std::vector<remove::PropLease> batch_;std::vector<std::string> owned_keys_;uint64_t batch_sequence_=0;bool busy_=false,observing_=false;
    bool retirement_attempted_=false;std::vector<Receipt> retirement_receipts_;
    bool context(const remove::RemoveContext&,World&,std::string&);
    static bool same_context(const remove::RemoveContext&,const remove::RemoveContext&);
};
}

#pragma once
#include "world_prop_reader.h"
#include "prop_host.h"
#include "remove_observer.h"

namespace mt2mp::build_actions::private_native::host {
// Root owns the hook installer, CRT resolution and snapshot/callback retirement.
// This boundary carries those receipts only; world and Prop proofs are read here.
struct RequiredHostServices {
    virtual ~RequiredHostServices()=default;
    virtual bool installation(bool& crt_receipt,uint32_t& remove_mask,bool& free_hook_receipt,std::string&)=0;
    virtual bool retirement(const mt2mp::prop_lease::PrivateLease&,const mt2mp::prop_lease::Retirement&,std::string&)=0;
    // Root uses one allocation journal for Building, Prop and every Services instance.
    virtual bool allocation_generation(uint64_t&,std::string&)=0;
    virtual bool free_invocation(uint64_t&,std::string&)=0;
};
class Services final:public mt2mp::prop_lease::Services {
public:
    Services(Memory&,Origin&,RequiredHostServices&,uintptr_t image,uint32_t thread,LookupApi,std::string exact_sha);
    bool bind_target(void*,uint64_t,std::string&);
    bool validate_api(const mt2mp::prop_lease::Api&,std::string&)override;
    bool runtime(mt2mp::prop_lease::RuntimeProof&,std::string&)override;
    bool read_prop(void*,uint64_t,mt2mp::prop_lease::Prop&,std::string&)override;
    bool reserve_own_holder_generation(uint64_t&,std::string&)override;
    bool validate_retirement(const mt2mp::prop_lease::PrivateLease&,const mt2mp::prop_lease::Retirement&,std::string&)override;
    bool next_free_invocation(uint64_t&,std::string&)override;
    bool remove_current(RemoveContext&,std::string&)override;
    bool observe_begin(const RemoveContext&,const std::vector<PropLease>&,std::string&)override;
    bool observe_end(uintptr_t&,std::string&)override;
    bool observed_destruction(const RemoveContext&,uintptr_t,const PropLease&,std::string&)override;
    // Passive hooks call these around their original; no method calls an original.
    void undo_open_returned(uintptr_t caller_rva,uintptr_t graph);
    void graph_lock_enter(uintptr_t caller_rva,uintptr_t graph);
    void graph_lock_returned(uintptr_t graph,bool returned);
    void prop_destructor_enter(uintptr_t prop);
    const GraphObservation& observation()const noexcept{return observer_.status();}
    const std::string& hook_error()const noexcept{return hook_error_;}
    uint64_t worker_hooks()const noexcept{return worker_hooks_.load();}
private:
    bool context(RemoveContext&,bool idle,std::string&);
    bool hook_context(RemoveContext&);
    Memory& memory_;RequiredHostServices& required_;uintptr_t image_;uint32_t thread_;std::string sha_;
    Reader reader_;opened::CurrentWorld bound_;uintptr_t target_=0;uint64_t target_uid_=0;
    std::set<uint64_t> allocation_generations_;std::string allocation_unknown_;
    std::vector<PropLease> retained_;RemoveObserver observer_;bool bound_target_=false,busy_=false;
    std::string hook_error_;
    std::atomic<uint64_t> worker_hooks_{0};
};
}

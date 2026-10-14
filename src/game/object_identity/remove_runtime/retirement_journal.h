#pragma once
#include "root_binding_retirement.h"
#include "copy/prop_host.h"
#include <optional>

namespace mt2mp::remove_runtime {
namespace native=build_actions::private_native;
struct QuiescenceMeasurement {
    uint32_t bound_thread=0,observed_thread=0;lease::World world;
    int32_t saves=-1,edits=-1;uintptr_t active_graph=0,active_operation=0;
    uint64_t dispatch_inflight=UINT64_MAX;
    bool backend_busy=true,adapter_busy=true,observer_active=true;
};
struct RetirementRecord {
    uint64_t id=0;PrivateRequest request;BindingRevocation root;
    QuiescenceMeasurement quiescence;
    native::RemoveResult commit;uintptr_t committed_graph=0;
    std::vector<lease::PrivateRemoveRecord> pools;
    std::vector<lease::PrivateAllocationRecord> factories;
    std::vector<lease::PrivateOwnershipRead> ownership;
};
struct RetirementEffects {
    bool attempted=false,confirmed=false,unknown=false;
    std::vector<lease::Receipt> releases;
    std::vector<native_free::Receipt> frees;
    std::string cause;
};
class RetirementJournal {
public:
    bool seal(const RetirementRecord&,std::string&);
    bool begin_run(std::string&);
    bool validate_allocation(const lease::PrivateLease&,const lease::Retirement&,std::string&)const;
    bool confirm_run(std::vector<lease::Receipt>,std::vector<native_free::Receipt>,std::string&);
    void unknown(const std::string&);
    const RetirementRecord* record()const noexcept{return sealed_?&*sealed_:nullptr;}
    const RetirementEffects& effects()const noexcept{return effects_;}
private:
    std::optional<RetirementRecord> sealed_;RetirementEffects effects_;
};
bool match_revocation(const PrivateRequest&,const BindingRevocation&,std::string&);
bool read_root_revocation_guarded(RootBindingRetirement&,const PrivateRequest&,BindingRevocation&,std::string&,uint32_t&);
bool verify_root_revocation_guarded(RootBindingRetirement&,const PrivateRequest&,const BindingRevocation&,std::string&,uint32_t&);
}

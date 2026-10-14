#pragma once
#include "selection_witness.h"
namespace mt2mp::build_actions::private_native {
struct RemoveContext {
    uint64_t epoch=0,thread=0;
    uintptr_t world=0,selection=0,target=0;
    bool exact_build=false,active_world=false,saves_idle=false,lease_current=false,main_thread=false;
};
struct PermitReceipt {
    bool active=false,unknown=false;
    uint32_t expected_clears=0,suppressed=0,foreign_forwarded=0,worker_forwarded=0,rejected=0;
    std::string error;
};
class RemovePermit {
public:
    // The lease provider supplies a fresh private context for each decision.
    bool begin(uintptr_t image,const RemoveContext&,Memory&,std::string&);
    bool should_forward_clear(const RemoveContext&,uintptr_t receiver,uintptr_t caller_rva,Memory&);
    PermitReceipt finish(bool destroy_attempted,bool destroy_returned,const std::string& original_error);
    const PermitReceipt& receipt()const noexcept{return receipt_;}
    const SelectionWitness& before()const noexcept{return before_;}
private:
    RemoveContext bound_{};uintptr_t image_=0;SelectionWitness before_{};PermitReceipt receipt_{};bool busy_=false;
    bool reject(const char*);
};
}

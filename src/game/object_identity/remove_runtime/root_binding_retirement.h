#pragma once
#include "operation_state.h"

namespace mt2mp::remove_runtime {
struct BindingRevocation {
    uint64_t registry_generation=0,entry_id=0,sequence=0,full_uid=0;
    lease::World world;
    std::string stable_snapshot_key,immutable_revocation_key;
    bool operator==(const BindingRevocation&)const=default;
};
// Root reads its actual stable-key registry. This API carries no holder pointer.
struct RootBindingRetirement {
    virtual ~RootBindingRetirement()=default;
    virtual bool read_actual_revocation(const PrivateRequest&,BindingRevocation&,std::string&)=0;
    virtual bool verify_actual_revocation(const PrivateRequest&,const BindingRevocation&,std::string&)=0;
};
class UnavailableRootBindingRetirement final:public RootBindingRetirement {
public:
    bool read_actual_revocation(const PrivateRequest&,BindingRevocation&,std::string&)override;
    bool verify_actual_revocation(const PrivateRequest&,const BindingRevocation&,std::string&)override;
};
}

#pragma once
#include "copy/prop_lease.h"
#include "copy/remove_backend.h"
#include <map>

namespace mt2mp::remove_runtime {
namespace lease=prop_lease;
struct PrivateRequest {
    uint64_t sequence=0;uintptr_t object=0;uint64_t full_uid=0;
    lease::World world;std::string snapshot_binding;
    bool operator==(const PrivateRequest&)const=default;
};
enum class OperationPhase {Prepared,Confirmed,Failed,Unknown};
struct OperationStatus {
    uint64_t sequence=0;OperationPhase phase=OperationPhase::Prepared;
    build_actions::private_native::RemoveResult removal;
    lease::Status pool;bool retirement_confirmed=false;
    std::string retirement_cause;
};
class OperationLedger {
public:
    enum class Admission {Fresh,Existing,Rejected};
    Admission admit(const PrivateRequest&,std::string&);
    void record(uint64_t,const build_actions::private_native::RemoveResult&,const lease::Status&);
    void lock_unknown(uint64_t,const std::string&);
    void retention_blocked(uint64_t,const lease::Status&,const std::string&);
    void retention_confirmed(uint64_t,const lease::Status&);
    const OperationStatus* find(uint64_t)const;
    bool unknown_locked()const noexcept{return unknown_;}
private:
    struct Entry {PrivateRequest binding;OperationStatus status;};
    std::map<uint64_t,Entry> entries_;uint64_t high_watermark_=0;bool unknown_=false;
};
// No actual snapshot/callback journal is available at this integration boundary.
bool deny_unproven_retirement(const lease::PrivateLease&,const lease::Retirement&,std::string&);
}

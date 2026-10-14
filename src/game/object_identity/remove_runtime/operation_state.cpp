#include "operation_state.h"
#include <stdexcept>

namespace mt2mp::remove_runtime {
OperationLedger::Admission OperationLedger::admit(const PrivateRequest& r,std::string& error){
    error.clear();
    if(!r.sequence||!r.object||!r.full_uid||!r.world.epoch||!r.world.origin_generation||!r.world.world||!r.world.map||r.snapshot_binding.empty()){error="remove_runtime_private_binding_incomplete";return Admission::Rejected;}
    const auto it=entries_.find(r.sequence);
    if(it!=entries_.end()){
        if(it->second.binding==r)return Admission::Existing;
        error="remove_runtime_conflicting_duplicate";return Admission::Rejected;
    }
    if(unknown_){error="remove_runtime_unknown_locked";return Admission::Rejected;}
    if(r.sequence<=high_watermark_){error="remove_runtime_old_sequence";return Admission::Rejected;}
    OperationStatus status;status.sequence=r.sequence;
    entries_.emplace(r.sequence,Entry{r,std::move(status)});high_watermark_=r.sequence;return Admission::Fresh;
}
void OperationLedger::record(uint64_t sequence,const build_actions::private_native::RemoveResult& r,const lease::Status& pool){
    auto& s=entries_.at(sequence).status;s.pool=pool;
    if(s.phase==OperationPhase::Unknown)return;
    s.removal=r;
    if(r.unknown_locked||pool.unknown_lock||(r.attempted&&!r.confirmed)){s.phase=OperationPhase::Unknown;unknown_=true;}
    else s.phase=r.confirmed?OperationPhase::Confirmed:OperationPhase::Failed;
}
void OperationLedger::lock_unknown(uint64_t sequence,const std::string& error){
    unknown_=true;auto& s=entries_.at(sequence).status;s.phase=OperationPhase::Unknown;s.removal.unknown_locked=true;s.removal.confirmed=false;
    if(s.removal.error.empty())s.removal.error=error.empty()?"remove_runtime_unknown_without_cause":error;
}
void OperationLedger::retention_blocked(uint64_t sequence,const lease::Status& pool,const std::string& error){
    auto& s=entries_.at(sequence).status;s.pool=pool;s.retirement_confirmed=false;
    s.retirement_cause=error.empty()?"remove_runtime_retirement_failed_without_cause":error;
}
const OperationStatus* OperationLedger::find(uint64_t sequence)const{const auto it=entries_.find(sequence);return it==entries_.end()?nullptr:&it->second.status;}
void OperationLedger::retention_confirmed(uint64_t sequence,const lease::Status& pool){auto& s=entries_.at(sequence).status;s.pool=pool;s.retirement_confirmed=true;s.retirement_cause="info: actual own snapshot/quiescence retirement and every current FreeACK confirmed";}
bool deny_unproven_retirement(const lease::PrivateLease&,const lease::Retirement&,std::string& error){error="remove_runtime_actual_snapshot_callback_retirement_journal_unavailable; own holders retained";return false;}
}

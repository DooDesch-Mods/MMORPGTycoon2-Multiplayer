#include "remove_permit.h"
namespace mt2mp::build_actions::private_native {
namespace {
const char* invalid(const RemoveContext& c){
    if(!c.exact_build)return "remove_build_guard";
    if(!c.main_thread||!c.thread)return "remove_wrong_thread";
    if(!c.epoch||!c.world||!c.active_world)return "remove_world_guard";
    if(!c.selection)return "remove_selection_missing";
    if(!c.target||!c.lease_current)return "remove_lease_guard";
    if(!c.saves_idle)return "remove_native_save_pending";
    return nullptr;
}
struct Busy {bool& flag;explicit Busy(bool& v):flag(v){flag=true;}~Busy(){flag=false;}};
}
bool RemovePermit::begin(uintptr_t image,const RemoveContext& context,Memory& memory,std::string& error){
    if(busy_||receipt_.active){error="remove_permit_reentrant";return false;}
    if(receipt_.unknown){error="remove_permit_unknown_locked";return false;}
    Busy busy(busy_);
    if(!image){error="remove_image_missing";return false;}
    if(const auto* reason=invalid(context)){error=reason;return false;}
    SelectionWitness selection;SelectionCounts counts;
    if(!read_selection(memory,image,context.selection,selection,counts,error))return false;
    if(selection.selection_in_progress||selection.queue_mode){error="remove_wait_selection_transaction";return false;}
    bound_=context;image_=image;before_=std::move(selection);receipt_={};receipt_.active=true;return true;
}
bool RemovePermit::reject(const char* error){++receipt_.rejected;receipt_.unknown=true;if(receipt_.error.empty())receipt_.error=error;return true;}
bool RemovePermit::should_forward_clear(const RemoveContext& context,uintptr_t receiver,uintptr_t caller,Memory& memory){
    if(!receipt_.active)return true;
    if(context.thread!=bound_.thread||!context.main_thread){++receipt_.worker_forwarded;return true;}
    if(receiver!=bound_.selection||(caller!=0x2d4351&&caller!=0x2d4b62)){++receipt_.foreign_forwarded;return true;}
    ++receipt_.expected_clears;
    if(busy_)return reject("remove_clear_reentrant");
    Busy busy(busy_);
    if(receipt_.unknown)return reject("remove_clear_already_unknown");
    if(!context.exact_build||!context.active_world||!context.saves_idle||!context.lease_current||context.epoch!=bound_.epoch||
       context.world!=bound_.world||context.selection!=bound_.selection||context.target!=bound_.target)return reject("remove_clear_lifetime_changed");
    if(receipt_.suppressed)return reject("remove_clear_duplicate_origin");
    SelectionWitness now;SelectionCounts counts;std::string error;
    if(!read_selection(memory,image_,receiver,now,counts,error)||!validate_selective_removal(before_,now,bound_.target,error)){
        receipt_.error=error;return reject("remove_clear_read_failed");
    }
    if(receipt_.unknown)return true;
    ++receipt_.suppressed;return false;
}
PermitReceipt RemovePermit::finish(bool attempted,bool returned,const std::string& error){
    if(busy_){reject("remove_finish_reentrant");return receipt_;}
    receipt_.active=false;
    if(attempted&&(!returned||receipt_.suppressed!=1)){receipt_.unknown=true;if(receipt_.error.empty())receipt_.error=error.empty()?"remove_clear_observation_incomplete":error;}
    if(!error.empty()){receipt_.unknown=receipt_.unknown||attempted;if(receipt_.error.empty())receipt_.error=error;}
    return receipt_;
}
}

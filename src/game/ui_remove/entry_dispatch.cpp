#include "native_entry_windows.h"
#include <exception>

namespace mt2mp::action_session::ui_capture {
void dispatch_entry(Publication& owner,void* manager,NativePropArray* array,uintptr_t return_slot,uint32_t thread) {
    Origin origin=Origin::Foreign;
    if(thread==owner.game_thread) {
        std::string cause;bool observed=false;
        try{observed=classify_origin(*owner.memory,owner.image,return_slot,origin,cause);}
        catch(const std::exception& failure){cause=failure.what();}
        catch(...){cause="native caller frame observation nonstandard exception";}
        if(!observed) {
            owner.capture->record_entry_failure("UI Remove exact native caller frame observation: "+cause);
            owner.last_disposition.store(uint8_t(Disposition::Rejected),std::memory_order_release);
            owner.last_original_attempted.store(false,std::memory_order_release);
            owner.last_original_returned.store(false,std::memory_order_release);
            return;
        }
    }
    const auto report=owner.capture->destroy_props(manager,array,origin);
    owner.last_origin.store(uint8_t(origin),std::memory_order_release);
    owner.last_disposition.store(uint8_t(report.disposition),std::memory_order_release);
    owner.last_original_attempted.store(report.original_attempted,std::memory_order_release);
    owner.last_original_returned.store(report.original_returned,std::memory_order_release);
}
}

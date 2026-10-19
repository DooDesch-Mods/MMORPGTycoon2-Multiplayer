#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::ability_receiver_cleanup {
struct State {
    uintptr_t icon=0,source=0;
    bool record_constructed=false,slot_constructed=false,icon_constructed=false,source_constructed=false;
    bool cleanup_attempted=false,cleanup_confirmed=false;
};
struct Receipt {
    bool slot_returned=false,icon_deleted_by_slot=false,icon_returned=false,source_returned=false,record_returned=false;
    uint32_t calls=0;std::string cause;
};
// The original Slot destructor deletes its docked Icon. A separate Icon delete
// is valid only when that original ownership did not take effect.
template<class Port>bool finish(Port& port,State& state,Receipt& receipt,std::string& error){
    if(state.cleanup_attempted){error="Ability private cleanup already attempted; retry is forbidden";return false;}
    state.cleanup_attempted=true;error.clear();
    const auto failed=[&](const std::string& fallback){if(error.empty())error=fallback;receipt.cause=error;return false;};
    if(state.slot_constructed){uintptr_t docked=0;
        if(!port.slot_owner(docked,error))return failed("Ability private Slot owner read failed");
        if(docked&&docked!=state.icon)return failed("Ability private Slot contains a foreign Icon; destruction is forbidden");
        ++receipt.calls;if(!port.destroy_slot(error))return failed("Ability original private Slot destruction failed");
        state.slot_constructed=false;receipt.slot_returned=true;
        if(docked){state.icon=0;state.icon_constructed=false;receipt.icon_deleted_by_slot=true;}}
    if(state.icon){++receipt.calls;if(!port.destroy_icon(state.icon,state.icon_constructed,error))return failed("Ability original private Icon destruction failed");
        state.icon=0;state.icon_constructed=false;receipt.icon_returned=true;}
    if(state.source){++receipt.calls;if(!port.destroy_source(state.source,state.source_constructed,error))return failed("Ability original private prototype destruction failed");
        state.source=0;state.source_constructed=false;receipt.source_returned=true;}
    if(state.record_constructed){++receipt.calls;if(!port.destroy_record(error))return failed("Ability original private command Record destruction failed");
        state.record_constructed=false;receipt.record_returned=true;}
    state.cleanup_confirmed=true;error.clear();return true;
}
}

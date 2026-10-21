#pragma once
#include <cstdint>
#include <string>
#include <exception>

namespace mt2mp::game::district_widget_scope {
// Only the UI value binding is disconnected. The original still owns its local
// cursor, rounding, displayed value and pane updates. The world field is never
// substituted or written by this presentation call.
template<class Port,class Call>
bool run(Port& port,uintptr_t slot,uintptr_t expected,Call&& call,std::string& error){
    if(!slot||!expected){error="District local presentation binding is empty";return false;}
    if(!port.exchange(slot,expected,0,error))return false;
    std::string original_error;bool returned=false;
    try{returned=call(original_error);}
    catch(const std::exception& exception){original_error=std::string("District local presentation exception: ")+exception.what();}
    catch(...){original_error="District local presentation unknown exception";}
    if(!returned&&original_error.empty())original_error="District local presentation original did not return";
    std::string restore_error;const bool restored=port.exchange(slot,0,expected,restore_error);
    if(!returned||!restored){
        error=!returned?original_error:restore_error;
        if(!returned&&!restored)error+="; binding restoration: "+restore_error;
        return false;
    }
    return true;
}
}

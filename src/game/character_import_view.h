#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::character_import_view {
struct State {
    uintptr_t view=0,base=0,selected=0,prefabs=0,notice=0;
    bool operator==(const State&)const=default;
};
struct Receipt {bool prepared=false,attempted=false,returned=false,restored=false,confirmed=false;};
template<class Calls>bool run(Calls& calls,const State& saved,uintptr_t target,bool preserve,Receipt& receipt,std::string& error){
    receipt={};State actual;
    if(!target){error="Character import selected target is null";return false;}
    if(!calls.read(actual,error))return false;
    if(actual!=saved){error="Character import current local presentation changed before preparation";return false;}
    auto prepared=saved;
    if(preserve){prepared.selected=target;prepared.notice=0;
        receipt.prepared=true;if(!calls.write(prepared,error))return false;
        if(!calls.read(actual,error)||actual!=prepared){if(error.empty())error="Character import scoped local preparation readback differs";return false;}
    }else if(saved.selected!=target){error="Character import initiating selected definition changed";return false;}
    const bool returned=calls.invoke(preserve,error);receipt.attempted=calls.attempted();receipt.returned=calls.returned();
    const auto original_error=error;
    if(preserve){std::string restore_error;
        if(!calls.read(actual,restore_error)||actual!=prepared){
            error=original_error;if(!error.empty())error+="; ";error+=restore_error.empty()?"Character import local presentation owner changed before restore":restore_error;return false;}
        if(!calls.write(saved,restore_error)||!calls.read(actual,restore_error)||actual!=saved){
            error=original_error;if(!error.empty())error+="; ";error+=restore_error.empty()?"Character import local presentation restoration readback differs":restore_error;return false;}
        receipt.restored=true;
    }
    if(!returned||!receipt.returned){error=original_error.empty()?"Character import original transaction did not return":original_error;return false;}
    receipt.confirmed=true;return true;
}
}

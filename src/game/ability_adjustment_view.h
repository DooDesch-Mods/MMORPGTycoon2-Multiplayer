#pragma once
#include <cstdint>
#include <bit>
#include <string>
namespace mt2mp::game::ability_adjustment_view {
struct State {
    uintptr_t view=0,selected=0,prototype=0;uint8_t cost=0;int32_t index=-1;float delta=0;
    bool operator==(const State& other)const{return view==other.view&&selected==other.selected&&prototype==other.prototype&&cost==other.cost&&index==other.index&&std::bit_cast<uint32_t>(delta)==std::bit_cast<uint32_t>(other.delta);}
};
template<class Calls>bool run(Calls& calls,const State& prepared,bool preserve,bool& attempted,std::string& error){
    attempted=false;State previous,actual;
    if(!calls.read(previous,error))return false;
    if(previous.view!=prepared.view){error="Ability adjustment current View receiver changed";return false;}
    const auto restore=[&](){std::string cause;
        if(!calls.restore(previous,cause)){if(!error.empty())error+="; ";error+="Ability adjustment proposal UI restoration: "+cause;}};
    if(!calls.prepare(prepared,error)){restore();return false;}
    if(!calls.read(actual,error)){restore();return false;}
    if(actual!=prepared){error="Ability adjustment original proposal UI preparation differs";restore();return false;}
    attempted=true;
    const bool returned=calls.invoke(preserve,error);
    std::string restore_error;
    const bool restored=!preserve||calls.restore(previous,restore_error);
    if(!restored){if(!error.empty())error+="; ";error+="Ability adjustment proposal UI restoration: "+restore_error;}
    if(!returned||!restored)return false;
    if(preserve){if(!calls.read(actual,error))return false;
        if(actual!=previous){error="Ability adjustment unrelated local proposal UI changed";return false;}}
    return true;
}
}

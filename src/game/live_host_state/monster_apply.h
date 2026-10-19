#pragma once
#include "monster_state.h"
#include <exception>
namespace mt2mp::game::live_host_state::monsters {
// A final pose pass follows all native scopes that can flush actor transforms.
template<class Calls>Receipt apply_motion(Calls& calls,const State& target){
    Receipt receipt;State before;
    const auto failed=[&](const std::string& why){receipt.cause=why.empty()?"Monster motion failed without cause":why;receipt.unknown=receipt.attempted;return receipt;};
    try{
        if(!calls.read(before,receipt.cause)||!preflight(before,target,receipt.cause))return failed(receipt.cause);
        for(size_t i=0;i<target.rows.size();++i){const auto& wanted=target.rows[i];const auto& old=before.rows[i];
            if(same_applied(old,wanted))continue;
            State current;if(!calls.read(current,receipt.cause)||!preflight(current,target,receipt.cause))return failed(receipt.cause);
            for(size_t j=0;j<current.rows.size();++j){const auto& expected=j<i?target.rows[j]:before.rows[j];
                if(!same_applied(current.rows[j],expected))return failed("Monster current applied state changed across final native callback birth="+current.rows[j].birth);}
            bool attempted=false;++receipt.attempted_rows;
            struct Attempt {Receipt& receipt;bool& value;~Attempt(){receipt.attempted=receipt.attempted||value;}} attempt{receipt,attempted};
            const bool changed=calls.set(old,wanted,attempted,receipt.cause);receipt.attempted=receipt.attempted||attempted;
            if(!changed)return failed(receipt.cause);++receipt.completed_rows;
        }
        State after;if(!calls.read(after,receipt.cause))return failed(receipt.cause);
        calls.observed(before,after);
        if(!adopted(after,target,receipt.cause))return failed(receipt.cause);
        receipt.confirmed=true;return receipt;
    }catch(const std::exception& x){return failed(std::string("Monster motion boundary exception: ")+x.what());}
    catch(...){return failed("Monster motion nonstandard boundary exception");}
}
template<class Calls>bool clear_spatial_dirty(Calls& calls,uintptr_t object,bool& attempted,std::string& e){
    uint8_t before=2;if(!calls.read_byte(object+0x480,before,e))return false;
    if(before>1){e="Monster spatial dirty byte outside boolean domain";return false;}
    if(!before)return true;
    attempted=true;if(!calls.update_spatial(object,e))return false;
    if(!calls.compare_byte(object+0x480,1,0,e))return false;
    uint8_t after=2;if(!calls.read_byte(object+0x480,after,e))return false;
    if(after){e="Monster original spatial update retained dirty byte";return false;}return true;
}
}

#pragma once
#include "staff_action.h"
#include "../typed_action/action_session.h"

namespace mt2mp::game::tutorial_replication::staff {
template<class Calls>
bool apply(const startingpoint::Row& expected,int32_t desired,Calls& calls,bool& attempted,std::string& error){
    attempted=false;auto target=expected;target.desired_workers=desired;
    if(!rows(expected,target,desired,error))return false;
    if(!calls.admit(desired,error))return false;
    startingpoint::Row current;action_session::Ledger before,after;
    if(!calls.read(current,before,error))return false;
    if(current!=expected){error="Startingpoint staff exact current Host BEFORE row differs";return false;}
    attempted=true;
    if(!calls.allocate(desired,error))return false;
    if(!calls.read(current,after,error))return false;
    if(current!=target||after!=before){error="Startingpoint staff original setter AFTER row/full ledger differs";return false;}
    return true;
}
}

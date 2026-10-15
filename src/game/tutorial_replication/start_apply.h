#pragma once
#include "start_ledger.h"
#include <exception>

namespace mt2mp::game::tutorial_replication::start_intent {
struct Application {
    bool consumed=false,attempted=false,payment_confirmed=false;
    bool creation_attempted=false,confirmed=false,unknown=false;
};

// Typed actions enter with the exact Host BEFORE Ledger. The existing economic
// action scope authorizes the original category-3 cash leaf once. The ordinary
// frame adopter instead receives AFTER and keeps its existing free-purchase path.
template<class Calls>
bool apply(const action_session::Ledger& before,const action_session::Ledger& after,
           const startingpoint::Row& before_row,const startingpoint::Row& after_row,
           Calls& calls,Application& receipt,std::string& error){
    if(receipt.consumed){error="Startingpoint Start application already consumed; no replay";return false;}
    if(!rows(before_row,after_row,error)||!payment(before,after,error))return false;
    try{
        action_session::Ledger current;startingpoint::Row row;
        if(!calls.read(current,row,error))return false;
        if(current!=before||row!=before_row){error="Startingpoint Start exact Host BEFORE ledger/row differs";return false;}
        if(!calls.preflight(error))return false;
        receipt.consumed=receipt.attempted=receipt.unknown=true;
        if(!calls.pay(error))return false;
        if(!calls.read(current,row,error))return false;
        auto paid=before_row;paid.purchased=true;
        if(current!=after||row!=paid){error="Startingpoint Start original Pay full AFTER ledger/purchased row differs";return false;}
        receipt.payment_confirmed=true;
        receipt.creation_attempted=true;
        if(!calls.create(paid,after_row,error))return false;
        if(!calls.read(current,row,error))return false;
        if(current!=after||row!=after_row){error="Startingpoint Start original creation full AFTER ledger/row differs";return false;}
        receipt.confirmed=true;receipt.unknown=false;error.clear();return true;
    }catch(const std::exception& cause){error=std::string("Startingpoint Start application failed: ")+cause.what();}
    catch(...){error="Startingpoint Start application failed with nonstandard exception";}
    return false;
}
}

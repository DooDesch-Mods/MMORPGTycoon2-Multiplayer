#pragma once
#include "../typed_action/action_session.h"
#include "startingpoint.h"
#include <climits>

namespace mt2mp::game::tutorial_replication::start_intent {
inline bool rows(const startingpoint::Row& before,const startingpoint::Row& after,std::string& error){
    startingpoint::Plan operation;
    if(!startingpoint::plan(before,after,operation,error))return false;
    if(operation!=startingpoint::Plan::Create||before.purchased){error="Startingpoint start requires original first purchase and project creation";return false;}
    return true;
}
inline bool payment(const action_session::Ledger& before,const action_session::Ledger& after,std::string& error){
    const int64_t price=int64_t(before.cash)-after.cash;
    if(before.record_present!=1||before.record_day!=before.flat_day||before.record_cash!=before.cash||
       price<0||price>INT32_MAX||before.cash<price){error="Startingpoint start purchase current ledger/price differs";return false;}
    if(before.line_counts[3]==INT32_MAX||int64_t(before.record_expense)+price>INT32_MAX||int64_t(before.line_income[3])-price<INT32_MIN){error="Startingpoint start purchase ledger overflow";return false;}
    auto expected=before;expected.cash-=int32_t(price);expected.record_cash-=int32_t(price);expected.record_expense+=int32_t(price);
    ++expected.line_counts[3];expected.line_income[3]-=int32_t(price);
    if(expected!=after){error="Startingpoint start original purchase full ledger/category3 differs";return false;}
    error.clear();return true;
}
inline bool expected_payment(const action_session::Ledger& before,int32_t price,action_session::Ledger& after,std::string& error){
    if(price<0||before.cash<price||before.line_counts[3]==INT32_MAX||
       int64_t(before.record_expense)+price>INT32_MAX||int64_t(before.line_income[3])-price<INT32_MIN){
        error="Startingpoint Start original price/affordability/ledger overflow rejected";return false;
    }
    const auto prior=before;
    after=prior;after.cash-=price;after.record_cash-=price;after.record_expense+=price;
    ++after.line_counts[3];after.line_income[3]-=price;
    return payment(prior,after,error);
}
}

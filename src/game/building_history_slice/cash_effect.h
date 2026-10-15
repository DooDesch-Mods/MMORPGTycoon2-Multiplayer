#pragma once
#include "../live_host_state/economic_fence.h"
namespace mt2mp::game::building_history_slice {
inline bool cash_effect(const action_session::Ledger& before,int32_t amount,action_session::Ledger& after,std::string& error) {
    if(!amount){after=before;error.clear();return true;}
    return live_host_state::predicted_cash(before,false,amount,6,after,error);
}
}

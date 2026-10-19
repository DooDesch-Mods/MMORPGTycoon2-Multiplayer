#pragma once
#include "network_remove_action.h"
namespace mt2mp::action_session {
struct NetworkHistoryProof {
    uint8_t source=0;int32_t amount=0;bool junction=false;
    bool operator==(const NetworkHistoryProof&)const=default;
};
struct Request;struct Evidence;
bool network_history(const Request&);
bool network_history_effect(const Request&,const Evidence&,std::string&);
}

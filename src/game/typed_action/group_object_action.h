#pragma once
#include "group_action.h"
#include "duplicate_action.h"

namespace mt2mp::action_session {
struct GroupObjectIntent {
    std::string key;
    std::array<uint32_t,10> before{},after{};
    std::vector<std::string> members;
    bool operator==(const GroupObjectIntent&)const=default;
};
struct GroupObjectState {
    GroupState ownership;
    DuplicateState records;
    bool operator==(const GroupObjectState&)const=default;
};
}

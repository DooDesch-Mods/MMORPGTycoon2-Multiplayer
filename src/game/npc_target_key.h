#pragma once
#include "subscriber_quest_state.h"
#include "snapshot_npc_key.h"
#include "building_npc_key.h"

namespace mt2mp::game::npc_target_key {
inline bool valid(const std::string& key) {
    return subscriber_quest_state::valid_key(key) || snapshot_npc_key::valid(key) || building_npc_key::valid(key);
}
inline bool input(const std::string& target) {
    return building_npc_key::valid(target) || snapshot_npc_key::valid(target) || subscriber_quest_state::valid_key("action:" + target);
}
inline std::string canonical(const std::string& target) {
    return (building_npc_key::valid(target) || snapshot_npc_key::valid(target)) ? target : "action:" + target;
}
inline std::string argument(const std::string& key) {
    return key.starts_with("action:") ? key.substr(7) : key;
}
inline bool request(const std::string& key, const std::string& action_uuid) {
    return action_uuid.empty() ? (snapshot_npc_key::valid(key) || building_npc_key::valid(key)) :
        subscriber_quest_state::valid_key("action:" + action_uuid) && key == "action:" + action_uuid;
}
}

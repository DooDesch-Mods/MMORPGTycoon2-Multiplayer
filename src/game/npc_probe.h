#pragma once

#include "vendor/nlohmann/json.hpp"
#include "typed_action/action_session.h"
#include "npc_move_visibility.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
namespace mt2mp::action_session::ui_capture {struct LocalBinding;struct Replay;}
namespace mt2mp::move_kiss {struct Intent;}

namespace mt2mp::game::npc_probe {
bool query(int32_t region_id, nlohmann::json& result, std::string& error);
bool tool_ui(const std::string& token, int32_t region_id, const std::string& tool,
             const std::string& phase, int32_t character_slot, int32_t character_type,
             nlohmann::json& result, std::string& error);
bool monster_zone_ui(const nlohmann::json&, nlohmann::json&, std::string&);
bool monster_zone_probe(const nlohmann::json&, nlohmann::json&, std::string&);
bool place_probe(const nlohmann::json& args, nlohmann::json& result, std::string& error);
namespace action_runtime {
namespace history {
bool reset(std::string&);
bool remember(const action_session::Binding&,const action_session::Request&,std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
}
struct CharacterNpcRetirement {uintptr_t pointer=0,region=0;uint64_t uid=0;};
bool confirm_character_membership_retirements(const action_session::Binding&,const action_session::Request&,
    const std::vector<CharacterNpcRetirement>&,std::string&);
struct StateOwner {
    std::string key;
    uintptr_t npc=0;
    std::vector<std::pair<int32_t,uintptr_t>> quests;
};
bool state_owners(const action_session::Context&,const action_session::Binding&,
    std::vector<StateOwner>&,std::string&);
bool resolve_state_link(const std::vector<StateOwner>&,const std::string& key,
    int32_t sequence_id,uintptr_t& npc,uintptr_t& quest,std::string&);
bool local_key(uintptr_t object,uint64_t uid,std::string& key,std::string& error);
bool bug_parent(const action_session::Context&,const action_session::Binding&,const std::string& key,uintptr_t&,std::string&);
bool enable(std::string& error);
void tick();
nlohmann::json status();
bool pending_ui();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,
    const action_session::Evidence* authority,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool native_ui(const nlohmann::json&,nlohmann::json&,std::string&);
bool native_remove_ui(const nlohmann::json&,nlohmann::json&,std::string&);
bool native_move_ui(const nlohmann::json&,nlohmann::json&,std::string&);
bool move_capture_binding(const std::string& snapshot,uint64_t epoch,uint64_t generation,
    uintptr_t object,uint64_t uid,action_session::BuildingState& before,
    std::string& key,std::string& error);
bool submit_restored_move(const move_kiss::Intent&,const std::string& action_uuid,std::string& error);
bool move_captured(const move_kiss::Intent&,const std::string& action_uuid,std::string& error);
void move_submitted(const std::string& action_uuid);
void move_failed(const std::string& cause);
bool remove_local_binding(uintptr_t manager,uintptr_t object,action_session::ui_capture::LocalBinding&,std::string&);
bool authorize_remove_replay(const action_session::ui_capture::Replay&,std::string&);
void remove_submitted(const std::string& action_uuid,const std::string& target_action_uuid);
}
}

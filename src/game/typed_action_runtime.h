#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
#include <cstdint>
namespace mt2mp::action_session::ui_capture {struct Replay;}
namespace mt2mp::action_session {struct BuildingState;struct PriceIntent;struct NpcIntent;struct Binding;struct Context;enum class Action:uint8_t;}

namespace mt2mp::game::typed_action_runtime {
bool start(std::string&);
void tick();
bool enable_live_state(std::string&);
bool live_state_active();
bool live_state_available(std::string&);
bool request_remove(const nlohmann::json&,nlohmann::json&,std::string&);
bool submit_place(const std::string& action_uuid,const action_session::BuildingState&,std::string&);
bool submit_move(const std::string& action_uuid,const std::string& object_key,const action_session::BuildingState&,std::string&);
bool submit_building_history(action_session::Action action,const std::string& action_uuid,const std::string& target_action_uuid,std::string&);
bool submit_commit_startingpoint(const std::string& action_uuid,std::string&);
bool submit_prices(const std::string& action_uuid,const action_session::PriceIntent&,std::string&);
bool submit_npc(const std::string& action_uuid,const action_session::NpcIntent&,std::string&);
bool submit_remove_npc(const std::string& action_uuid,const std::string& target_action_uuid,std::string&);
bool submit_move_npc(const std::string& action_uuid,const std::string& target_action_uuid,
    const action_session::BuildingState& transform,std::string&);
bool observe_startingpoint_submission(const action_session::Context& expected_context,
    const action_session::Binding& expected_binding,const std::string& action_uuid,std::string&);
bool network_binding(const std::string& snapshot,action_session::Binding&,std::string&);
bool request_place(const nlohmann::json&,nlohmann::json&,std::string&);
bool request_move(const nlohmann::json&,nlohmann::json&,std::string&);
nlohmann::json controller_status();
nlohmann::json status();
bool authorize_ui_replay(const action_session::ui_capture::Replay&,std::string&);
}

namespace mt2mp::game::typed_action_runtime { bool begin_lobby_leave(std::string&); bool finish_lobby_leave(bool& ready,std::string&); }

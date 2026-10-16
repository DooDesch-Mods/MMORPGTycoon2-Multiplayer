#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
#include <vector>
#include <cstdint>
namespace mt2mp::action_session::ui_capture {struct Replay;}
namespace mt2mp::action_session {struct BuildingState;struct PriceIntent;struct ZoneIntent;struct NpcIntent;struct Binding;struct Context;enum class Action:uint8_t;}

namespace mt2mp::game::developer_staff::construction {struct LocalJob;}
namespace mt2mp::game::typed_action_runtime {
bool start(std::string&);
bool read_subscriber_weapon(int32_t serialized_id,nlohmann::json& out,std::string& error);
void tick();
bool enable_live_state(std::string&);
bool live_state_active();
bool live_state_available(std::string&);
bool request_remove(const nlohmann::json&,nlohmann::json&,std::string&);
bool submit_place(const std::string& action_uuid,const action_session::BuildingState&,std::string&);
bool submit_move(const std::string& action_uuid,const std::string& object_key,const action_session::BuildingState&,std::string&);
bool submit_building_history(action_session::Action action,const std::string& action_uuid,const std::string& target_action_uuid,std::string&);
bool submit_building_history(action_session::Action action,const std::string& action_uuid,const std::string& target_action_uuid,const std::string& object_key,std::string&);
bool submit_commit_startingpoint(const std::string& action_uuid,std::string&);
bool submit_developer(bool hire,const std::string& id,std::string& error);
bool submit_startingpoint_workers(const std::string& id,int32_t desired,std::string& error);
bool submit_start_startingpoint(const std::string& action_uuid,std::string&);
bool submit_prices(const std::string& action_uuid,const action_session::PriceIntent&,std::string&);
bool submit_zone(const std::string&,const action_session::ZoneIntent&,std::string&);
bool submit_npc(const std::string& action_uuid,const action_session::NpcIntent&,std::string&);
bool submit_remove_npc(const std::string& action_uuid,const std::string& target_action_uuid,std::string&);
bool submit_move_npc(const std::string& action_uuid,const std::string& target_action_uuid,
    const action_session::BuildingState& transform,std::string&);
bool observe_startingpoint_submission(const action_session::Context& expected_context,
    const action_session::Binding& expected_binding,const std::string& action_uuid,std::string&);
bool bug_parent_alive(const action_session::Context&,const action_session::Binding&,const std::string& key,
    bool& known,bool& alive,std::string&);
bool bug_context(action_session::Context&,action_session::Binding&,uint64_t& publication,bool& active,std::string&);
bool network_binding(const std::string& snapshot,action_session::Binding&,std::string&);
bool construction_jobs(const action_session::Context&,const action_session::Binding&,
    std::vector<developer_staff::construction::LocalJob>&,std::string&);
bool request_place(const nlohmann::json&,nlohmann::json&,std::string&);
bool request_move(const nlohmann::json&,nlohmann::json&,std::string&);
nlohmann::json controller_status();
nlohmann::json status();
bool authorize_ui_replay(const action_session::ui_capture::Replay&,std::string&);
}

namespace mt2mp::game::typed_action_runtime { bool begin_lobby_leave(std::string&); bool finish_lobby_leave(bool& ready,std::string&); }

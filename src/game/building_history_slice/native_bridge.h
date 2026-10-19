#pragma once
#include "typed_action/action_session.h"
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>

namespace mt2mp::game::building_history_slice {
bool enable(uintptr_t image,uint32_t main_thread,std::string& error);
bool remember_place(const std::string& original_action_uuid,uintptr_t graph,std::string& error);
bool remember_remove(const std::string& original_action_uuid,const std::string& object_key,uintptr_t graph,std::string& error);
action_session::Outcome perform(action_session::Action direction,const std::string& original_action_uuid,
                                std::string& error);
bool invoke_ui(const nlohmann::json& args,nlohmann::json& out,std::string& error);
bool read_ui(nlohmann::json& out,std::string& error);
}

namespace mt2mp::game::building_history_slice {
bool remember_terrain(const std::string&,uintptr_t,std::string&);
bool remember_network(const std::string& original_action_uuid,uintptr_t graph,std::string& error);
bool remember_path(const std::string& original_action_uuid,uintptr_t graph,std::string& error);
bool remember_region(const std::string& original_action_uuid,int32_t region_id,uintptr_t graph,std::string& error);
bool remember_dungeon_tiles(const std::string&,uintptr_t,std::string&);
bool remember_quest(const std::string& original_action_uuid,uintptr_t graph,std::string& error);
bool remember_duplicate(const std::string&,uintptr_t,std::string&);
bool remember_scenery(const std::string& original_action_uuid,uintptr_t graph,std::string& error);
bool remember_group(const std::string& original_action_uuid,uintptr_t graph,std::string& error);
bool remember_group_object(const std::string& original_action_uuid,uintptr_t graph,std::string& error);
bool remember_selection(const std::string& original_action_uuid,uintptr_t graph,std::string& error);
bool graph_for(action_session::Action direction,const std::string& original_action_uuid,uintptr_t& graph,std::string& error);
bool remember_move(const std::string& original_action_uuid,const std::string& object_key,uintptr_t graph,std::string& error);
}

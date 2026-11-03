#pragma once
#include <string>
#include <vendor/nlohmann/json.hpp>
namespace mt2mp::game::npc_probe::monster_action_runtime::character_rename {
std::string native_edit_input_token();
bool native_edit_submitted(const std::string& token,const std::string& action_uuid,std::string& error);
bool native_edit_ui(const nlohmann::json&,nlohmann::json&,std::string&);
bool native_edit_read(const nlohmann::json&,nlohmann::json&,std::string&);
}

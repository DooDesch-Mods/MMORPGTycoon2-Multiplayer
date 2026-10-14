#pragma once
#include "typed_action/action_session.h"
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>

namespace mt2mp::game::building_history_slice {
bool enable(uintptr_t image,uint32_t main_thread,std::string& error);
bool remember_place(const std::string& original_action_uuid,uintptr_t graph,std::string& error);
action_session::Outcome perform(action_session::Action direction,const std::string& original_action_uuid,
                                std::string& error);
bool invoke_ui(const nlohmann::json& args,nlohmann::json& out,std::string& error);
bool read_ui(nlohmann::json& out,std::string& error);
}

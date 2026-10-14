#pragma once
#include "vendor/nlohmann/json.hpp"
#include "typed_action/action_session.h"
namespace mt2mp::game::placement_ui_diagnostics {
bool probe(const nlohmann::json&,nlohmann::json&,std::string&);
bool receipt(const nlohmann::json&,nlohmann::json&,std::string&);
bool unresolved() noexcept;
void after_game_update(uint64_t completed,float dt) noexcept;
void build_update_entered(void* receiver,float dt) noexcept;
bool release_ready(void* receiver,std::string&) noexcept;
bool capture_bound(void* receiver,const std::string& action_uuid,const action_session::BuildingState&,std::string&) noexcept;
void capture_failed(void* receiver,const std::string& cause) noexcept;
}

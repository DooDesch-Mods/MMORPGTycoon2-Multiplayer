#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::ui_move_runtime {
bool enable(std::string&);
bool enabled() noexcept;
void tick();
// Passive cleanup readback and command admission for the core Move capture.
void tick_core_capture();
nlohmann::json status();
bool move(const action_session::Request&,action_session::BuildingState&,nlohmann::json&,std::string&);
bool can_reclaim_global_minhook(std::string&);
namespace object_ui {
bool probe(const nlohmann::json&,nlohmann::json&,std::string&);
bool read(const nlohmann::json&,nlohmann::json&,std::string&);
bool unresolved() noexcept;
}
}

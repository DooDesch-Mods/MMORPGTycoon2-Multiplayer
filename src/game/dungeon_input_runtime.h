#pragma once
#include "typed_action/action_session.h"
#include "dungeon_input_authored.h"
#include "world_zone_runtime.h"
namespace mt2mp::game::npc_probe::monster_action_runtime::dungeon_input {
bool enable(std::string&);
bool retire_lobby(std::string&);
bool pending_ui();
bool capture_gizmo_move(uintptr_t behaviour,uintptr_t caller,bool& handled,std::string&);
bool capture_gizmo_destroy(uintptr_t object,uint64_t uid,std::string&);
bool capture_scene_remove(uintptr_t manager,uintptr_t array,bool& handled,std::string&);
bool preparing_gizmo_ui();bool gizmo_ui_owner(uintptr_t object,uint64_t uid);void reject_gizmo_ui();
void tick();
bool owns(const std::string& action);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
action_session::Outcome perform_history(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe_history(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire_history(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

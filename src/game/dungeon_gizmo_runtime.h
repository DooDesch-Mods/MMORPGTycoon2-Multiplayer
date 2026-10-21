#pragma once
#include "typed_action/action_session.h"
#include "object_identity/remove_runtime/copy/undo_witness.h"
#include "dungeon_gizmo_live_state.h"
#include <vendor/nlohmann/json.hpp>
namespace mt2mp::game::npc_probe::monster_action_runtime::dungeon_gizmo {
bool enable(std::string&);
bool rename_target(uintptr_t,std::string& key,std::string& scene,std::string& error);
bool rename_read(const action_session::Binding&,const std::string& snapshot,const std::string& key,const std::string& scene,
    action_session::DungeonGizmoState&,dungeon_gizmo_identity::Member&,action_session::Ledger&,build_actions::private_native::UndoWitness&,std::string&);
bool rename_apply(const action_session::Binding&,const std::string& snapshot,const std::string& key,const std::string& scene,
    const dungeon_gizmo_identity::Member&,const action_session::DungeonGizmoState&,const action_session::Ledger&,
    const build_actions::private_native::UndoWitness&,const std::string&,bool& returned,std::string&);
bool capture_live(const action_session::Context&,const action_session::Binding&,dungeon_gizmo_live::State&,std::string&);
bool apply_live(const action_session::Context&,const action_session::Binding&,const dungeon_gizmo_live::State&,dungeon_gizmo_live::Receipt&,std::string&);
bool retire_lobby(std::string&);bool pending_ui();void tick();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

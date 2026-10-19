#pragma once
#include "typed_action/action_session.h"
#include <vendor/nlohmann/json.hpp>
namespace mt2mp::game::npc_probe::monster_action_runtime::dungeon_cells {
action_session::Outcome perform_history(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe_history(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire_history(const action_session::Binding&,const action_session::Request&,std::string&);
bool enable(std::string&);bool retire_lobby(std::string&);bool pending_ui();void tick();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}
namespace mt2mp::game::npc_probe::monster_action_runtime::dungeon_palette {
bool enable(std::string&);
bool retire_lobby(std::string&);bool pending_ui();void tick();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

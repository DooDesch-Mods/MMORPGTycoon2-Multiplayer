#pragma once
#include "typed_action/action_session.h"
#include <vendor/nlohmann/json.hpp>
namespace mt2mp::game::npc_probe::monster_action_runtime::ability_edit {
bool enable(std::string&);
bool retire_lobby(std::string&);bool pending_ui();void tick();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

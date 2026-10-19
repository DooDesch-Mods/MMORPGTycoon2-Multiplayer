#pragma once
#include "typed_action/action_session.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::npc_probe::monster_action_runtime::ability_membership {
bool enable(std::string&);
void tick();
bool pending_ui();
bool retire_lobby(std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

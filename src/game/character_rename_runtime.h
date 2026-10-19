#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::npc_probe::monster_action_runtime::character_rename {
bool enable(std::string&);
bool retire_lobby(std::string&);
void tick();
bool pending_ui();
nlohmann::json status();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
}

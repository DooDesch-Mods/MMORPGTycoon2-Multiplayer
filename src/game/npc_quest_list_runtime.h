#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::npc_probe::monster_action_runtime::quest_list {
bool enable(std::string&);
bool retire_lobby(std::string&);
void tick();
bool pending_ui();
nlohmann::json status();
bool verify_ui(uintptr_t,bool,std::string&);
bool verify_npc_command(uintptr_t,std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool native_ui(const nlohmann::json&,nlohmann::json&,std::string&);
}

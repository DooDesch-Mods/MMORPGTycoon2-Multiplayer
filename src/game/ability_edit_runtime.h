#pragma once
#include <vendor/nlohmann/json.hpp>
#include "typed_action/action_session.h"
namespace mt2mp::game::npc_probe::monster_action_runtime::ability_edit {
bool native_apply_state(nlohmann::json&,std::string&);
bool capture_adjustment(void*,void*,bool&,std::string&);
bool presentation_route(std::string&);
bool enable(std::string&);
bool retire_lobby(std::string&);bool pending_ui();void tick();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

#pragma once
#include "typed_action/action_session.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::ui_move_runtime::district_edit {
bool enable(std::string&);
bool pending();
void tick();
void retire_session();
bool checkbox(uintptr_t,bool,void* original,bool& handled,std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
action_session::Outcome perform_history(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe_history(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire_history(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

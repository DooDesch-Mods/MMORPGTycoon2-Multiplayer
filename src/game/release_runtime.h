#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::release_runtime {
bool enable(std::string&);
bool pending_ui();bool retire_lobby(std::string&);void tick();
bool framework_retire(std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

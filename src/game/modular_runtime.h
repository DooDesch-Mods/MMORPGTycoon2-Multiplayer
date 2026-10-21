#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::ui_move_runtime::modular {
bool enable(std::string&);bool pending();void tick();void retire_session();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool publication(const action_session::Binding&,const action_session::Request&,action_session::ModularEffect&,std::string&);
bool targets(const action_session::Binding&,const action_session::Request&,std::vector<std::string>&,std::string&);
nlohmann::json status();
}

#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::bug_authority::manual_action {
bool enable(std::string&);
void tick();
bool pending_ui();
bool retire_lobby(std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
bool read_ui_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
bool run_ui_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
}

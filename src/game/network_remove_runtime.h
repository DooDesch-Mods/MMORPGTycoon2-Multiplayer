#pragma once
#include "typed_action/action_session.h"
#include "ui_remove/ui_capture.h"
namespace mt2mp::game::network_remove_runtime {
bool local_binding(uintptr_t manager,uintptr_t object,action_session::ui_capture::LocalBinding&,std::string&);
bool submit(const action_session::ui_capture::Intent&,std::string&);
bool authorize(const action_session::ui_capture::Replay&,std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool unresolved();
nlohmann::json status();
}

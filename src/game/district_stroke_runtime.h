#pragma once
#include "typed_action/action_session.h"
#include "ui_remove/ui_capture.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::ui_move_runtime::district_stroke {
bool enable(std::string&);
bool pending();void tick();void retire_session();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
action_session::Outcome perform_history(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe_history(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire_history(const action_session::Binding&,const action_session::Request&,std::string&);
bool remove_local_binding(uintptr_t,uintptr_t,action_session::ui_capture::LocalBinding&,std::string&);
bool submit_remove(const action_session::ui_capture::Intent&,std::string&);
bool authorize_remove_replay(const action_session::ui_capture::Replay&,std::string&);
nlohmann::json status();
}

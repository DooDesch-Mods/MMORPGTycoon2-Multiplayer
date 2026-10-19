#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::ui_move_runtime::selection_move {
bool capture_purchase(uintptr_t,uintptr_t,bool&,std::string&);
void tick();
bool pending();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool history_intent(const action_session::Binding&,const action_session::Request&,action_session::SelectionMoveIntent&,std::string&);
action_session::Outcome perform_history(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe_history(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire_history(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

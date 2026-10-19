#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::ui_move_runtime::prop_text {
bool enable(std::string&);
void tick();
bool pending();
// Called by the shared Prop::Rename hook only for the exact HeaderView caller.
bool capture_rename(uintptr_t,const void*,bool&,std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

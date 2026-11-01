#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::typed_action_runtime {
bool held_tutorial_session_read(action_session::Session&,action_session::Context&,std::string&);
bool held_tutorial_read_binding(const std::string&,action_session::Binding&,std::string&);
bool held_tutorial_capture(action_session::Context&,action_session::Binding&,bool&,std::string&);
}

#pragma once
#include "typed_action/action_session.h"
#include "general_project_state.h"
#include <map>
namespace mt2mp::game::general_project_runtime {
bool enable(std::string&);
bool capture_ui(void* window,void* record,bool& handled,bool& accepted);
bool original_route(uintptr_t entry,void* receiver,int32_t workers,bool& handled);
bool admit_bug_birth(const action_session::Context&,const action_session::Binding&,const std::string& action,uintptr_t caller,bool& owned,std::string&);
bool pending_ui();bool retire_lobby(std::string&);void tick();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool capture_progress(const action_session::Context&,const action_session::Binding&,general_project::Progress&,std::string&);
bool release_reference_keys(const action_session::Context&,const action_session::Binding&,std::map<uintptr_t,std::string>&,std::string&);
bool apply_progress(const action_session::Context&,const action_session::Binding&,const general_project::Progress&,bool& attempted,uint32_t& attempted_stores,uint32_t& stores,std::string&);
nlohmann::json status();
}

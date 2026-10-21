#pragma once
#include "typed_action/action_session.h"
#include <vector>
#include "ui_remove/ui_capture.h"
namespace mt2mp::game::ui_path_runtime {
bool enable(std::string&);
bool remove_local_binding(uintptr_t,uintptr_t,action_session::ui_capture::LocalBinding&,std::string&);
bool submit_remove(const action_session::ui_capture::Intent&,std::string&);
action_session::Outcome perform_history(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe_history(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire_history(const action_session::Binding&,const action_session::Request&,std::string&);
bool unresolved();
bool world_path_pending();
void retire_world();
void tick();
nlohmann::json status();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool query(int32_t,nlohmann::json&,std::string&);
bool region_members(int32_t,std::vector<uint64_t>&,std::vector<uint64_t>&,std::string&);
bool native_delete(const nlohmann::json&,nlohmann::json&,std::string&);
bool native_segment(const nlohmann::json&,nlohmann::json&,std::string&);
}

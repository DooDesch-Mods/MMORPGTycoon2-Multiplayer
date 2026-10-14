#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::ui_terrain_runtime {
bool enable(std::string&);
bool unresolved();
void tick();
nlohmann::json status();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool retired_keys(const action_session::Binding&,const action_session::Request&,std::vector<std::string>&,std::string&);
bool query(int32_t,nlohmann::json&,std::string&);
bool native_height(const nlohmann::json&,nlohmann::json&,std::string&);
bool native_stroke(const nlohmann::json&,nlohmann::json&,std::string&);
}

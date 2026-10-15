#pragma once
#include "shared_state.h"
#include "typed_action/action_session.h"
namespace mt2mp::game::developer_staff::action_runtime {
bool enable(std::string&);
bool enabled();
bool verify_entry(uintptr_t,std::string&);
void tick();
bool pending_ui();
bool retire_lobby(std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool capture(const action_session::Context&,const action_session::Binding&,PoolState&,std::string&);
bool preflight_frame(const action_session::Context&,const action_session::Binding&,const PoolState&,std::string&);
bool adopt_frame(const action_session::Context&,const action_session::Binding&,const PoolState&,bool& attempted,bool& unknown,std::string&);
nlohmann::json status();
bool ui_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
}

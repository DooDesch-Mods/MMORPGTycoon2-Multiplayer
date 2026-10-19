#pragma once
#include "intervention_actor_runtime.h"
#include "intervention_actor_lifecycle.h"
#include "live_host_state/state.h"

namespace mt2mp::game::interventions::actors::live {
bool enable(std::string&);
bool bind(const action_session::Context&,const action_session::Binding&,uint64_t,std::string&);
bool suspend(std::string&);
bool capture(const action_session::Context&,const action_session::Binding&,State&,lifecycle::Batch&,std::string&);
bool apply(const action_session::Context&,const action_session::Binding&,const live_host_state::Frame&,Receipt&,std::string&);
bool sent(const live_host_state::Frame&,std::string&);
bool acknowledge(uint64_t,uint64_t,std::string&);
bool admit(const live_host_state::Frame&,const live_host_state::Frame*,std::string&);
bool begin_ordered(const action_session::Context&,const action_session::Binding&,const std::string&,std::string&);
bool end_ordered(bool,std::string&);
// The existing physical Monster routes delegate natural Zone owners here.
bool monster_update(uintptr_t,float,bool& handled,std::string&);
bool adjustment(uintptr_t,bool& handled,std::string&);
bool verify_observer(uintptr_t,const char*,bool& handled,std::string&);
nlohmann::json status();
}

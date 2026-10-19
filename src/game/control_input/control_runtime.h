#pragma once
#include "control_replica.h"
#include "../typed_action/action_session.h"
namespace mt2mp::control_input::runtime {
bool enable(std::string&);
// When no Control runtime exists, owned remains false and the caller must
// still validate its complete pristine pin. A published owner must verify.
bool verify_item_activate(uintptr_t image,size_t bytes,const std::string& sha,bool& owned,std::string&);
bool verify_clock_time_scale(uintptr_t image,size_t bytes,const std::string& sha,bool& owned,std::string&);

bool bind(const action_session::Context&,const action_session::Binding&,uint64_t,std::string&);
bool suspend(std::string&);
bool retire_lobby(std::string&);
bool capture_acquire(uintptr_t,std::string&);
bool pending();
bool replaying();
void tick();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool capture(const action_session::Context&,const action_session::Binding&,ReplicaState&,std::string&);
bool apply(const action_session::Context&,const action_session::Binding&,const ReplicaState&,bool& attempted,std::string&);
nlohmann::json status();
}

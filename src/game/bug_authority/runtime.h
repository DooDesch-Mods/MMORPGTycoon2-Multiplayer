#pragma once
#include "state.h"
#include "job_state.h"
#include "typed_action/action_session.h"
#include <cstddef>
namespace mt2mp::game::bug_authority {
using action_session::Context;
using action_session::Binding;
bool initialize(std::string&);
bool capture(const Context&,const Binding&,uint64_t publication,State&,std::string&);
bool preflight_frame(const Context&,const Binding&,uint64_t publication,const State&,std::string&,const WorkState* work=nullptr);
bool adopt_frame(const Context&,const Binding&,uint64_t publication,const State&,bool& attempted,bool& unknown,std::string&);
bool finish_frame(const Context&,const Binding&,uint64_t publication,const State&,bool& attempted,bool& unknown,std::string&);
bool verify_job_destructor(std::string&);
// The existing ordered action owns this scope, including original history loads.
bool begin_action(const Context&,const Binding&,const std::string& action,std::string&);
bool end_action(bool confirmed,std::string&);
bool arm_snapshot(const std::string& uuid,const std::string& sha,bool host,std::string&);
bool seal_snapshot(const std::string& uuid,const std::string& sha,std::string&);
void cancel_snapshot(const std::string&);
}

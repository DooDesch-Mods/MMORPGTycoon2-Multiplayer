#pragma once
#include "state.h"
#include "typed_action/action_session.h"
namespace mt2mp::game::tutorial_replication {
bool initialize(uintptr_t image,std::string&);
bool hooks_enabled(std::string&);
bool verify_sequence_entry(uintptr_t address,std::string&);
bool verify_commit_entry(uintptr_t address,std::string&);
bool verify_pay_entry(uintptr_t address,std::string&);
bool start_startingpoint(const action_session::Context&,const action_session::Binding&,
    const action_session::Ledger&,const startingpoint::Row&,action_session::Ledger&,startingpoint::Row&,bool&,std::string&);
bool apply_startingpoint_start(const action_session::Context&,const action_session::Binding&,const std::string& action,
    const action_session::Ledger&,const action_session::Ledger&,const startingpoint::Row&,const startingpoint::Row&,bool&,bool&,std::string&);

bool arm_client_load(const std::string& local_uuid,uint64_t generation,std::string&);
bool finish_client_load(bool& pending,std::string&);
void invalidate(const std::string& cause);
void retire(const std::string& cause);
bool capture(const action_session::Context&,const action_session::Binding&,State&,std::string&);
bool read_startingpoint_row(const action_session::Context&,const action_session::Binding&,startingpoint::Row&,std::string&);
bool commit_startingpoint(const action_session::Context&,const action_session::Binding&,const startingpoint::Row& expected,bool& attempted,std::string&);
bool apply_startingpoint_commit(const action_session::Context&,const action_session::Binding&,const startingpoint::Row& before,const startingpoint::Row& after,bool& attempted,bool& unknown,std::string&);
bool read_progress(nlohmann::json&,std::string&);
bool preflight(const action_session::Context&,const action_session::Binding&,const State&,std::string&);
bool apply(const action_session::Context&,const action_session::Binding&,const State&,bool& attempted,bool& unknown,std::string&);
}

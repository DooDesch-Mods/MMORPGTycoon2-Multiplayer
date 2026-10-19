#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::management_runtime {
bool enable(std::string&);
void tick();
bool pending_ui();
// True only while the validated ordered intervention invokes its original.
bool intervention_replaying();
bool retire_lobby(std::string&);
// Existing economic hook owns the Loan leaves. It calls this before its fence.
// false means foreign/original replay; true means the normal UI was consumed.
bool capture_loan(bool repay,void*,const void*,uintptr_t caller,bool& accepted);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
nlohmann::json status();
}

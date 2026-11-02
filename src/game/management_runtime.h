#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::management_runtime {
bool enable(std::string&);
void tick();
void tick_pending();
bool ordered_pending(const action_session::Context&,const action_session::Binding&,int32_t,std::string&);
bool pending_ui();
bool gm_ui_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
bool loan_ui_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
bool campaign_ui_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
bool service_sync_ui_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
bool service_price_ui_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
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

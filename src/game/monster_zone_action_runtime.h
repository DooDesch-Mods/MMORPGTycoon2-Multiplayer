#pragma once
#include "typed_action/action_session.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::npc_probe::zone_action_runtime {
bool enable(std::string&);
void tick();
bool pending_ui();
nlohmann::json status();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,
    const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool verify_owned_entry(uintptr_t rva,std::string&);
bool begin_ui_capture(const std::string& token,std::string&);
void end_ui_capture();
nlohmann::json ui_receipt(const std::string& token);
}

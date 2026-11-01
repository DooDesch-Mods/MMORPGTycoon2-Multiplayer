#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
#include <functional>
#include "recovery_owner.h"
namespace mt2mp::game::session_reload {
bool initialize(std::string& error);
bool bind_recovery(recovery::Owner&,ordered_session::ProofOwner&,const MT2FW_MT2_StepRecoveryDraftV1&,const MT2FW_MT2_StepReceipt&,uint8_t,std::string&);
bool retire_recovery_owner(const recovery::Owner&,std::string&);
bool retain_recovery_capture(const std::string&,recovery::Owner::Capture,std::string&);
bool recovery_capture_pending(const std::string&);
bool recovery_capture_completed(const std::string&);
bool recovery_capture_consumed(const std::string&,std::string&);
bool recovery_active();
bool recovery_read(const std::string&,const std::function<bool(std::string&)>&,std::string&);
bool busy();
const std::string& failure_reason();
void admit_refreshed_ordered_client();
bool request_resync(std::string& error);
bool release_for_leave(std::string& error);
void tick();
nlohmann::json status();
}

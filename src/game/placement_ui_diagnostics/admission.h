#pragma once
#include "vendor/nlohmann/json.hpp"
#include "placement_ui_probe/ui_probe.h"
#include "typed_action/action_session.h"
namespace mt2mp::game::placement_ui_diagnostics {
struct Input {
    placement_ui_probe::Request native;
    std::string snapshot_uuid;
    uint64_t epoch{},generation{};
};
struct Job {
    Input input;
    placement_ui_probe::Binding binding;
    placement_ui_probe::Receipt receipt;
    bool reserved{};
    uint64_t armed_at{};
    uint64_t capture_frame{};
    bool confirmation_observed=false;
    uint64_t confirmation_observed_at=0;
    std::string recovered_timeout_cause,recovery_cause;
    nlohmann::json fresh_pose;
    nlohmann::json arm_observation,first_frame,last_frame,first_build_entry,last_build_entry;
    uint64_t armed_completed_frame=0,frames_observed=0,build_entries_observed=0,observation_failures=0;
    std::string first_observation_error,last_observation_error;
};
bool parse_input(const nlohmann::json&,Input&,std::string&);
bool parse_token(const nlohmann::json&,std::string&,std::string&);
bool bind_release(Job&,uintptr_t,const placement_ui_probe::NativePose&,std::string&);
bool bind_capture(Job&,uintptr_t,const std::string&,const action_session::BuildingState&,std::string&);
bool observe_confirmation(Job&,const nlohmann::json& controller,const nlohmann::json& native_receipts,std::string&,bool aggregate_host_acks=true);
std::string observation_timeout(const Job&,uint64_t now);
bool can_reconcile_timeout(const Job&) noexcept;
bool publish_reconciliation(Job&,const Job&,std::string&);
bool may_read_fresh_pose(const Job&,uint64_t completed) noexcept;
}

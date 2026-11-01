#pragma once
#include "mt2mp/net/sidecar_client.h"
#include <array>
#include <string>

namespace mt2mp::net::session_wire {
bool prepare(std::string& error);
bool tick(std::string& error);
bool connected();
size_t peers();
uint8_t peer_mask();
uint64_t local_peer();
uint64_t session_id();
bool send(uint8_t channel,const uint8_t*,uint32_t,uint64_t target=0);
void register_handler(uint8_t channel,SidecarHandler);
bool claim_handler(uint8_t channel,SidecarHandler,std::string& error);
std::string failure_reason();
struct ProcessCohort {
    std::array<uint64_t,5> physical_ids{},process_nonces{};
    uint64_t session=0,lobby=0,local=0;
    bool operator==(const ProcessCohort&)const=default;
};
// Observed process identity is independent of the committed world roster.
// Local physical IDs are authenticated relay slots; Steam IDs are accounts.
bool read_process_cohort(ProcessCohort&,std::string& error);
// Slot mapping is acknowledged owner metadata, not the application's claimed sender.
bool read_binding(std::array<uint64_t,5>& ids,uint64_t& lobby,uint64_t& local,std::string& error,bool retain_original=false);
}

namespace mt2mp::net::session_wire { void suspend_actions(); bool retire_lobby(std::string&); }

namespace mt2mp::net::session_wire {
bool membership_pending();
uint8_t membership_retirement_mask();
bool retire_world(std::string&);
bool refresh_membership(std::string&);
}

#pragma once
#include <string>
#include <cstddef>
#include <cstdint>
namespace mt2mp::net {
bool start_local_session();
// Dispatch engine packets and session UI only from the game thread.
void tick_local_session();
bool local_session_connected();
size_t local_session_peers();
uint8_t local_session_peer_mask();
std::string local_session_status();
// The retained notice is scoped to the current shared snapshot, not transport lifetime.
bool publish_local_host_leave(std::string& error);
bool local_host_left(const std::string& snapshot);
bool prepare_local_membership(std::string& error);
uint64_t local_membership_generation();
bool local_membership_pending();
uint8_t local_membership_retirement_mask();
void retire_local_membership();
bool refresh_local_membership(std::string& error);
bool join_local_lobby(std::string& error);
bool leave_local_lobby(std::string& error);
}

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
}

#include "mt2mp/net/net_channel.h"
#include "mt2mp/core/config.h"

namespace mt2mp::net {

void NetChannel::update_heartbeat(uint64_t steam_id) {
    peers_[steam_id].last_heartbeat = std::chrono::steady_clock::now();
}

bool NetChannel::is_alive(uint64_t steam_id, int timeout_ms) const {
    auto it = peers_.find(steam_id);
    if (it == peers_.end()) return false;
    auto elapsed = std::chrono::steady_clock::now() - it->second.last_heartbeat;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < timeout_ms;
}

void NetChannel::remove(uint64_t steam_id) {
    peers_.erase(steam_id);
}

float NetChannel::get_rtt(uint64_t steam_id) const {
    auto it = peers_.find(steam_id);
    return it != peers_.end() ? it->second.rtt_ms : 0.0f;
}

} // namespace mt2mp::net

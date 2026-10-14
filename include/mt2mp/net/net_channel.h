#pragma once

#include "mt2mp/net/packet_types.h"
#include <cstdint>
#include <chrono>
#include <unordered_map>

namespace mt2mp::net {

// Tracks per-peer connection quality and heartbeat
class NetChannel {
public:
    void update_heartbeat(uint64_t steam_id);
    bool is_alive(uint64_t steam_id, int timeout_ms) const;
    void remove(uint64_t steam_id);

    float get_rtt(uint64_t steam_id) const; // estimated round-trip time in ms

private:
    struct PeerState {
        std::chrono::steady_clock::time_point last_heartbeat;
        float rtt_ms = 0.0f;
    };
    std::unordered_map<uint64_t, PeerState> peers_;
};

} // namespace mt2mp::net

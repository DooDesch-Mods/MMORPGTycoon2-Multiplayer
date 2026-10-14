#pragma once

#include "mt2mp/net/packet_types.h"
#include <cstdint>

namespace mt2mp::game {

// Tracks high-level game state for desync detection
struct GameStateSnapshot {
    uint32_t building_count = 0;
    uint32_t monster_count = 0;
    uint32_t npc_count = 0;
    uint64_t gold = 0;
    uint32_t game_day = 0;
    float day_fraction = 0.0f;

    uint32_t compute_hash() const;
    net::ChecksumPayload to_checksum() const;
};

class GameStateTracker {
public:
    static GameStateTracker& instance();

    // Capture current state from game memory (requires RE'd pointers)
    GameStateSnapshot capture();

    // Compare with remote checksum
    bool matches(const net::ChecksumPayload& remote) const;

    const GameStateSnapshot& last_snapshot() const;

private:
    GameStateTracker() = default;
    GameStateSnapshot last_;
};

} // namespace mt2mp::game

#include "mt2mp/game/game_state.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::game {

uint32_t GameStateSnapshot::compute_hash() const {
    // FNV-1a hash of the snapshot fields
    uint32_t hash = 2166136261u;
    auto mix = [&hash](uint32_t val) {
        hash ^= val;
        hash *= 16777619u;
    };
    mix(building_count);
    mix(monster_count);
    mix(npc_count);
    mix(static_cast<uint32_t>(gold & 0xFFFFFFFF));
    mix(static_cast<uint32_t>(gold >> 32));
    mix(game_day);
    mix(*reinterpret_cast<const uint32_t*>(&day_fraction));
    return hash;
}

net::ChecksumPayload GameStateSnapshot::to_checksum() const {
    return {building_count, monster_count, npc_count, gold, game_day, day_fraction, compute_hash()};
}

GameStateTracker& GameStateTracker::instance() {
    static GameStateTracker tracker;
    return tracker;
}

GameStateSnapshot GameStateTracker::capture() {
    // TODO: Read actual values from game memory once pointers are RE'd
    // For now return cached state
    LOG_DEBUG("GameStateTracker::capture() - using placeholder");
    return last_;
}

bool GameStateTracker::matches(const net::ChecksumPayload& remote) const {
    auto local = last_.to_checksum();
    return local.hash == remote.hash;
}

const GameStateSnapshot& GameStateTracker::last_snapshot() const {
    return last_;
}

} // namespace mt2mp::game

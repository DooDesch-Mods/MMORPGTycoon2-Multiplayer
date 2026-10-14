#pragma once

#include <cstdint>
#include <unordered_map>
#include <chrono>
#include <optional>

namespace mt2mp::game {

class RegionLockManager {
public:
    static RegionLockManager& instance();

    // Host-side: try to grant lock
    bool try_lock(uint32_t region_id, uint64_t steam_id);
    void release(uint32_t region_id, uint64_t steam_id);
    void release_all(uint64_t steam_id); // on disconnect
    std::optional<uint64_t> owner_of(uint32_t region_id) const;

    // Expire stale locks
    void update();

private:
    RegionLockManager() = default;

    struct LockEntry {
        uint64_t owner_steam_id;
        std::chrono::steady_clock::time_point last_activity;
    };

    std::unordered_map<uint32_t, LockEntry> locks_;
};

} // namespace mt2mp::game

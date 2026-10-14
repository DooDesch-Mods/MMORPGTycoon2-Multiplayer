#include "mt2mp/game/region_lock.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::game {

RegionLockManager& RegionLockManager::instance() {
    static RegionLockManager mgr;
    return mgr;
}

bool RegionLockManager::try_lock(uint32_t region_id, uint64_t steam_id) {
    auto it = locks_.find(region_id);
    if (it != locks_.end() && it->second.owner_steam_id != steam_id) {
        LOG_DEBUG("Region {} locked by {}, denied to {}",
            region_id, it->second.owner_steam_id, steam_id);
        return false;
    }

    locks_[region_id] = {steam_id, std::chrono::steady_clock::now()};
    LOG_DEBUG("Region {} locked by {}", region_id, steam_id);
    return true;
}

void RegionLockManager::release(uint32_t region_id, uint64_t steam_id) {
    auto it = locks_.find(region_id);
    if (it != locks_.end() && it->second.owner_steam_id == steam_id) {
        locks_.erase(it);
        LOG_DEBUG("Region {} released by {}", region_id, steam_id);
    }
}

void RegionLockManager::release_all(uint64_t steam_id) {
    for (auto it = locks_.begin(); it != locks_.end(); ) {
        if (it->second.owner_steam_id == steam_id) {
            it = locks_.erase(it);
        } else {
            ++it;
        }
    }
}

std::optional<uint64_t> RegionLockManager::owner_of(uint32_t region_id) const {
    auto it = locks_.find(region_id);
    if (it != locks_.end()) return it->second.owner_steam_id;
    return std::nullopt;
}

void RegionLockManager::update() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = locks_.begin(); it != locks_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_activity);
        if (elapsed.count() >= REGION_LOCK_TIMEOUT_SEC) {
            LOG_INFO("Region {} lock expired (owner: {})", it->first, it->second.owner_steam_id);
            it = locks_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace mt2mp::game

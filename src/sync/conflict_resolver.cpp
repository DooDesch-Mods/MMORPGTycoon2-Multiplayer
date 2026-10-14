#include "mt2mp/sync/conflict_resolver.h"
#include "mt2mp/game/region_lock.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::sync {

ConflictResolver& ConflictResolver::instance() {
    static ConflictResolver cr;
    return cr;
}

ConflictResult ConflictResolver::check(const game::EditOperation& op) {
    auto& locks = game::RegionLockManager::instance();

    // Check region lock
    auto owner = locks.owner_of(op.region_id);
    if (owner.has_value() && *owner != op.player_steam_id) {
        return ConflictResult::Denied_RegionLocked;
    }

    // TODO: Spatial conflict detection
    // Check if another entity already exists at the placement position

    return ConflictResult::Allowed;
}

const char* ConflictResolver::result_string(ConflictResult r) {
    switch (r) {
        case ConflictResult::Allowed: return "Allowed";
        case ConflictResult::Denied_RegionLocked: return "Region is locked by another player";
        case ConflictResult::Denied_SpatialConflict: return "Space is already occupied";
        case ConflictResult::Denied_HostOverride: return "Host override";
        default: return "Unknown";
    }
}

} // namespace mt2mp::sync

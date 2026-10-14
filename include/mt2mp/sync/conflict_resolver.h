#pragma once

#include "mt2mp/game/edit_operation.h"
#include <cstdint>

namespace mt2mp::sync {

enum class ConflictResult {
    Allowed,
    Denied_RegionLocked,
    Denied_SpatialConflict,
    Denied_HostOverride,
};

class ConflictResolver {
public:
    static ConflictResolver& instance();

    // Check if an edit is allowed (host-side only)
    ConflictResult check(const game::EditOperation& op);

    // Describe denial reason
    static const char* result_string(ConflictResult r);

private:
    ConflictResolver() = default;
};

} // namespace mt2mp::sync

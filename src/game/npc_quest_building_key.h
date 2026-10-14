#pragma once
#include "npc_quest_targets.h"
#include "remove_diagnostics.h"

namespace mt2mp::game::npc_quest_targets {
inline bool building_key(const World& world, uintptr_t pointer, std::string& key, std::string& error) {
    key.clear();
    remove_diagnostics::LocalTarget target;
    if (!remove_diagnostics::resolve_local_target(world.selection, pointer, target, error)) return false;
    if (!valid_origin(world.origin) || !target.normal_building || target.object != pointer || target.manager != world.selection ||
        target.snapshot_id != world.origin.uuid || target.world_epoch != world.origin.world_epoch ||
        target.world_generation != world.origin.world_generation || target.record_key.empty())
        return fail(error, "quest Building existing binding is not in the current snapshot/world");
    key = target.record_key; error.clear(); return true;
}
}

#pragma once
#include "npc_quest_targets.h"
#include "remove_diagnostics.h"

namespace mt2mp::game::npc_quest_targets {
inline bool building_key(const World& world, uintptr_t pointer, std::string& key, std::string& error) {
    key.clear();
    remove_diagnostics::LocalTarget target;
    if (!remove_diagnostics::resolve_local_target(world.selection, pointer, target, error)) return false;
    if (!valid_origin(world.origin) || target.object != pointer || target.manager != world.selection ||
        target.snapshot_id != world.origin.uuid || target.world_epoch != world.origin.world_epoch ||
        target.world_generation != world.origin.world_generation || target.record_key.empty())
        return fail(error, "quest Building existing binding is not in the current snapshot/world"
            " uid=" + std::to_string(target.uid) + " key=" + target.record_key +
            " type=" + target.type + " variant=" + target.variant +
            " normal_building=" + std::to_string(target.normal_building) +
            " normal_scenery=" + std::to_string(target.normal_scenery) + " classification=" + target.kind_cause +
            " valid_origin=" + std::to_string(valid_origin(world.origin)) +
            " object_matches=" + std::to_string(target.object == pointer) +
            " manager_matches=" + std::to_string(target.manager == world.selection) +
            " required_snapshot=" + world.origin.uuid + " actual_snapshot=" + target.snapshot_id +
            " required_epoch=" + std::to_string(world.origin.world_epoch) + " actual_epoch=" + std::to_string(target.world_epoch) +
            " required_generation=" + std::to_string(world.origin.world_generation) +
            " actual_generation=" + std::to_string(target.world_generation));
    if (!target.normal_building) {
        if (target.normal_scenery || target.type != "Scenery" || target.variant != "base")
            return fail(error, "quest Building target is neither normal nor an exact native Modular Building");
        // Original Building target pools also contain Modular Building objects.
        // Keep the existing full UID, native class, non-Art and owned Layout/Plan proof.
        if (!remove_diagnostics::is_modular_target(target, error)) return false;
    }
    key = target.record_key; error.clear(); return true;
}
}

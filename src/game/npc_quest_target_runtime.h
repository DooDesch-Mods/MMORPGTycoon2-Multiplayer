#pragma once
#include "npc_quest_targets.h"
#include "dungeon_scene_identity_runtime.h"
#include <cstdint>
#include <string>
#include <set>
#include <vector>

namespace mt2mp::game::npc_quest_target_runtime {
struct CurrentTargets {
    std::string snapshot_uuid;
    int32_t saved_region = -1;
    std::vector<std::string> building_keys, monster_zone_keys;
};
// Both calls require the same current confirmed snapshot and unchanged saved
// Monster pool. Building keys use the existing live Snapshot/Action bindings.
// Vectors preserve the original RNG pool order; no pointer enters this result.
bool read_current(int32_t saved_region, CurrentTargets&, std::string& error);
bool bind_destination(int32_t saved_region, uintptr_t destination, std::string& key, std::string& error);

// The existing Controller brackets one original new-zone action with these calls.
// No key is published until the exact native append and old members are checked.
bool begin_zone_append(int32_t saved_region, const std::string& action_uuid, std::string& error);
bool finish_zone_append(int32_t saved_region, const std::string& action_uuid, uintptr_t created,
    uint64_t uid, const npc_quest_targets::Content& required, std::string& key, std::string& error);

using PropertyCall = bool(*)(const void*, void*, void*, void*);
using Ticket = uint64_t;
bool initialize(std::string& error);
bool stage_scenes(const dungeon_scene_identity_runtime::Census&,std::string& error);
bool bind_scene_destination(uintptr_t destination,std::string& key,std::string& error);
// Resolve an existing Quest reference by its actual current scene, independent of the camera.
bool bind_current_destination(uintptr_t destination,std::string& key,bool& in_dungeon,std::string& error);
struct DungeonCellCascade {
    std::string snapshot,request;uint64_t epoch=0,generation=0;
    std::map<uintptr_t,npc_quest_targets::SavedPool> before;
    bool confirmed=false;
};
bool prepare_dungeon_cells(const action_session::Binding&,const action_session::Request&,DungeonCellCascade&,std::string&);
bool confirm_dungeon_cells(const action_session::Binding&,const action_session::Request&,DungeonCellCascade&,std::string&);
bool confirm_dungeon_cell_history(const action_session::Binding&,const action_session::Request&,DungeonCellCascade& current,
    const DungeonCellCascade& original,const std::set<uint64_t>& restored_zones,std::string&);
void* entrypoint(bool save);
bool confirm_hooks(std::string& error);
// BuildingCapture supplies its freshly checked world before any NativeOpen load.
struct CaptureWorld {
    void* game = nullptr; void* mode = nullptr; void* state = nullptr;
    void* world = nullptr; void* map = nullptr;
};
bool arm_host(const std::string& uuid, const CaptureWorld&, std::string& error);
bool seal_host(const std::string& uuid, const std::string& sha, const CaptureWorld&, std::string& error);
bool arm_client(const std::string& uuid, const std::string& sha, std::string& error);
bool seal_client(std::string& error);
void cancel(const std::string& cause);
Ticket save_before(const void* property, void* manager, void* writer, void* context) noexcept;
void save_after(Ticket, const void* property, void* manager, void* writer, void* context, bool original_result) noexcept;
Ticket load_before(const void* property, void* manager, void* reader, void* context) noexcept;
void load_after(Ticket, const void* property, void* manager, void* reader, void* context, bool original_result) noexcept;
void children_before(uintptr_t caller_rva, void* reader) noexcept;
void children_after(uintptr_t caller_rva, void* reader, int32_t count) noexcept;
void registered(uintptr_t caller_rva, void* context, int32_t id, void* object) noexcept;
void original_failed(Ticket, const std::string& cause) noexcept;
}

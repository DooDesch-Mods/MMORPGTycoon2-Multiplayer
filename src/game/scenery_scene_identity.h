#pragma once
#include "building_scene_identity.h"
#include "scenery_scene_key.h"
#include "scenery_scene_snapshot_alias.h"
namespace mt2mp::game::remove_diagnostics::scene_scenery {
using Row=scene_buildings::Row;
// Classification only. Callers still use the existing fresh LocalTarget and
// native readback before an original operation.
bool registered(const std::string& snapshot,const std::string& key,uint64_t uid);
bool owner(uintptr_t object,uint64_t uid,Row&,dungeon_scene_identity_runtime::World&,std::string&);
bool resolve(const dungeon_scene_identity_runtime::World&,uintptr_t object,uint64_t uid,std::string& key,std::string&);
bool prepare_saved(const dungeon_scene_identity_runtime::Census&,const scenery_scene_snapshot_alias::Manifest&,std::string&);
bool capture_saved(const dungeon_scene_identity_runtime::Census&,scenery_scene_snapshot_alias::Manifest&,std::string&);
bool append_saved(nlohmann::json&,std::string&);
}

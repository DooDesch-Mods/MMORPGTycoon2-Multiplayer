#pragma once
#include "dungeon_gizmo_identity.h"
#include "dungeon_gizmo_reader.h"
#include "dungeon_gizmo_snapshot_alias.h"
#include "dungeon_scene_roster.h"
#include "dungeon_zone_roster.h"
#include "object_identity/remove_runtime/copy/scenery_witness.h"
#include <functional>
#include "typed_action/action_session.h"
namespace mt2mp::game::dungeon_scene_records {struct Witness;using External=std::function<bool(uintptr_t,std::string&,std::string&)>;}

namespace mt2mp::game::dungeon_scene_identity_runtime {
struct World {
    void* game=nullptr;void* mode=nullptr;void* state=nullptr;void* world=nullptr;void* map=nullptr;
    uint64_t generation=0,world_epoch=0;std::string uuid,sha256;
};
struct Exterior {uint64_t uid=0;std::string record;};
struct Pool {
    dungeon_scene_roster::Scene scene;
    std::array<uintptr_t,4> managers{};
    std::vector<dungeon_gizmo_reader::Record> gizmos;
    dungeon_gizmo_reader::Local local;
    dungeon_zone_roster::Census zones;
    mt2mp::build_actions::private_native::place::SceneryWitness scenery;
};
struct Census {World world;std::vector<Pool> pools;std::vector<dungeon_gizmo_identity::Saved> saved;std::vector<std::string> keys;bool snapshot_census=false;};
struct CaptureWorld {void* game=nullptr;void* mode=nullptr;void* state=nullptr;void* world=nullptr;void* map=nullptr;};
// Root calls classify after actual root Building array seal and before Monster
// pool seal. The supplied records come from that exact native snapshot receipt.
bool classify(const World&,const std::vector<Exterior>&,Census&,std::string& error);
bool classify_capture(const std::string& uuid,const std::string& sha,const CaptureWorld&,const std::vector<Exterior>&,Census&,std::string& error);
bool classify_loaded(const std::string& uuid,const std::string& sha,const std::vector<Exterior>&,Census&,std::string& error);
// Fresh current rosters use only confirmed keys. They cannot seed saved identity.
bool read_current(Census&,std::string& error);
// Dynamic Instance children need their actual scene owners before birth aliases
// exist. This reads only original scene membership and confirmed root exteriors.
bool read_scene_roster(World&,std::vector<dungeon_scene_roster::Scene>&,std::string& error);
bool confirm_cell_cascade(const action_session::Binding&,const action_session::Request&,const Census& before,Census& after,std::string& error);
struct GizmoRestore {std::string key;dungeon_gizmo_identity::Member before,after;};
bool confirm_cell_restoration(const action_session::Binding&,const action_session::Request&,const Census& before,
    const std::vector<GizmoRestore>&,const std::vector<std::pair<std::string,dungeon_gizmo_identity::Member>>& retired,Census& after,std::string& error);
// Complete original Region/actor records and Dungeon layout/paint records. The
// resolver uses a precomputed external census and must not reenter this reader.
bool read_records(const dungeon_scene_records::External&,Census&,dungeon_scene_records::Witness&,std::string& error);
bool seal_snapshot(const Census&,const dungeon_gizmo_snapshot_alias::Manifest* aliases,std::string& error);
bool capture_aliases(const Census& current,const std::string& next_sha,dungeon_gizmo_snapshot_alias::Manifest&,std::string& error);
bool current(dungeon_gizmo_identity::Scope&,std::string& error);
bool resolve(uintptr_t object,uint64_t full_uid,std::string& key,std::string& error);
bool lookup(const std::string& key,dungeon_gizmo_identity::Member&,std::string& error);
bool confirmed_birth(const std::string& action_uuid,const std::vector<dungeon_gizmo_identity::Member>&,std::string& error);
bool confirmed_retirement(const std::string& key,const dungeon_gizmo_identity::Member&,std::string& error);
struct ClonePreview {
    dungeon_gizmo_identity::Scope scope;
    dungeon_gizmo_identity::Registry before,after;
    bool prepared=false;
};
bool preview_clones(const World&,const std::vector<std::pair<std::string,dungeon_gizmo_identity::Member>>& births,
    const std::vector<std::pair<std::string,dungeon_gizmo_identity::Member>>& retired,ClonePreview&,Census&,std::string&);
bool check_clones(const ClonePreview&,std::string&);
bool publish_clones(ClonePreview&,std::string&);
void invalidate();
}

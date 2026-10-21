#pragma once
#include "typed_action/dungeon_building_action.h"
#include "dungeon_scene_identity_runtime.h"
#include "duplicate_building_history.h"
#include "monster_quest_retirement.h"
#include "building_npc_lifecycle.h"
#include "building_npc_scope.h"
#include "duplicate_service_history.h"
#include "duplicate_quest_history.h"
#include "dungeon_building_children.h"
#include <memory>
namespace mt2mp::game::npc_probe::monster_action_runtime::dungeon_building {
struct Local;
struct Snapshot {action_session::DungeonBuildingState value;std::shared_ptr<Local> local;};
struct BuildingOwner {
    std::string key,scene;
    uintptr_t object=0,manager=0,shard=0,region=0;
    uint64_t uid=0;
    bool active=false;
    action_session::BuildingState state;
    duplicate_building_history::Saved saved;
    bool operator==(const BuildingOwner&)const=default;
};
// Prepare existing child aliases before any complete Quest reader. The output
// is a previous-owner seed, never a current action proof.
bool prepare_children(const action_session::Binding&,const std::string&,const action_session::Request&,Snapshot&,std::string&,const Snapshot* previous=nullptr);
bool prepare_history_seed(const action_session::Binding&,const std::string&,const Snapshot& consumed,Snapshot&,std::string&);
bool publish_children(const action_session::Binding&,const std::string&,const action_session::Request&,const Snapshot& before,
                      const Snapshot* consumed,const std::vector<uintptr_t>& inverse_operations,Snapshot& seed,std::string&);
bool capture(const action_session::Binding&,const std::string&,Snapshot&,std::string&,const Snapshot* previous=nullptr);
bool seed(const action_session::Binding&,const std::string&,const std::array<uint64_t,4>&,std::string&);
bool read_quests(const Snapshot&,monster_quest_retirement::Witness&,nlohmann::json& references,std::string&);
bool read_owners(const Snapshot&,dungeon_scene_identity_runtime::Census&,std::vector<BuildingOwner>&,std::string&);
bool building_inverse(const Snapshot&,uintptr_t node,const std::string& key,std::string&);
bool related_inverse(const Snapshot&,uintptr_t node,uintptr_t addresspoint,std::string&);
bool read_children(const Snapshot&,std::map<std::string,building_npc_lifecycle::Receipt>&,std::string&);
bool read_services(const Snapshot&,duplicate_service_history::World&,std::map<uint64_t,std::string>& keys,std::string&);
bool quest_seed(const Snapshot& current,const Snapshot& original,const std::vector<duplicate_quest_history::OwnerRebind>& restored,
                const std::set<uintptr_t>& retired,Snapshot&,std::string&);
}
namespace mt2mp::game::npc_probe {namespace dungeon_building=monster_action_runtime::dungeon_building;}

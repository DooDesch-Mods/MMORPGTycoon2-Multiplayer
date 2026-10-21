#pragma once
#include "character_boss_retirement.h"
#include "vendor/nlohmann/json.hpp"
#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace mt2mp::game::remove_diagnostics {
struct LocalTarget {
    uintptr_t manager=0,object=0;
    uint64_t uid=0,world_epoch=0,world_generation=0;
    std::string snapshot_id,record_key,type,variant;
    bool normal_building=false,normal_scenery=false;
    std::string kind_cause;
};
bool resolve_current_building_key(const std::string& snapshot,uintptr_t object,uint64_t full_uid,std::string& key,std::string& error);
bool resolve_current_building_identity(const std::string& snapshot,const std::string& key,uintptr_t& object,uint64_t& full_uid,std::string& error);
bool resolve_local_target(uintptr_t manager,uintptr_t object,LocalTarget&,std::string&);
bool resolve_record_target(const std::string& snapshot,const std::string& key,LocalTarget&,std::string&);
nlohmann::json status();
bool bindings(nlohmann::json&,std::string&);
bool identity_bindings(nlohmann::json&,std::string&);
bool party_identity_bindings(nlohmann::json&,std::string&);
bool party_identity_current(const std::string&,const std::string&,uint64_t,uintptr_t,uintptr_t,std::string&);
bool identity_targets(std::vector<LocalTarget>&,std::string&);
bool identity_building_targets(std::vector<LocalTarget>&,std::string&,std::map<uintptr_t,std::string>* destinations=nullptr);
bool resolve_identity_destination(uintptr_t,LocalTarget&,std::string&);
bool resolve_identity_destination(const std::string&,const std::string&,LocalTarget&,std::string&);
bool retire_region_scenery(const std::string& snapshot,const std::string& action_uuid,const std::vector<uint64_t>& removed_uids,std::vector<std::string>& retired_keys,std::string&);
bool admit_created_modular(const std::string&,const std::string&,uint64_t,std::string&);
bool admit_created_dungeon_building(const std::string&,const std::string&,uint64_t,const std::string& scene,std::string&);
bool restore_redone_modular(const std::string&,const std::string&,uint64_t,std::string&);
bool restore_redone_dungeon_building(const std::string&,const std::string&,uint64_t,const std::string& scene,std::string&);
bool is_modular_target(const LocalTarget&,std::string&);
bool dungeon_building_scene(const LocalTarget&,std::string& scene,bool& matched,std::string&);
bool is_dungeon_building_target(const LocalTarget&,const std::string& scene,std::string&);
bool admit_created_building(const std::string& snapshot,const std::string& shared_id,uint64_t local_uid,std::string&);
bool refresh_moved_building(const std::string& snapshot,const std::string& shared_id,uint64_t local_uid,std::string&,const std::string& dungeon_scene={});
bool retire_undone_building(const std::string& snapshot,const std::string& shared_id,uint64_t local_uid,std::string&);
bool restore_redone_building(const std::string& snapshot,const std::string& shared_id,uint64_t local_uid,std::string&);
bool history_target_absent(const std::string& snapshot,const std::string& key,uint64_t local_uid,std::string&);
bool confirmed_remove_graph(const std::string& token,const std::string& snapshot,const std::string& key,uint64_t local_uid,uintptr_t& graph,std::string&);
bool remove(const nlohmann::json&,nlohmann::json&,std::string&);
bool retire(const nlohmann::json&,nlohmann::json&,std::string&);
}

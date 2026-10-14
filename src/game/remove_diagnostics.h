#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace mt2mp::game::remove_diagnostics {
struct LocalTarget {
    uintptr_t manager=0,object=0;
    uint64_t uid=0,world_epoch=0,world_generation=0;
    std::string snapshot_id,record_key,type,variant;
    bool normal_building=false,normal_scenery=false;
    std::string kind_cause;
};
bool resolve_local_target(uintptr_t manager,uintptr_t object,LocalTarget&,std::string&);
bool resolve_record_target(const std::string& snapshot,const std::string& key,LocalTarget&,std::string&);
nlohmann::json status();
bool bindings(nlohmann::json&,std::string&);
bool retire_region_scenery(const std::string& snapshot,const std::string& action_uuid,const std::vector<uint64_t>& removed_uids,std::vector<std::string>& retired_keys,std::string&);
bool admit_created_building(const std::string& snapshot,const std::string& shared_id,uint64_t local_uid,std::string&);
bool refresh_moved_building(const std::string& snapshot,const std::string& shared_id,uint64_t local_uid,std::string&);
bool retire_undone_building(const std::string& snapshot,const std::string& shared_id,uint64_t local_uid,std::string&);
bool restore_redone_building(const std::string& snapshot,const std::string& shared_id,uint64_t local_uid,std::string&);
bool remove(const nlohmann::json&,nlohmann::json&,std::string&);
bool retire(const nlohmann::json&,nlohmann::json&,std::string&);
}

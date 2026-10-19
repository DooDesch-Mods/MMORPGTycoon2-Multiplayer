#pragma once
#include "remove_diagnostics.h"
#include <set>
namespace mt2mp::game::service_building_census {
template<class Resolve>
bool read(const nlohmann::json& census,Resolve&& resolve,std::vector<remove_diagnostics::LocalTarget>& out,std::string& e){
    out.clear();std::set<std::string> keys;std::set<uintptr_t> objects;std::set<uint64_t> uids;
    try{for(const auto& row:census.at("bindings")){
        if(row.at("unknown").template get<bool>()){e="Service census includes Unknown native binding";return false;}
        if(row.at("revoked").template get<bool>())continue;
        const auto key=row.at("record_key").template get<std::string>();const auto snapshot=row.at("snapshot_id").template get<std::string>();const auto uid=row.at("local_uid").template get<uint64_t>();
        remove_diagnostics::LocalTarget target;if(!resolve(row,target,e))return false;
        if(!target.object||!uid||target.uid!=uid||target.snapshot_id!=snapshot||target.record_key!=key||target.world_epoch!=census.at("world_epoch")||target.world_generation!=census.at("world_generation")||
           !keys.insert(key).second||!objects.insert(target.object).second||!uids.insert(uid).second){e="Service census exact key/full UID/current NativeOrigin or uniqueness differs";return false;}
        out.push_back(std::move(target));
    }}catch(const std::exception& x){e=std::string("Service census complete binding shape invalid: ")+x.what();return false;}
    e.clear();return true;
}
}

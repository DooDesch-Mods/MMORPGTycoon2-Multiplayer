#pragma once
#include "monster_zone_read.h"
#include "direct_monster_identity.h"
namespace mt2mp::game::monster_canonical {
// Keep Zone members and direct births in one roster without exposing native IDs.
template<class Name,class Verify>
bool normalize(const monster_zone_read::State& state,const std::map<uint64_t,std::string>& zones,
    const direct_monster_identity::Scope& scope,nlohmann::json& content,Name name,Verify verify,
    std::string& error,const std::string& pending={}) {
    std::map<uint64_t,std::string> keys;
    for(const auto& row:state.monsters){
        if(!verify(row,error))return false;
        if(!row.zone){std::string key;if(!direct_monster_identity::registry.resolve(scope,row.pointer,row.uid,key,error,pending))return false;
            keys.emplace(row.uid,std::move(key));continue;}
        const auto zone=std::find_if(state.zones.begin(),state.zones.end(),[&](const auto& v){return v.value.pointer==row.zone;});
        if(zone==state.zones.end()){error="Monster canonical non-null Zone lacks reciprocal owner";return false;}
        const auto member=std::find(zone->monsters.begin(),zone->monsters.end(),row.uid);
        const auto key=zones.find(zone->value.uid);
        if(member==zone->monsters.end()||key==zones.end()){error="Monster canonical Zone member/key absent";return false;}
        keys.emplace(row.uid,key->second+":monster:"+std::to_string(member-zone->monsters.begin()));
    }
    for(auto& row:content["zones"]){const auto uid=row.at("local_uid").template get<uint64_t>();const auto found=zones.find(uid);
        if(found==zones.end()){error="Monster canonical Zone key absent";return false;}row.erase("local_uid");row["key"]=found->second;
        nlohmann::json members=nlohmann::json::array();for(const auto& uid_value:row.at("monster_local_uids"))members.push_back(keys.at(uid_value.template get<uint64_t>()));
        row.erase("monster_local_uids");row["monsters"]=std::move(members);}
    for(size_t i=0;i<state.monsters.size();++i){const auto& actual=state.monsters[i];auto& row=content["monsters"][i];
        const auto zone=row.at("zone_local_uid").template get<uint64_t>();row.erase("local_uid");row.erase("zone_local_uid");row["key"]=keys.at(actual.uid);
        if(actual.zone)row["zone"]=zones.at(zone);
        else{std::string generated;if(!name(actual.pointer,generated,error))return false;row["zone"]=nullptr;row["name"]=std::move(generated);}}
    return true;
}
}

#pragma once
#include "monster_target_key.h"
#include "network_snapshot_alias.h"
#include "dungeon_gizmo_snapshot_alias.h"
#include "scenery_scene_snapshot_alias.h"
#include "party_snapshot_alias.h"
#include "boss_snapshot_alias.h"
#include "vendor/nlohmann/json.hpp"
#include <set>
#include <vector>

namespace mt2mp::game::monster_snapshot_companion {
inline constexpr size_t maximum_rows=4096,maximum_bytes=32*1024*1024;
struct Row {int32_t region=-1;std::string record,key;bool operator==(const Row&)const=default;};
struct Manifest {std::string native_sha256;uint64_t native_bytes=0;std::vector<Row> rows;network_snapshot_alias::Manifest network;dungeon_gizmo_snapshot_alias::Manifest dungeon_gizmos;party_snapshot_alias::Manifest parties;boss_snapshot_alias::Manifest bosses;scenery_scene_snapshot_alias::Manifest scene_scenery;bool operator==(const Manifest&)const=default;};
inline bool fail(std::string& e,const std::string& cause){e=cause;return false;}
inline bool sha(std::string_view value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string_view::npos;}
inline bool valid(const Manifest& m,std::string& e){
    if(!sha(m.native_sha256)||!m.native_bytes||m.native_bytes>1024ull*1024*1024||m.rows.size()>maximum_rows)
        return fail(e,"Monster companion native file identity/row budget invalid");
    std::set<std::string> keys;std::string previous;
    for(const auto& row:m.rows){
        if(row.region<0||!snapshot_monster_key::valid(row.record)||row.record.substr(0,64)!=m.native_sha256||
           !monster_target_key::valid(row.key)||(!previous.empty()&&row.record<=previous)||!keys.insert(row.key).second)
            return fail(e,"Monster companion record/key/Region duplicate, unsorted or invalid");
        previous=row.record;
    }
    if(!network_snapshot_alias::valid(m.network,e)||!dungeon_gizmo_snapshot_alias::valid(m.dungeon_gizmos,m.native_sha256,e)||!party_snapshot_alias::valid(m.parties,m.native_sha256,e)||!boss_snapshot_alias::valid(m.bosses,m.native_sha256,e)||!scenery_scene_snapshot_alias::valid(m.scene_scenery,m.native_sha256,e))return false;
    e.clear();return true;
}
inline bool encode(const Manifest& m,std::string& bytes,std::string& e){
    bytes.clear();if(!valid(m,e))return false;
    nlohmann::json rows=nlohmann::json::array();
    for(const auto& row:m.rows)rows.push_back({{"region",row.region},{"record",row.record},{"key",row.key}});
    auto value=nlohmann::json{{"revision",m.scene_scenery.root_present?7:m.scene_scenery.present?6:m.parties.present?5:m.bosses.present?4:m.dungeon_gizmos.present?3:m.network.present?2:1},{"native_sha256",m.native_sha256},{"native_bytes",m.native_bytes},{"rows",rows}};
    if(m.network.present)value["network"]=network_snapshot_alias::encode(m.network);
    if(m.dungeon_gizmos.present)value["dungeon_gizmos"]=dungeon_gizmo_snapshot_alias::encode(m.dungeon_gizmos);if(m.parties.present)value["parties"]=party_snapshot_alias::encode(m.parties);if(m.bosses.present)value["bosses"]=boss_snapshot_alias::encode(m.bosses);if(m.scene_scenery.present)value["scene_scenery"]=scenery_scene_snapshot_alias::encode(m.scene_scenery);auto text=value.dump();
    if(text.size()>maximum_bytes)return fail(e,"Monster companion serialized byte budget exceeded");
    bytes=std::move(text);return true;
}
inline bool decode(std::string_view bytes,const std::string& native_sha,uint64_t native_bytes,Manifest& out,std::string& e)try{
    out={};if(bytes.empty()||bytes.size()>maximum_bytes)return fail(e,"Monster companion received byte budget invalid");
    const auto j=nlohmann::json::parse(bytes);
    if(!j.is_object()||!j.at("revision").is_number_integer()||
       !((j.at("revision")==1&&j.size()==4)||(j.at("revision")==2&&j.size()==5&&j.contains("network"))||
         (j.at("revision")==3&&j.contains("dungeon_gizmos")&&j.size()==(j.contains("network")?6u:5u))||(j.at("revision")==4&&j.contains("bosses")&&j.size()==5u+size_t(j.contains("network"))+size_t(j.contains("dungeon_gizmos")))||(j.at("revision")==5&&j.contains("parties")&&j.size()==5u+size_t(j.contains("network"))+size_t(j.contains("dungeon_gizmos"))+size_t(j.contains("bosses")))||(j.at("revision")==6&&j.contains("scene_scenery")&&j.size()==5u+size_t(j.contains("network"))+size_t(j.contains("dungeon_gizmos"))+size_t(j.contains("bosses"))+size_t(j.contains("parties")))||(j.at("revision")==7&&j.contains("scene_scenery")&&j.size()==5u+size_t(j.contains("network"))+size_t(j.contains("dungeon_gizmos"))+size_t(j.contains("bosses"))+size_t(j.contains("parties"))))||!j.at("native_bytes").is_number_unsigned()||
       !j.at("rows").is_array()||j.at("rows").size()>maximum_rows)return fail(e,"Monster companion exact schema differs");
    Manifest m;m.native_sha256=j.at("native_sha256").get<std::string>();m.native_bytes=j.at("native_bytes").get<uint64_t>();
    if(m.native_sha256!=native_sha||m.native_bytes!=native_bytes)return fail(e,"Monster companion native file SHA/length differs");
    for(const auto& row:j.at("rows")){
        if(!row.is_object()||row.size()!=3||!row.at("region").is_number_integer()||
           row.at("region").get<int64_t>()<0||row.at("region").get<uint64_t>()>INT32_MAX)return fail(e,"Monster companion row schema/Region invalid");
        m.rows.push_back({row.at("region").get<int32_t>(),row.at("record").get<std::string>(),row.at("key").get<std::string>()});
    }
    if(j.contains("network")&&!network_snapshot_alias::decode(j.at("network"),m.network,e))return false;
    if(j.contains("dungeon_gizmos")&&!dungeon_gizmo_snapshot_alias::decode(j.at("dungeon_gizmos"),m.native_sha256,m.dungeon_gizmos,e))return false;
    if(j.contains("bosses")&&!boss_snapshot_alias::decode(j.at("bosses"),m.native_sha256,m.bosses,e))return false;
    if(j.contains("parties")&&!party_snapshot_alias::decode(j.at("parties"),m.native_sha256,m.parties,e))return false;
    if(j.contains("scene_scenery")&&!scenery_scene_snapshot_alias::decode(j.at("scene_scenery"),m.native_sha256,m.scene_scenery,e))return false;
    if((j.at("revision")==7)!=m.scene_scenery.root_present)return fail(e,"Root Scenery companion revision/census marker differs");
    if(!valid(m,e))return false;out=std::move(m);e.clear();return true;
}catch(const std::exception& ex){return fail(e,std::string("Monster companion decode failed: ")+ex.what());}
}

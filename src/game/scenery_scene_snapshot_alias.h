#pragma once
#include "scenery_scene_key.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::scenery_scene_snapshot_alias {
struct Manifest {
    bool present=false;std::vector<dungeon_gizmo_identity::Alias> rows;std::set<std::string> retired;bool root_present=false;
    bool operator==(const Manifest&)const=default;
};
inline bool fail(std::string& e,const char* cause){e=cause;return false;}
inline bool valid(const Manifest& m,const std::string& native_sha,std::string& e){
    if(!m.present)return (m.rows.empty()&&m.retired.empty()&&!m.root_present)||fail(e,"Absent Scene Scenery aliases contain records");
    if(!dungeon_gizmo_identity::digest(native_sha))return fail(e,"Scene Scenery alias native file SHA invalid");
    std::set<std::string> keys;std::string previous;
    for(const auto& row:m.rows){
        if((scenery_scene_key::root_saved(row.record)&&!m.root_present)||!scenery_scene_key::saved(row.record)||row.record.substr(0,64)!=native_sha||!scenery_scene_key::valid(row.key)||
           (!previous.empty()&&row.record<=previous)||!keys.insert(row.key).second||m.retired.contains(row.key))
            return fail(e,"Scene Scenery aliases are unsorted, duplicated, foreign or retired");previous=row.record;
    }
    for(const auto& id:m.retired)if(!scenery_scene_key::valid(id)||(scenery_scene_key::root_saved(id)&&!m.root_present))return fail(e,"Scene Scenery retired alias key invalid");
    e.clear();return true;
}
inline nlohmann::json encode(const Manifest& m){
    auto rows=nlohmann::json::array();for(const auto& row:m.rows)rows.push_back({{"record",row.record},{"key",row.key}});
    nlohmann::json out={{"rows",rows},{"retired",m.retired}};if(m.root_present)out["root"]=true;return out;
}
inline bool decode(const nlohmann::json& j,const std::string& native_sha,Manifest& out,std::string& e)try{
    out={};if(!j.is_object()||j.size()!=2+size_t(j.contains("root"))||!j.at("rows").is_array()||!j.at("retired").is_array())return fail(e,"Scene Scenery companion exact schema differs");
    Manifest m;m.present=true;if(j.contains("root")){if(!j.at("root").is_boolean()||!j.at("root").get<bool>())return fail(e,"Root Scenery complete census marker differs");m.root_present=true;}
    for(const auto& row:j.at("rows")){
        if(!row.is_object()||row.size()!=2||!row.at("record").is_string()||!row.at("key").is_string())return fail(e,"Scene Scenery companion row schema differs");
        m.rows.push_back({row.at("record").get<std::string>(),row.at("key").get<std::string>()});
    }
    for(const auto& id:j.at("retired"))if(!id.is_string()||!m.retired.insert(id.get<std::string>()).second)return fail(e,"Scene Scenery companion retired key duplicated or invalid");
    if(!valid(m,native_sha,e))return false;out=std::move(m);e.clear();return true;
}catch(const std::exception& exception){out={};e=std::string("Scene Scenery companion decode: ")+exception.what();return false;}
}

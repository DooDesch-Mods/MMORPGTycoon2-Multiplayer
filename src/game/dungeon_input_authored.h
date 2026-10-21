#pragma once
#include "dungeon_gizmo_portable.h"
#include "dungeon_scene_key.h"
#include "duplicate_owned_history.h"
#include "monster_target_key.h"
#include "network_object_identity.h"
#include <cmath>
#include <map>
#include <set>

namespace mt2mp::game::dungeon_input_authored {
using json=nlohmann::json;
struct Monster {
    std::string key;int32_t rank=-1,slot=-1,type=-1;
};
struct Scenery {
    int32_t category=-1;bool bounds_valid=false;
    std::array<float,3> minimum{},maximum{},position{},scale{};
    std::array<float,4> rotation{};
};
struct Scene {
    std::string key,dungeon,canvas_sha,design_sha;
    std::vector<action_session::DungeonGizmoRow> gizmos;
    std::vector<Monster> monsters;
    std::vector<std::pair<std::string,int32_t>> zones;
    std::array<int32_t,47> scenery_counts{};
    std::vector<Scenery> scenery;
};
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
// Canvas owns only its Shape, ordered Strokes and their Fill values. Any
// external SaveContext reference contradicts that exact original save graph.
inline bool canvas(const subscriber_lifecycle::Graph& graph,std::string& sha,std::string& error){
    return duplicate_owned_history::captured(graph,sha,error);
}
inline bool cell_digest(const std::string& canvas,const std::string& layout,std::string& output,std::string& error){
    output.clear();
    if(!network_object_identity::sha(canvas)||!network_object_identity::sha(layout))
        return fail(error,"Dungeon cell authored Canvas/layout digest is incomplete");
    try{const auto encoded=json::array({canvas,layout}).dump();
        output=mt2mp::sha256(std::vector<unsigned char>(encoded.begin(),encoded.end()));
    }catch(const std::exception& ex){return fail(error,std::string("Dungeon cell authored digest: ")+ex.what());}
    error.clear();return true;
}
inline bool digest(const std::vector<Scene>& scenes,std::string& canvases,std::string& members,std::string& error){
    canvases.clear();members.clear();json values=json::array(),paints=json::array();
    std::set<std::string> scene_keys,actor_keys;std::map<std::string,std::pair<std::string,std::string>> dungeon_canvas;
    for(const auto& scene:scenes){
        if(!dungeon_gizmo_identity::scene(scene.key)||!dungeon_scene_key::building_key(scene.dungeon)||
           !scene.key.starts_with(scene.dungeon+"/")||!scene_keys.insert(scene.key).second||
           !network_object_identity::sha(scene.canvas_sha)||!network_object_identity::sha(scene.design_sha))return fail(error,"Dungeon input authored scene/Canvas owner is invalid or repeated");
        const auto [known,inserted]=dungeon_canvas.emplace(scene.dungeon,std::pair{scene.canvas_sha,scene.design_sha});
        if(!inserted&&known->second!=std::pair{scene.canvas_sha,scene.design_sha})return fail(error,"Dungeon input shared original Canvas differs between its scene owners");
        json gizmos=json::array(),monsters=json::array(),zones=json::array(),scenery=json::array();
        for(const auto& row:scene.gizmos){
            if(!dungeon_gizmo_identity::key(row.key)||!actor_keys.insert(row.key).second)
                return fail(error,"Dungeon input authored Gizmo canonical membership is invalid or repeated");
            gizmos.push_back(dungeon_gizmo_portable::row(row));
        }
        for(const auto& row:scene.monsters){
            if(!monster_target_key::valid(row.key)||!actor_keys.insert(row.key).second||row.rank<0||row.rank>2||row.slot!=1||row.type<0)
                return fail(error,"Dungeon input authored Monster canonical membership is invalid or repeated");
            // Current motion, level, AI and private tasks travel through Actor24.
            // This action protects the original Character and roster ownership.
            monsters.push_back(json::array({row.key,row.rank,row.slot,row.type}));
        }
        for(const auto& [key,type]:scene.zones){
            if(key.empty()||type<0||type>3||!actor_keys.insert(key).second)
                return fail(error,"Dungeon input authored Zone canonical membership is invalid or repeated");
            zones.push_back(json::array({key,type}));
        }
        size_t total=0;std::array<int32_t,47> counts{};
        for(const auto count:scene.scenery_counts){if(count<0||size_t(count)>1000000-total)return fail(error,"Dungeon input authored Scenery counts exceed their complete bound");total+=size_t(count);}
        if(total!=scene.scenery.size())return fail(error,"Dungeon input authored Scenery membership/count differs");
        for(const auto& row:scene.scenery){
            if(row.category<0||row.category>=47)return fail(error,"Dungeon input authored Scenery category differs");
            ++counts[size_t(row.category)];
            for(const auto* vector:{&row.minimum,&row.maximum,&row.position,&row.scale})for(const auto value:*vector)
                if(!std::isfinite(value))return fail(error,"Dungeon input authored Scenery value is not finite");
            for(const auto value:row.rotation)if(!std::isfinite(value))return fail(error,"Dungeon input authored Scenery rotation is not finite");
            scenery.push_back(json::array({row.category,row.bounds_valid,row.minimum,row.maximum,row.position,row.rotation,row.scale}));
        }
        if(counts!=scene.scenery_counts)return fail(error,"Dungeon input authored Scenery categories/count differ");
        values.push_back(json::array({scene.key,scene.design_sha,std::move(gizmos),std::move(monsters),std::move(zones),scene.scenery_counts,std::move(scenery)}));
    }
    for(const auto& [key,sha]:dungeon_canvas)paints.push_back(json::array({key,sha.first}));
    try{
        const auto paint_text=paints.dump(),member_text=values.dump();
        canvases=mt2mp::sha256(std::vector<unsigned char>(paint_text.begin(),paint_text.end()));
        members=mt2mp::sha256(std::vector<unsigned char>(member_text.begin(),member_text.end()));
    }catch(const std::exception& ex){return fail(error,std::string("Dungeon input authored digest: ")+ex.what());}
    error.clear();return true;
}
}

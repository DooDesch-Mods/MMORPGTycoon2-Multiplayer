#pragma once
#include "duplicate_records.h"
#include "dungeon_gizmo_identity.h"
#include <functional>
namespace mt2mp::game::dungeon_scene_records {
struct Scene {
    std::string key,dungeon_key;uintptr_t region=0,shard=0,dungeon=0;
    subscriber_lifecycle::Graph graph;
};
using External=std::function<bool(uintptr_t,std::string&,std::string&)>;
struct Dungeon {std::string key;uintptr_t object=0;subscriber_lifecycle::Graph graph;};
struct Witness {std::string sha;std::map<std::string,std::string> dungeons;size_t scenes=0,records=0,tokens=0,links=0;};
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
// These paths normalize references inside the complete current content digest.
// They are not stable actor identities and must never target an action. All
// original tokens and SaveContext references remain represented in the digest.
inline bool summarize(const std::vector<Scene>& scenes,const std::vector<Dungeon>& dungeons,const External& external,Witness& out,std::string& error){
    out={};std::map<uintptr_t,std::string> owners;std::set<std::string> keys;
    const auto bind=[&](uintptr_t pointer,const std::string& key){
        if(!pointer||key.empty())return fail(error,"Dungeon complete Record canonical owner is absent");
        const auto found=owners.find(pointer);
        if(found!=owners.end())return found->second==key||fail(error,"Dungeon complete Record pointer has two canonical owners");
        owners.emplace(pointer,key);return true;
    };
    for(const auto& scene:scenes){
        if(!dungeon_gizmo_identity::scene(scene.key)||!dungeon_scene_key::building_key(scene.dungeon_key)||!keys.insert(scene.key).second)
            return fail(error,"Dungeon complete Record scene or canonical Dungeon key is invalid or repeated");
        if(!bind(scene.region,scene.key+"/region")||!bind(scene.shard,scene.key+"/shard")||!bind(scene.dungeon,scene.dungeon_key+"/dungeon"))return false;
    }
    for(const auto& dungeon:dungeons){if(!dungeon_scene_key::building_key(dungeon.key))return fail(error,"Dungeon complete Record canonical owner key is invalid");
        if(!bind(dungeon.object,dungeon.key+"/dungeon"))return false;}
    const auto definitions=[&](const auto& graph,uintptr_t root,const std::string& key){
        duplicate_records::Witness validated;
        const auto placeholder=[](uintptr_t,std::string& result,std::string&){result="current-external-reference";return true;};
        if(!duplicate_records::summarize(graph,root,placeholder,validated,error))return false;
        std::map<int32_t,size_t> inline_records;
        for(const auto& record:graph.records)if((record.label.type==1||record.label.type==2)&&record.label.text=="linkId")
            inline_records.emplace(int32_t(record.tokens[0].bits),record.parent);
        for(const auto& link:graph.private_links)if(link.has_inline_definition&&!owners.contains(link.object)){
            const auto found=inline_records.find(link.saved_link_id);
            if(found==inline_records.end())return fail(error,"Dungeon current inline reference has no original complete record");
            if(!bind(link.object,key+"/record/"+std::to_string(found->second)))return false;
        }return true;
    };
    for(const auto& scene:scenes){
        if(!definitions(scene.graph,scene.region,scene.key))return false;
    }
    for(const auto& dungeon:dungeons)if(!definitions(dungeon.graph,dungeon.object,dungeon.key+"/dungeon"))return false;
    nlohmann::json rows=nlohmann::json::array();Witness next;
    const auto canonical=[&](uintptr_t pointer,std::string& key,std::string& cause){
        const auto found=owners.find(pointer);if(found!=owners.end()){key=found->second;return true;}
        if(!external)return fail(cause,"Dungeon complete Record external owner has no current canonical resolver");
        if(!external(pointer,key,cause)){if(cause.empty())cause="Dungeon complete Record external resolver failed without cause";return false;}
        return true;
    };
    for(const auto& scene:scenes){
        duplicate_records::Witness value;
        if(!duplicate_records::summarize(scene.graph,scene.region,canonical,value,error))return false;
        rows.push_back(nlohmann::json::array({scene.key,value.sha}));++next.scenes;next.records+=value.records;next.tokens+=value.tokens;next.links+=value.links;
    }
    for(const auto& dungeon:dungeons){duplicate_records::Witness value;
        if(!duplicate_records::summarize(dungeon.graph,dungeon.object,canonical,value,error))return false;
        if(!next.dungeons.emplace(dungeon.key,value.sha).second)return fail(error,"Dungeon complete Record canonical owner repeats");
    }
    try{const auto text=rows.dump();next.sha=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));}
    catch(const std::exception& ex){return fail(error,std::string("Dungeon complete Record digest failed: ")+ex.what());}
    out=std::move(next);error.clear();return true;
}
}

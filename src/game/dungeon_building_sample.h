#pragma once
#include "typed_action/dungeon_building_action.h"
#include "dungeon_input_authored.h"
#include "duplicate_history_effects.h"
#include <memory>
namespace mt2mp::game::dungeon_building_sample {
inline bool quests(const monster_quest_retirement::Witness& value,std::string& sha,std::string& error){
    sha.clear();if(!duplicate_quest_history::aggregate(value,error))return false;
    nlohmann::json global=nlohmann::json::array();std::map<std::string,nlohmann::json> owners;
    std::map<uintptr_t,std::pair<std::string,size_t>> current;
    if(value.rows.size()!=value.quests.size()||value.links.size()!=value.quests.size()){
        error="Dungeon Building complete global Quest witness count differs";return false;}
    for(const auto& [pointer,owner]:value.owners){nlohmann::json rows=nlohmann::json::array();
        if(owner.pointer!=pointer||!owner.uid||owner.key.empty()||owner.state.records.size()!=owner.state.local.size()){
            error="Dungeon Building complete Quest owner/member identity differs";return false;}
        for(size_t i=0;i<owner.state.records.size();++i){const auto& local=owner.state.local[i];
            if(local.owner!=pointer||!local.quest||owner.state.records[i].sequence_id!=int32_t(i)||
               !current.emplace(local.quest,std::pair{owner.key,i}).second){error="Dungeon Building Quest owner/order repeats or differs";return false;}
            // Private native task cleanup is retained in the local Witness.
            // Authored admission includes every current weak membership flag.
            rows.push_back(nlohmann::json::array({duplicate_history_effects::quest(owner.state.records[i]),
                local.npc_list_member,local.owner_weak_confirmed,local.destination_weak_confirmed,local.manager_member}));
        }
        if(!owners.emplace(owner.key,std::move(rows)).second){error="Dungeon Building Quest canonical owner repeats";return false;}
    }
    std::set<uintptr_t> seen;
    for(size_t i=0;i<value.quests.size();++i){const auto found=current.find(value.quests[i]);
        if(found==current.end()||!seen.insert(value.quests[i]).second||found->second.first!=value.rows[i].owner_key){
            error="Dungeon Building global Quest canonical order differs";return false;}
        const auto owner=value.owners.find(value.links[i][3]);
        if(owner==value.owners.end()||owner->second.key!=found->second.first||
           owner->second.state.records[found->second.second]!=value.rows[i].record){error="Dungeon Building global Quest record differs from current owner";return false;}
        global.push_back(nlohmann::json::array({found->second.first,found->second.second,duplicate_history_effects::quest(value.rows[i].record)}));
    }
    if(seen.size()!=current.size()){error="Dungeon Building owned Quest absent from complete global witness";return false;}
    nlohmann::json rows=nlohmann::json::array();for(auto& [key,records]:owners)rows.push_back(nlohmann::json::array({key,std::move(records)}));
    return duplicate_history_effects::digest(nlohmann::json::array({value.completed,std::move(global),std::move(rows)}),sha,error);
}
struct Scene {dungeon_input_authored::Scene authored;std::string zones,buildings;};
inline bool compose(const std::vector<Scene>& scenes,const std::map<std::string,std::string>& config,
                    std::map<std::string,std::string>& output,std::string& error){
    output.clear();std::map<std::string,std::vector<dungeon_input_authored::Scene>> groups;
    std::map<std::string,nlohmann::json> extras;std::set<std::string> keys,templates;
    for(const auto& scene:scenes){const auto& row=scene.authored;
        if(!network_object_identity::sha(scene.zones)||!network_object_identity::sha(scene.buildings)||
           !keys.insert(row.key).second){error="Dungeon Building authored scene value proof is missing or repeated";return false;}
        groups[row.dungeon].push_back(row);
        if(row.key==row.dungeon+"/template")templates.insert(row.dungeon);
        auto [found,inserted]=extras.try_emplace(row.dungeon,nlohmann::json::array());(void)inserted;
        found->second.push_back(nlohmann::json::array({row.key,scene.zones,scene.buildings}));
    }
    if(groups.size()!=config.size()||templates.size()!=groups.size()){
        error="Dungeon Building authored exterior/config/template owner set differs";return false;}
    for(const auto& [key,rows]:groups){const auto settings=config.find(key);
        if(settings==config.end()||!network_object_identity::sha(settings->second)){
            error="Dungeon Building original complete settings proof absent";return false;}
        std::string canvas,members;
        if(!dungeon_input_authored::digest(rows,canvas,members,error))return false;
        try{const auto text=nlohmann::json::array({canvas,members,settings->second,extras.at(key)}).dump();
            output.emplace(key,mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end())));
        }catch(const std::exception& ex){error=std::string("Dungeon Building authored value serialization: ")+ex.what();output.clear();return false;}
    }
    error.clear();return true;
}
// Both reads retain actual values. Only the authored projection is compared;
// unrelated Actor motion continues through the authoritative Actor frame.
template<class Port,class Snapshot>bool capture(Port& port,Snapshot& out,std::string& error,const Snapshot* previous){
    out={};auto first=std::make_unique<Snapshot>(),last=std::make_unique<Snapshot>();
    if(!port.read(*first,error,previous)||!port.read(*last,error,first.get())||!port.same(*first,*last,error))return false;
    out=std::move(*last);return true;
}
template<class Port>bool seed(Port& port,const std::array<uint64_t,4>& desired,std::string& error){
    if(!port.before(error))return false;
    if(!port.write(desired,error))return false;
    return port.after(desired,error);
}
}

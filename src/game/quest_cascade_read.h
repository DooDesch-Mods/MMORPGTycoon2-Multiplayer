#pragma once
#include "monster_quest_retirement.h"
#include "vendor/nlohmann/json.hpp"
#include <algorithm>

namespace mt2mp::game::quest_cascade_read {
namespace qr=monster_quest_retirement;
using Destinations=std::map<std::string,uint64_t>;

// Destination keys and full UIDs come from the same bound pre-original census.
// No receiver is read after the original operation destroys its lifetime.
inline bool select(qr::Witness& before,const Destinations& destinations,std::string& error){
    if(before.quests.size()!=before.rows.size()||before.quests.size()!=before.links.size())
        return qr::fail(error,"Quest cascade global witness shape differs");
    std::set<uint64_t> uids;
    for(const auto& [key,uid]:destinations)
        if(key.empty()||!uid||!uids.insert(uid).second)
            return qr::fail(error,"Quest cascade destination key/full UID is empty or aliased");
    std::vector<uintptr_t> removed;std::set<uintptr_t> quests;
    for(size_t i=0;i<before.quests.size();++i){
        const auto quest=before.quests[i];const auto& link=before.links[i];
        const auto owner=before.owners.find(link[3]);
        if(!quest||!quests.insert(quest).second||owner==before.owners.end()||owner->second.key!=before.rows[i].owner_key)
            return qr::fail(error,"Quest cascade global member has a missing or foreign owner");
        const auto& local=owner->second.state.local;
        const auto found=std::find_if(local.begin(),local.end(),[&](const auto& row){return row.quest==quest;});
        if(found==local.end())return qr::fail(error,"Quest cascade global member is absent from its owner");
        const auto slot=size_t(found-local.begin());
        if(owner->second.state.records.size()!=local.size()||owner->second.destination_uids.size()!=local.size()||
           owner->second.state.records[slot]!=before.rows[i].record||found->owner!=link[3]||found->destination!=link[0])
            return qr::fail(error,"Quest cascade owner record, local destination or full UID witness differs");
        const auto target=destinations.find(before.rows[i].record.destination_key);
        if(target==destinations.end())continue;
        if(owner->second.destination_uids[slot]!=target->second)
            return qr::fail(error,"Quest cascade selected canonical destination has a different full UID");
        removed.push_back(quest);
    }
    before.removed=std::move(removed);error.clear();return true;
}

// Original Quest destruction expires progress weak links and renumbers the
// surviving owner slots. Rewards, completion counts and other links stay exact.
inline bool references(const qr::Witness& before,const nlohmann::json& old_rows,
                       const nlohmann::json& new_rows,std::string& error)try{
    using json=nlohmann::json;
    if(!old_rows.is_array()||!new_rows.is_array()||old_rows.size()!=new_rows.size())
        return qr::fail(error,"Quest cascade Subscriber cohort differs");
    std::map<std::string,std::string> mapping;std::set<uintptr_t> removed(before.removed.begin(),before.removed.end());
    if(removed.size()!=before.removed.size())return qr::fail(error,"Quest cascade repeats a removed Quest");
    size_t matched=0;
    for(const auto& [pointer,owner]:before.owners){(void)pointer;
        if(owner.key.empty()||owner.state.records.size()!=owner.state.local.size())
            return qr::fail(error,"Quest cascade active-reference owner shape differs");
        size_t next=0;
        for(size_t slot=0;slot<owner.state.local.size();++slot){
            const auto key=owner.key+"/quest/"+std::to_string(slot);
            const bool retired=removed.contains(owner.state.local[slot].quest);
            if(retired)++matched;
            const auto target=retired?std::string{}:owner.key+"/quest/"+std::to_string(next++);
            if(!mapping.emplace(key,target).second)return qr::fail(error,"Quest cascade repeats a canonical owner/current slot");
        }
    }
    if(matched!=removed.size())return qr::fail(error,"Quest cascade removed Quest lacks its exact owner slot");
    auto expected=old_rows;
    for(size_t i=0;i<expected.size();++i){auto& row=expected.at(i);const auto& actual=new_rows.at(i);
        if(!row.is_object()||!actual.is_object()||!row.contains("progress")||!actual.contains("progress")||
           !row.at("progress").is_array()||!actual.at("progress").is_array()||
           row.at("progress").size()!=actual.at("progress").size())
            return qr::fail(error,"Quest cascade active progress shape or count differs");
        for(size_t j=0;j<row.at("progress").size();++j){auto& progress=row.at("progress").at(j);
            auto& link=progress.at("quest");
            if(!link.is_object()||!link.at("state").is_number_integer()||!link.at("key").is_string())
                return qr::fail(error,"Quest cascade active Quest weak link shape differs");
            if(link.at("state")!=2)continue;
            const auto found=mapping.find(link.at("key").get<std::string>());
            if(found==mapping.end())return qr::fail(error,"Quest cascade Live progress lacks a current canonical Quest");
            if(!found->second.empty()){link["key"]=found->second;continue;}
            const auto& expired=actual.at("progress").at(j).at("quest");
            if(!expired.is_object()||!expired.at("state").is_number_integer()||
               (expired.at("state")!=0&&expired.at("state")!=1)||expired.at("key")!="")
                return qr::fail(error,"Quest cascade retained a Live retired Quest reference");
            link=expired;progress["complete"]=false;
        }
    }
    return expected==new_rows||qr::fail(error,"Quest cascade changed unrelated progress, rewards, NPC references or current-slot order");
}catch(const nlohmann::json::exception& exception){
    return qr::fail(error,std::string("Quest cascade active-reference payload: ")+exception.what());
}
}

#pragma once
#include "duplicate_service_history.h"
#include "duplicate_quest_history.h"
#include "duplicate_transition.h"
#include "mt2mp/core/hash.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::duplicate_history_effects {
using json=nlohmann::json;
inline bool digest(const json& value,std::string& out,std::string& e){
    out.clear();try{const auto bytes=value.dump();out=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));}
    catch(const std::exception& ex){e=std::string("Duplicate Building history witness serialization failed: ")+ex.what();return false;}
    return true;
}
inline json daily(const duplicate_service_history::Daily& value){return json::array({value.entries,value.history,value.words,value.warning});}
inline bool services(const duplicate_service_history::World& saved,const std::map<uint64_t,action_session::ServiceMembershipState>& values,
                     const std::map<uint64_t,std::string>& keys,std::string& sha,std::string& e){
    if(saved.size()!=values.size()||saved.size()!=keys.size())return npc_quest_targets::fail(e,"Duplicate complete Service witness owner set differs");
    std::map<std::string,json> ordered;
    for(const auto& [owner,rows]:saved){const auto state=values.find(owner);const auto key=keys.find(owner);
        if(state==values.end()||key==keys.end()||key->second.empty()||rows.size()!=state->second.rows.size())
            return npc_quest_targets::fail(e,"Duplicate complete Service witness key/member count differs");
        const auto& current=state->second;json members=json::array(),characters=json::array(),configs=json::array();
        for(size_t i=0;i<rows.size();++i){const auto& original=rows[i];const auto& member=current.rows[i];
            if(original.owner_uid!=owner||original.type!=member.type)return npc_quest_targets::fail(e,"Duplicate complete Service witness owner/type differs");
            members.push_back(json::array({member.type,member.character,member.index,member.destination,member.config_key,member.price,
                member.flags,member.prototype,member.config,original.price,original.flags,daily(original.counts[0]),daily(original.counts[1])}));
        }
        for(const auto& character:current.characters)characters.push_back(json::array({character.id,character.name,character.deleted}));
        for(const auto& config:current.configs)configs.push_back(json::array({config.key,config.price}));
        if(!ordered.emplace(key->second,json::array({current.building_definition,current.spawn_type,std::move(members),std::move(characters),
            current.config_present,std::move(configs)})).second)return npc_quest_targets::fail(e,"Duplicate complete Service canonical Building key repeats");
    }
    json result=json::array();for(auto& [key,value]:ordered)result.push_back(json::array({key,std::move(value)}));
    return digest(result,sha,e);
}
inline json quest(const npc_quest_records::Record& value){
    return json::array({value.sequence_id,value.type,value.name,value.description,value.action,value.monsters_to_kill,value.monster_type,
        value.xp_reward,value.gold_reward,value.destination_key,
        json::array({value.accepted.history,value.accepted.today,value.accepted.total}),
        json::array({value.completed.history,value.completed.today,value.completed.total}),
        json::array({std::bit_cast<uint32_t>(value.rating.beauty),std::bit_cast<uint32_t>(value.rating.convenience),
            std::bit_cast<uint32_t>(value.rating.danger),value.rating.set})});
}
inline bool quests(const monster_quest_retirement::Witness& value,std::string& sha,std::string& e){
    if(!duplicate_quest_history::aggregate(value,e))return false;
    if(value.rows.size()!=value.quests.size()||value.links.size()!=value.quests.size())
        return npc_quest_targets::fail(e,"Duplicate complete Quest global witness count differs");
    json global=json::array();std::map<std::string,json> owners;std::map<uintptr_t,std::pair<std::string,size_t>> references;
    for(const auto& [pointer,owner]:value.owners){if(owner.pointer!=pointer||!owner.uid||owner.key.empty()||owner.state.records.size()!=owner.state.local.size())
            return npc_quest_targets::fail(e,"Duplicate complete Quest canonical owner/member count differs");
        json records=json::array();
        for(size_t i=0;i<owner.state.records.size();++i){const auto& local=owner.state.local[i];
            if(local.owner!=pointer||!local.quest||owner.state.records[i].sequence_id!=int32_t(i)||
               !references.emplace(local.quest,std::pair{owner.key,i}).second)
                return npc_quest_targets::fail(e,"Duplicate complete Quest current owner/sequence identity differs");
            records.push_back(json::array({quest(owner.state.records[i]),uint8_t(local.task),local.npc_list_member,
                local.owner_weak_confirmed,local.destination_weak_confirmed,local.manager_member}));
        }
        if(!owners.emplace(owner.key,std::move(records)).second)return npc_quest_targets::fail(e,"Duplicate complete Quest canonical owner key repeats");
    }
    std::set<uintptr_t> seen;
    for(size_t i=0;i<value.quests.size();++i){const auto found=references.find(value.quests[i]);
        if(found==references.end()||!seen.insert(value.quests[i]).second||found->second.first!=value.rows[i].owner_key)
            return npc_quest_targets::fail(e,"Duplicate complete Quest global order/canonical owner differs");
        const auto owner=value.owners.find(value.links[i][3]);
        if(owner==value.owners.end()||owner->second.key!=found->second.first||found->second.second>=owner->second.state.records.size()||
           owner->second.state.records[found->second.second]!=value.rows[i].record)
            return npc_quest_targets::fail(e,"Duplicate complete Quest global record differs from its current owner list");
        global.push_back(json::array({found->second.first,found->second.second,quest(value.rows[i].record)}));
    }
    if(seen.size()!=references.size())return npc_quest_targets::fail(e,"Duplicate complete Quest owner member absent from global witness");
    json records=json::array();for(auto& [key,rows]:owners)records.push_back(json::array({key,std::move(rows)}));
    return digest(json::array({value.completed,std::move(global),std::move(records)}),sha,e);
}
inline bool prop_records(const std::map<std::string,action_session::DuplicateObject>& objects,const action_session::DuplicateState& current,
                         const std::set<std::string>& related,std::string& related_sha,std::string& untouched_sha,std::string& e){
    std::set<std::string> excluded;for(const auto& row:current.sources)excluded.insert(row.key);
    for(const auto& row:current.created)excluded.insert(row.key);
    json changed=json::array(),unchanged=json::array();
    for(const auto& key:related){const auto found=objects.find(key);
        if(found==objects.end()||found->second.family!=action_session::DuplicateFamily::Building||
           std::any_of(current.created.begin(),current.created.end(),[&](const auto& row){return row.key==key;}))
            return npc_quest_targets::fail(e,"Duplicate related Service owner is not a surviving canonical Building");
        changed.push_back(json::array({key,found->second.record_sha}));excluded.insert(key);
    }
    for(const auto& [key,row]:objects)if(!excluded.contains(key))unchanged.push_back(json::array({key,row.record_sha}));
    return digest(changed,related_sha,e)&&digest(unchanged,untouched_sha,e);
}
struct OwnedChild {
    std::string parent_key,child_key,record_sha;
    bool present=false;
};
inline bool owned_children(const std::string& related_props_sha,const std::vector<OwnedChild>& children,
                           std::string& sha,std::string& e){
    if(!duplicate_transition::sha(related_props_sha))
        return npc_quest_targets::fail(e,"Duplicate related prop witness hash is invalid");
    std::map<std::string,json> ordered;std::set<std::string> keys;
    for(const auto& child:children){
        if(child.parent_key.empty()||
           (child.present&&(child.child_key.empty()||!duplicate_transition::sha(child.record_sha)||
                            child.child_key==child.parent_key||!keys.insert(child.child_key).second))||
           (!child.present&&(!child.child_key.empty()||!child.record_sha.empty())))
            return npc_quest_targets::fail(e,"Duplicate owned child canonical identity/complete record differs");
        if(!ordered.emplace(child.parent_key,json::array({child.present,child.child_key,child.record_sha})).second)
            return npc_quest_targets::fail(e,"Duplicate owned child canonical Building key repeats");
    }
    json records=json::array();for(auto& [key,row]:ordered)records.push_back(json::array({key,std::move(row)}));
    return digest(json::array({related_props_sha,std::move(records)}),sha,e);
}
}

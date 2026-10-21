#pragma once
#include "duplicate_quest_history.h"
#include "quest_cascade_read.h"

namespace mt2mp::game::zone_quest_history {
namespace qr=monster_quest_retirement;
namespace dh=duplicate_quest_history;
namespace qc=quest_cascade_read;
enum class Kind {Destroy,Make,EditZone,Quests};
// Entries are supplied in the consumed, verified native graph's execution
// order. The receiver is a local pre-original witness, never a wire identity.
struct Step {Kind kind=Kind::Destroy;std::string key;uint64_t uid=0;uintptr_t receiver=0;};
namespace detail {
inline bool shape(const qr::Witness& value,std::string& error){
    if(!value.manager||value.quests.size()!=value.rows.size()||value.quests.size()!=value.links.size())
        return qr::fail(error,"Zone history complete Quest witness shape differs");
    auto checked=value;
    if(!qc::select(checked,{},error)||!dh::aggregate(value,error))return false;
    std::set<uintptr_t> members;std::set<std::string> keys;
    for(const auto& [pointer,owner]:value.owners){
        if(!pointer||pointer!=owner.pointer||!owner.uid||owner.key.empty()||!keys.insert(owner.key).second||
           owner.state.records.size()!=owner.state.local.size()||owner.destination_uids.size()!=owner.state.local.size())
            return qr::fail(error,"Zone history complete Quest owner shape or identity differs");
        for(size_t slot=0;slot<owner.state.local.size();++slot){const auto& local=owner.state.local[slot];
            const auto found=std::find(value.quests.begin(),value.quests.end(),local.quest);
            if(!local.quest||!members.insert(local.quest).second||found==value.quests.end()||
               local.owner!=pointer||!local.destination||!owner.destination_uids[slot]||
               !local.npc_list_member||!local.owner_weak_confirmed||!local.destination_weak_confirmed||!local.manager_member||
               owner.state.records[slot].sequence_id!=int32_t(slot))
                return qr::fail(error,"Zone history Quest owner membership, full UID or weak reference differs");
            const auto index=size_t(found-value.quests.begin());const auto& link=value.links[index];
            if(link[0]!=local.destination||link[3]!=pointer||!link[1]||!link[4]||
               link[2]!=local.destination+0x20||link[5]!=pointer+0x20)
                return qr::fail(error,"Zone history Quest complete weak link differs");
        }
    }
    return members.size()==value.quests.size()||qr::fail(error,"Zone history Quest global owner coverage differs");
}
inline bool same_local(const npc_quest_records::Local& a,const npc_quest_records::Local& b){
    return a.quest==b.quest&&a.owner==b.owner&&a.destination==b.destination&&a.task==b.task&&
        a.npc_list_member==b.npc_list_member&&a.owner_weak_confirmed==b.owner_weak_confirmed&&
        a.destination_weak_confirmed==b.destination_weak_confirmed&&a.manager_member==b.manager_member;
}
struct Token {uintptr_t owner=0,original=0;uint64_t destination_uid=0;npc_quest_records::Record record;};
}

// This proves membership after the existing original graph. It never creates
// engine records or reconnects expired Subscriber weak links. SetNPCQuests
// computes new runtime values; the caller still compares its full result with
// the Host and verifies every retired original Quest absent from the manager.
inline bool verify(const qr::Witness& stored,const qr::Witness& before,const qr::Witness& after,
                   const std::vector<Step>& execution,const nlohmann::json& references_before,
                   const nlohmann::json& references_after,qr::Witness& retired,std::string& error){
    retired={};
    if(!detail::shape(stored,error)||!detail::shape(before,error)||!detail::shape(after,error))return false;
    if(stored.manager!=before.manager||before.manager!=after.manager||before.owners.size()!=after.owners.size())
        return qr::fail(error,"Zone history complete seeded Quest owners or manager differ");
    for(const auto& [pointer,owner]:stored.owners){const auto current=before.owners.find(pointer);
        if(current==before.owners.end()||current->second.uid!=owner.uid||current->second.key!=owner.key)
            return qr::fail(error,"Zone history stored Quest owner lacks its seeded current lifetime");}
    std::map<std::string,uintptr_t> owner_keys;
    for(const auto& [pointer,owner]:before.owners){
        const auto current=after.owners.find(pointer);
        if(current==after.owners.end()||current->second.uid!=owner.uid||current->second.key!=owner.key||!owner_keys.emplace(owner.key,pointer).second)
            return qr::fail(error,"Zone history original Quest owner lifetime/key/full UID changed");
    }
    std::vector<detail::Token> tokens;std::vector<size_t> global;
    std::map<uintptr_t,std::vector<size_t>> owners;
    for(const auto& [pointer,owner]:before.owners){auto& list=owners[pointer];
        for(size_t slot=0;slot<owner.state.local.size();++slot){list.push_back(tokens.size());
            tokens.push_back({pointer,owner.state.local[slot].quest,owner.destination_uids[slot],owner.state.records[slot]});}}
    for(auto quest:before.quests){const auto found=std::find_if(tokens.begin(),tokens.end(),[&](const auto& row){return row.original==quest;});
        if(found==tokens.end())return qr::fail(error,"Zone history initial global Quest has no exact owner token");
        global.push_back(size_t(found-tokens.begin()));}
    std::vector<uintptr_t> removed;
    const auto erase=[&](size_t token){
        const auto found=std::find(global.begin(),global.end(),token);
        if(found==global.end())return qr::fail(error,"Zone history original Quest token retired twice");
        *found=global.back();global.pop_back();
        auto& list=owners.at(tokens[token].owner);const auto local=std::find(list.begin(),list.end(),token);
        if(local==list.end())return qr::fail(error,"Zone history original owner lacks retiring Quest token");
        list.erase(local);if(tokens[token].original)removed.push_back(tokens[token].original);
        return true;
    };
    for(const auto& step:execution){
        if(step.key.empty()||!step.uid)return qr::fail(error,"Zone history execution lacks canonical key/full UID");
        switch(step.kind){
        case Kind::Make:case Kind::EditZone:break;
        case Kind::Destroy:{
            std::vector<size_t> matching;
            for(auto token:global){const auto& value=tokens[token];if(value.record.destination_key!=step.key)continue;
                if(value.destination_uid!=step.uid)return qr::fail(error,"Zone history Destroy canonical destination full UID differs");
                if(value.original){const auto index=size_t(std::find(before.quests.begin(),before.quests.end(),value.original)-before.quests.begin());
                    if(!step.receiver||before.links[index][0]!=step.receiver)
                        return qr::fail(error,"Zone history Destroy original destination receiver differs");}
                matching.push_back(token);}
            for(auto token:matching)if(!erase(token))return false;
            break;}
        case Kind::Quests:{
            const auto key=owner_keys.find(step.key);
            if(key==owner_keys.end()||key->second!=step.receiver||before.owners.at(key->second).uid!=step.uid||!stored.owners.contains(key->second))
                return qr::fail(error,"Zone history SetNPCQuests original owner receipt differs");
            const auto pointer=key->second;const auto previous=owners.at(pointer);
            for(auto token:previous)if(!erase(token))return false;
            const auto& source=stored.owners.at(pointer);
            for(size_t slot=0;slot<source.state.records.size();++slot){const auto& record=source.state.records[slot];
                if(record.action==8)continue;
                const auto token=tokens.size();tokens.push_back({pointer,0,source.destination_uids[slot],record});
                owners.at(pointer).push_back(token);global.push_back(token);}
            break;}
        default:return qr::fail(error,"Zone history execution has an unsupported Quest effect kind");
        }
    }
    std::map<size_t,uintptr_t> actual;
    for(const auto& [pointer,list]:owners){const auto& current=after.owners.at(pointer);
        if(list.size()!=current.state.records.size())return qr::fail(error,"Zone history actual owner Quest count differs from original execution");
        bool generated=false;qr::Owner expected=current;expected.state={};expected.destination_uids.clear();
        for(size_t slot=0;slot<list.size();++slot){const auto token=list[slot];const auto& value=tokens[token];
            const auto& local=current.state.local[slot];
            if(current.destination_uids[slot]!=value.destination_uid)
                return qr::fail(error,"Zone history restored destination full UID differs");
            actual.emplace(token,local.quest);auto record=value.record;record.sequence_id=int32_t(slot);
            expected.state.records.push_back(record);expected.destination_uids.push_back(value.destination_uid);
            if(value.original){const auto& prior=before.owners.at(pointer);
                const auto found=std::find_if(prior.state.local.begin(),prior.state.local.end(),[&](const auto& row){return row.quest==value.original;});
                const auto index=size_t(std::find(before.quests.begin(),before.quests.end(),value.original)-before.quests.begin());
                const auto now=size_t(std::find(after.quests.begin(),after.quests.end(),local.quest)-after.quests.begin());
                if(found==prior.state.local.end()||record!=current.state.records[slot]||!detail::same_local(*found,local)||
                   before.links[index]!=after.links[now])return qr::fail(error,"Zone history changed a surviving Quest record/task/weak lifetime");
            }else generated=true;
        }
        if(generated){
            // A whole-owner Set removes every prior member. Any final new
            // definitions are validated by the existing original restore rule.
            if(std::any_of(list.begin(),list.end(),[&](size_t token){return tokens[token].original!=0;}))
                return qr::fail(error,"Zone history Set mixed old and new Quest lifetimes");
            qr::Witness source,empty,result;source.manager=empty.manager=result.manager=before.manager;
            source.owners.emplace(pointer,std::move(expected));auto blank=current;blank.state={};blank.destination_uids.clear();
            empty.owners.emplace(pointer,std::move(blank));result.owners.emplace(pointer,current);
            for(const auto& local:current.state.local){const auto index=size_t(std::find(after.quests.begin(),after.quests.end(),local.quest)-after.quests.begin());
                result.quests.push_back(local.quest);result.rows.push_back(after.rows[index]);result.links.push_back(after.links[index]);
                if(after.rows[index].record.completed.total>0)++result.completed;}
            if(!dh::restored(source,empty,result,{pointer},error))return false;
        }
    }
    std::vector<uintptr_t> expected_global;
    for(auto token:global)expected_global.push_back(actual.at(token));
    if(expected_global!=after.quests)return qr::fail(error,"Zone history actual complete global Quest swap-last/append order differs");
    auto selected=before;selected.removed=std::move(removed);
    if(!qc::references(selected,references_before,references_after,error))return false;
    retired=std::move(selected);error.clear();return true;
}
}

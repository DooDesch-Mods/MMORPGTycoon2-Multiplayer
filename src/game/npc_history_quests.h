#pragma once
#include "npc_quest_cascade.h"
#include "duplicate_quest_history.h"

namespace mt2mp::game::npc_history_quests {
namespace qr=monster_quest_retirement;
inline bool complete(const qr::Witness& value,std::string& error){
    if(value.quests.size()!=value.rows.size()||value.quests.size()!=value.links.size())return qr::fail(error,"NPC history complete global Quest rows/links differ");
    std::set<uintptr_t> global(value.quests.begin(),value.quests.end()),owned;
    if(global.size()!=value.quests.size()||global.contains(0))return qr::fail(error,"NPC history global Quest identity repeats or is null");
    for(const auto& [pointer,owner]:value.owners){
        if(owner.pointer!=pointer||!pointer||!owner.uid||owner.key.empty()||owner.state.records.size()!=owner.state.local.size())return qr::fail(error,"NPC history complete owner identity/rows differ");
        for(size_t slot=0;slot<owner.state.local.size();++slot){const auto& local=owner.state.local[slot];
            const auto index=size_t(std::find(value.quests.begin(),value.quests.end(),local.quest)-value.quests.begin());
            if(index>=value.quests.size()||!owned.insert(local.quest).second||local.owner!=pointer||
               value.links[index][0]!=local.destination||value.links[index][3]!=pointer||value.rows[index].owner_key!=owner.key||value.rows[index].record!=owner.state.records[slot])
                return qr::fail(error,"NPC history global Quest is absent, duplicated or differs from its complete owner record");
        }
    }
    return global==owned||qr::fail(error,"NPC history complete global/owner membership differs");
}
// Use the current witness, plus one explicitly restored owner seed. No read is
// issued at a retired address. Empty owned lists still receive a fresh read.
inline qr::Witness seed(const qr::Witness& current,const std::string& key,uintptr_t object,uint64_t uid){
    auto out=npc_quest_cascade::survivor_seed(current,key);
    if(object){qr::Owner owner;owner.pointer=object;owner.uid=uid;owner.key=key;out.owners.emplace(object,std::move(owner));}
    return out;
}
inline bool removed(const qr::Witness& before,const qr::Witness& after,const std::string& key,
                    const std::vector<uint64_t>& reconstructed,std::string& error){
    if(before.manager!=after.manager)return qr::fail(error,"NPC history retirement changed its complete Quest manager");
    if(!complete(before,error)||!complete(after,error)||!duplicate_quest_history::aggregate(before,error)||!duplicate_quest_history::aggregate(after,error))return false;
    size_t expected_owners=before.owners.size();
    for(const auto& [pointer,owner]:before.owners){
        if(owner.key==key){--expected_owners;if(after.owners.contains(pointer))return qr::fail(error,"NPC history retired owner remains current");continue;}
        const auto found=after.owners.find(pointer);
        if(found==after.owners.end()||found->second.uid!=owner.uid||found->second.key!=owner.key)return qr::fail(error,"NPC history retirement changed a surviving Quest owner identity");
        const bool rebuilt=std::find(reconstructed.begin(),reconstructed.end(),owner.uid)!=reconstructed.end();
        size_t slot=0;
        for(size_t index=0;index<owner.state.records.size();++index){
            if(std::find(before.removed.begin(),before.removed.end(),owner.state.local[index].quest)!=before.removed.end())continue;
            auto expected=owner.state.records[index];if(rebuilt&&expected.action==8)continue;expected.sequence_id=int32_t(slot);
            if(slot>=found->second.state.records.size())return qr::fail(error,"NPC history surviving authored definition is absent");
            const auto& actual=found->second.state.records[slot];
            if(rebuilt){if(actual.type!=expected.type||actual.name!=expected.name||actual.action!=expected.action||
                actual.monsters_to_kill!=expected.monsters_to_kill||actual.destination_key!=expected.destination_key||actual.sequence_id!=expected.sequence_id)
                    return qr::fail(error,"NPC history original reconstructed survivor definitions/order differ");
            }else{
                if(actual!=expected||slot>=found->second.state.local.size())return qr::fail(error,"NPC history changed a surviving untouched Quest record");
                const auto& old=owner.state.local[index];const auto& current=found->second.state.local[slot];
                if(old.quest!=current.quest||old.owner!=current.owner||old.destination!=current.destination||old.task!=current.task)
                    return qr::fail(error,"NPC history changed a surviving untouched Quest identity, owner, destination or task phase");
                const auto old_index=size_t(std::find(before.quests.begin(),before.quests.end(),old.quest)-before.quests.begin());
                const auto new_index=size_t(std::find(after.quests.begin(),after.quests.end(),old.quest)-after.quests.begin());
                if(old_index>=before.links.size()||new_index>=after.links.size()||before.links[old_index]!=after.links[new_index])
                    return qr::fail(error,"NPC history changed a surviving untouched Quest weak-link receipt");
            }
            ++slot;
        }
        if(slot!=found->second.state.records.size())return qr::fail(error,"NPC history surviving owner has extra definitions");
    }
    return after.owners.size()==expected_owners||qr::fail(error,"NPC history retirement introduced an unrelated Quest owner");
}
inline bool restored(const qr::Witness& original,const qr::Witness& before,const qr::Witness& after,
                     const std::string& key,uintptr_t old_pointer,uintptr_t current_pointer,uint64_t uid,
                     const std::vector<uint64_t>& changed_uids,std::string& error,bool quest_owner=true){
    auto source=original,prior=before;
    auto old=source.owners.find(old_pointer);
    if(old!=source.owners.end()){
        if(old->second.uid!=uid||old->second.key!=key)return qr::fail(error,"NPC history restored Quest owner original full UID/key differs");
        auto owner=old->second;source.owners.erase(old);owner.pointer=current_pointer;
        for(auto& local:owner.state.local){local.owner=current_pointer;if(local.destination==old_pointer)local.destination=current_pointer;}
        if(!source.owners.emplace(current_pointer,std::move(owner)).second)return qr::fail(error,"NPC history restored Quest owner collides with a survivor");
    }
    // A non-Quest NPC has no owner row, including after restoration.
    const auto now=after.owners.find(current_pointer);
    if(now!=after.owners.end()){
        if(now->second.uid!=uid||now->second.key!=key||prior.owners.contains(current_pointer))
            return qr::fail(error,"NPC history restored Quest owner is not a fresh exact canonical member");
        qr::Owner empty;empty.pointer=current_pointer;empty.uid=uid;empty.key=key;prior.owners.emplace(current_pointer,std::move(empty));
        if(!source.owners.contains(current_pointer)){
            qr::Owner empty_source;empty_source.pointer=current_pointer;empty_source.uid=uid;empty_source.key=key;
            source.owners.emplace(current_pointer,std::move(empty_source));
        }
    }
    std::set<uintptr_t> changed_owners;
    for(auto changed:changed_uids){uintptr_t pointer=0;
        if(changed==uid&&!quest_owner){
            if(now!=after.owners.end()||source.owners.contains(current_pointer))return qr::fail(error,"NPC history non-Quest actor unexpectedly owns Quest records");
            continue;
        }
        for(const auto& [object,owner]:after.owners)if(owner.uid==changed){
            if(pointer)return qr::fail(error,"NPC history inverse Quest owner full UID repeats");pointer=object;}
        if(!pointer)return qr::fail(error,"NPC history inverse Quest owner is absent from complete current witness");
        changed_owners.insert(pointer);
    }
    if(source.manager!=before.manager||before.manager!=after.manager)return qr::fail(error,"NPC history restored complete Quest manager differs");
    if(!complete(before,error)||!complete(after,error)||!duplicate_quest_history::aggregate(before,error)||!duplicate_quest_history::aggregate(after,error))return false;
    if(prior.owners.size()!=after.owners.size())return qr::fail(error,"NPC history restored complete Quest owner set differs");
    for(const auto& [pointer,owner]:after.owners){const auto old_owner=prior.owners.find(pointer);
        if(old_owner==prior.owners.end()||old_owner->second.key!=owner.key||old_owner->second.uid!=owner.uid)
            return qr::fail(error,"NPC history restored Quest owner full UID/canonical identity differs");
        if(changed_owners.contains(pointer)){
            const auto expected=source.owners.find(pointer);
            if(expected==source.owners.end()||expected->second.uid!=owner.uid||expected->second.key!=owner.key)
                return qr::fail(error,"NPC history changed Quest owner lacks its retained original snapshot");
            size_t slot=0;
            for(const auto& record:expected->second.state.records){if(record.action==8)continue;
                if(slot>=owner.state.records.size()||slot>=owner.state.local.size())return qr::fail(error,"NPC history original restored definition is absent");
                const auto& current=owner.state.records[slot];
                if(current.sequence_id!=int32_t(slot)||current.type!=record.type||current.name!=record.name||
                   current.action!=record.action||current.monsters_to_kill!=record.monsters_to_kill||current.destination_key!=record.destination_key||
                   owner.state.local[slot].owner!=pointer)
                    return qr::fail(error,"NPC history complete original restored definitions/destinations/order differ");
                ++slot;
            }
            if(slot!=owner.state.records.size()||slot!=owner.state.local.size())return qr::fail(error,"NPC history restored owner has extra definition/local rows");
        }else{
            const auto& previous=old_owner->second;
            if(previous.state.records!=owner.state.records||previous.state.local.size()!=owner.state.local.size())
                return qr::fail(error,"NPC history changed an unrelated complete Quest list");
            for(size_t slot=0;slot<previous.state.local.size();++slot){const auto& old_local=previous.state.local[slot];const auto& current=owner.state.local[slot];
                if(old_local.quest!=current.quest||old_local.owner!=current.owner||old_local.destination!=current.destination||old_local.task!=current.task)
                    return qr::fail(error,"NPC history changed unrelated Quest identity, owner, destination or task phase");
                const auto prior_index=size_t(std::find(before.quests.begin(),before.quests.end(),old_local.quest)-before.quests.begin());
                const auto current_index=size_t(std::find(after.quests.begin(),after.quests.end(),old_local.quest)-after.quests.begin());
                if(prior_index>=before.links.size()||current_index>=after.links.size()||before.links[prior_index]!=after.links[current_index])
                    return qr::fail(error,"NPC history changed an unrelated complete weak-link receipt");
            }
        }
    }
    // The retained original graph owns operation ordering. The World reader
    // proves complete current global/list/hash membership, and its actual
    // ordered result is included in the Host proof and exact peer comparison.
    return true;
}
inline bool references(const qr::Witness& before,const nlohmann::json& prior,const nlohmann::json& current,
                       const std::string& key,bool removing,const std::vector<uint64_t>& changed,std::string& error){
    auto selected=before;selected.removed.clear();
    for(size_t i=0;i<before.quests.size();++i){const auto owner=before.owners.find(before.links[i][3]);
        if(owner==before.owners.end())return qr::fail(error,"NPC history active-reference owner is absent from complete before witness");
        if(std::find(changed.begin(),changed.end(),owner->second.uid)!=changed.end())selected.removed.push_back(before.quests[i]);
    }
    if(removing){
        // The caller already selected the actual target cascade, including a
        // non-QuestGiver that cannot occur in the owner map.
        for(auto quest:before.removed)if(std::find(selected.removed.begin(),selected.removed.end(),quest)==selected.removed.end())selected.removed.push_back(quest);
        return npc_quest_cascade::references(selected,prior,current,key,error);
    }
    return quest_cascade_read::references(selected,prior,current,error);
}
}

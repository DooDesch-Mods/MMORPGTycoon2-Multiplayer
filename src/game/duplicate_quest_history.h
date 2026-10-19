#pragma once
#include "monster_quest_retirement.h"
namespace mt2mp::game::duplicate_quest_history {
namespace q=monster_quest_retirement;
struct OwnerRebind {uintptr_t before=0,after=0;uint64_t uid=0;std::string key;};
// Only confirmed child lifecycle receipts may supply these local remaps. The
// existing Quest reader uses previous owners to retain empty lists; it must
// never reread a retired pointer after the original Make creates a new actor.
// The result is only that reader's previous-owner seed, not a current complete
// Quest witness. The reader must still produce the actual global/list state.
inline bool previous_owners(const q::Witness& current,const q::Witness& original,const std::vector<OwnerRebind>& restored,
                            const std::set<uintptr_t>& retired,q::Witness& out,std::string& e){
    auto next=current;std::set<uintptr_t> before,after;std::set<std::string> keys;
    for(const auto& value:restored){const auto source=original.owners.find(value.before);
        if(!value.before||!value.after||!value.uid||value.key.empty()||source==original.owners.end()||
           source->second.uid!=value.uid||source->second.key!=value.key||
           !before.insert(value.before).second||!after.insert(value.after).second||!keys.insert(value.key).second||retired.contains(value.before))
            return q::fail(e,"Duplicate Quest previous owner restore receipt differs from original canonical full UID");
        for(const auto& [pointer,owner]:next.owners)if((pointer==value.after||owner.key==value.key)&&pointer!=value.before)
            return q::fail(e,"Duplicate Quest restored owner aliases an unrelated current owner");
        next.owners.erase(value.before);auto seed=source->second;seed.pointer=value.after;seed.state={};seed.destination_uids.clear();
        if(!next.owners.emplace(value.after,std::move(seed)).second)return q::fail(e,"Duplicate Quest restored previous owner pointer repeats");
    }
    for(auto pointer:retired){const auto old=next.owners.find(pointer);
        if(old==next.owners.end())return q::fail(e,"Duplicate Quest retired previous owner was not captured");
        next.owners.erase(old);
    }
    out=std::move(next);
    return true;
}
inline bool aggregate(const q::Witness& value,std::string& e){
    int32_t completed=0;
    for(const auto& row:value.rows)if(row.record.completed.total>0)++completed;
    return completed==value.completed||q::fail(e,"Duplicate history complete Quest aggregate differs actual="+
        std::to_string(value.completed)+" derived="+std::to_string(completed));
}
// The original complete graph owns changed Quest lists. Preserve all other
// owners exactly, while allowing its own swap-last global membership order.
// Complete actual global/list state is still compared with the Host.
inline bool unchanged_owners(const q::Witness& before,const q::Witness& after,const std::set<std::string>& changed,
                             const std::set<std::string>& retired,const std::set<std::string>& restored,std::string& e){
    if(before.manager!=after.manager)return q::fail(e,"Duplicate history Quest manager differs");
    if(!aggregate(before,e)||!aggregate(after,e))return false;
    for(const auto& key:retired)if(!changed.contains(key)||restored.contains(key))return q::fail(e,"Duplicate history retired Quest owner is not its exclusive graph target");
    for(const auto& key:restored)if(!changed.contains(key))return q::fail(e,"Duplicate history restored Quest owner is not its graph target");
    std::map<std::string,const q::Owner*> old,current;std::map<uintptr_t,size_t> old_quests,current_quests;
    for(const auto& [pointer,owner]:before.owners){if(pointer!=owner.pointer||!old.emplace(owner.key,&owner).second)
            return q::fail(e,"Duplicate prior complete Quest owner pointer/key repeats");}
    for(const auto& [pointer,owner]:after.owners){if(pointer!=owner.pointer||!current.emplace(owner.key,&owner).second)
            return q::fail(e,"Duplicate actual complete Quest owner pointer/key repeats");}
    for(size_t i=0;i<before.quests.size();++i)if(!old_quests.emplace(before.quests[i],i).second)return q::fail(e,"Duplicate prior complete Quest identity repeats");
    for(size_t i=0;i<after.quests.size();++i)if(!current_quests.emplace(after.quests[i],i).second)return q::fail(e,"Duplicate actual complete Quest identity repeats");
    for(const auto& key:retired)if(!old.contains(key)||current.contains(key))return q::fail(e,"Duplicate original retired Quest owner remains or was never captured");
    for(const auto& key:restored)if(old.contains(key)||!current.contains(key))return q::fail(e,"Duplicate original restored Quest owner is not newly present");
    for(const auto& key:changed)if(!old.contains(key)&&!restored.contains(key))return q::fail(e,"Duplicate history changed Quest owner lacks an original receipt");
    for(const auto& [key,owner]:current)if(!old.contains(key)&&!restored.contains(key))return q::fail(e,"Duplicate history introduced an unrelated Quest owner");
    for(const auto& [key,owner]:old){if(retired.contains(key))continue;
        const auto found=current.find(key);
        if(found==current.end()||found->second->uid!=owner->uid||found->second->pointer!=owner->pointer)
            return q::fail(e,"Duplicate history changed a surviving Quest owner full UID/pointer");
        if(changed.contains(key))continue;
        if(found->second->state.records!=owner->state.records||found->second->state.local.size()!=owner->state.local.size())
            return q::fail(e,"Duplicate history changed an unrelated complete Quest list");
        for(size_t i=0;i<owner->state.local.size();++i){const auto& a=owner->state.local[i];const auto& b=found->second->state.local[i];
            const auto old_index=old_quests.find(a.quest),new_index=current_quests.find(a.quest);
            if(a.quest!=b.quest||a.owner!=b.owner||a.destination!=b.destination||a.task!=b.task||
               a.npc_list_member!=b.npc_list_member||a.owner_weak_confirmed!=b.owner_weak_confirmed||
               a.destination_weak_confirmed!=b.destination_weak_confirmed||a.manager_member!=b.manager_member||
               old_index==old_quests.end()||new_index==current_quests.end()||old_index->second>=before.links.size()||new_index->second>=after.links.size()||
               before.links[old_index->second]!=after.links[new_index->second])
                return q::fail(e,"Duplicate history changed an unrelated Quest identity/task/weak reference");
        }
    }
    return true;
}
// Preserve the original graph's ordered Destroy targets. Each destination's
// OnDestroy removes its matching Quests before the next destination executes.
inline bool removal_order(const q::Witness& before,const std::vector<uintptr_t>& destinations,q::Witness& expected,std::string& e){
    expected=before;expected.removed.clear();auto remaining=before.quests;
    std::set<uintptr_t> unique;
    if(before.quests.size()!=before.links.size()||before.quests.size()!=before.rows.size())return q::fail(e,"Duplicate Quest complete before rows differ");
    for(auto destination:destinations){
        if(!destination||!unique.insert(destination).second)return q::fail(e,"Duplicate Quest Destroy destination repeats");
        std::vector<uintptr_t> removed;
        for(auto quest:remaining){const auto index=size_t(std::find(before.quests.begin(),before.quests.end(),quest)-before.quests.begin());
            if(index>=before.links.size())return q::fail(e,"Duplicate Quest Destroy member absent from before");
            if(before.links[index][0]==destination)removed.push_back(quest);}
        for(auto quest:removed){const auto found=std::find(remaining.begin(),remaining.end(),quest);
            if(found==remaining.end())return q::fail(e,"Duplicate Quest Destroy member already removed");
            *found=remaining.back();remaining.pop_back();expected.removed.push_back(quest);}
    }
    return aggregate(before,e);
}
inline bool removed(const q::Witness& before,const q::Witness& after,const std::vector<uintptr_t>& destinations,std::string& e){
    q::Witness expected;if(!removal_order(before,destinations,expected,e)||!aggregate(after,e))return false;
    return q::survivors(expected,after,e);
}
inline std::vector<uintptr_t> affected(const q::Witness& original,const std::vector<uintptr_t>& destinations){
    std::vector<uintptr_t> result;
    for(size_t i=0;i<original.quests.size();++i)if(std::find(destinations.begin(),destinations.end(),original.links[i][0])!=destinations.end()){
        const auto owner=original.links[i][3];if(std::find(result.begin(),result.end(),owner)==result.end())result.push_back(owner);}
    return result;
}
inline bool restored(const q::Witness& original,const q::Witness& before,const q::Witness& after,
                     const std::vector<uintptr_t>& ordered_owners,std::string& e){
    if(original.manager!=before.manager||before.manager!=after.manager||before.owners.size()!=after.owners.size())
        return q::fail(e,"Duplicate Quest restore manager/complete owner set differs");
    if(!aggregate(before,e)||!aggregate(after,e))return false;
    auto global=before.quests;std::set<uintptr_t> changed;
    for(auto pointer:ordered_owners){
        const auto source=original.owners.find(pointer),prior=before.owners.find(pointer),current=after.owners.find(pointer);
        if(!changed.insert(pointer).second||source==original.owners.end()||prior==before.owners.end()||current==after.owners.end()||
           source->second.uid!=prior->second.uid||prior->second.uid!=current->second.uid||
           source->second.key!=prior->second.key||prior->second.key!=current->second.key)
            return q::fail(e,"Duplicate Quest restore original owner/full UID/order differs");
        // DoSetNPCQuests removes the complete current list and appends valid
        // stored definitions in source order. Native Set computes new runtime
        // values; the caller compares their complete readback with the Host.
        for(const auto& local:prior->second.state.local){const auto found=std::find(global.begin(),global.end(),local.quest);
            if(found==global.end())return q::fail(e,"Duplicate Quest restore previous owner member absent");
            *found=global.back();global.pop_back();}
        size_t index=0;
        for(const auto& row:source->second.state.records){if(row.action==8)continue;
            if(index>=current->second.state.records.size()||index>=current->second.state.local.size())return q::fail(e,"Duplicate Quest restore original definition missing");
            const auto& now=current->second.state.records[index];const auto& local=current->second.state.local[index];
            if(now.sequence_id!=int32_t(index)||now.type!=row.type||now.name!=row.name||now.action!=row.action||
               now.monsters_to_kill!=row.monsters_to_kill||now.destination_key!=row.destination_key||local.owner!=pointer)
                return q::fail(e,"Duplicate Quest restore authored fields/destination/order differ");
            if(std::find(global.begin(),global.end(),local.quest)!=global.end())return q::fail(e,"Duplicate Quest restore duplicates live global member");
            global.push_back(local.quest);++index;}
        if(index!=current->second.state.records.size())return q::fail(e,"Duplicate Quest restore has unexpected owner records");
    }
    if(global!=after.quests)return q::fail(e,"Duplicate Quest restore complete original global order differs");
    for(const auto& [pointer,prior]:before.owners){const auto current=after.owners.find(pointer);
        if(current==after.owners.end()||prior.uid!=current->second.uid||prior.key!=current->second.key)
            return q::fail(e,"Duplicate Quest restore changed unrelated owner identity");
        if(changed.contains(pointer))continue;
        if(prior.state.records!=current->second.state.records||prior.state.local.size()!=current->second.state.local.size())
            return q::fail(e,"Duplicate Quest restore changed unrelated records");
        for(size_t i=0;i<prior.state.local.size();++i){const auto& old=prior.state.local[i];const auto& now=current->second.state.local[i];
            if(old.quest!=now.quest||old.owner!=now.owner||old.destination!=now.destination||old.task!=now.task)
                return q::fail(e,"Duplicate Quest restore changed unrelated task/reference");
            const auto old_index=size_t(std::find(before.quests.begin(),before.quests.end(),old.quest)-before.quests.begin());
            const auto now_index=size_t(std::find(after.quests.begin(),after.quests.end(),now.quest)-after.quests.begin());
            if(old_index>=before.links.size()||now_index>=after.links.size()||before.links[old_index]!=after.links[now_index])
                return q::fail(e,"Duplicate Quest restore changed unrelated weak link");}
    }
    return true;
}
}

#pragma once
#include "npc_target_key.h"

#include "npc_probe.h"
#include "npc_quest_records.h"
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::npc_probe::action_runtime::host_binding {

inline bool reject(std::string& error,const std::string& cause){error=cause;return false;}

inline bool quest_owners(npc_census::Memory& memory,uintptr_t image,uintptr_t npc,
                         int32_t expected_count,std::vector<std::pair<int32_t,uintptr_t>>& output,
                         std::string& error) {
    using npc_quest_records::get;
    if(!image||image>UINTPTR_MAX-0x174ac10||!npc||npc>UINTPTR_MAX-0x874||
       expected_count<0||expected_count>100000)
        return reject(error,"NPC Hoststate Quest image, NPC or expected count invalid");
    uintptr_t ap=0,manager=0,entries=0,proxy=0,weak=0;
    int32_t type=-1,count=-1,capacity=-1;
    if(!get(memory,npc,0,ap,"owner NPC AP",error)||
       !get(memory,npc,0x80c,type,"owner NPC type",error)||
       !get(memory,npc,0x28,proxy,"owner NPC weak proxy",error)||
       !get(memory,npc,0x868,entries,"owner Quest array",error)||
       !get(memory,npc,0x870,count,"owner Quest count",error)||
       !get(memory,npc,0x874,capacity,"owner Quest capacity",error)||
       !get(memory,image,0x13df410,manager,"owner QuestManager singleton",error))return false;
    if(ap!=image+0x1742980||type!=0||!proxy||!manager||count!=expected_count||
       capacity<count||capacity>100000||(count&&!entries)||
       entries>UINTPTR_MAX-size_t(count)*sizeof(uintptr_t))
        return reject(error,"NPC Hoststate Quest owner class, type, array or count differs");
    if(!get(memory,proxy,0,weak,"owner NPC weak target",error)||weak!=npc+0x20)
        return reject(error,"NPC Hoststate Quest owner weak target differs");
    std::set<int32_t> sequences;std::set<uintptr_t> pointers;
    std::vector<std::pair<int32_t,uintptr_t>> result;
    result.reserve(size_t(count));
    for(int32_t index=0;index<count;++index){
        uintptr_t quest=0,quest_ap=0,owner=0,owner_proxy=0;
        int32_t sequence=-1;
        if(!get(memory,entries,size_t(index)*8,quest,"owner Quest entry",error))return false;
        if(!quest||!pointers.insert(quest).second)
            return reject(error,"NPC Hoststate Quest pointer absent or duplicated");
        if(!get(memory,quest,0,quest_ap,"owner Quest AP",error)||
           !get(memory,quest,0x84,sequence,"owner Quest sequence",error)||
           !get(memory,quest,0xb8,owner,"owner Quest NPC",error)||
           !get(memory,quest,0xc0,owner_proxy,"owner Quest weak proxy",error))return false;
        if(quest_ap!=image+0x174ac10||sequence<0||
           !sequences.insert(sequence).second||owner!=npc||owner_proxy!=proxy)
            return reject(error,"NPC Hoststate Quest class, sequence or NPC owner differs");
        if(!npc_quest_records::manager_member(memory,image,manager,quest,error))return false;
        result.emplace_back(sequence,quest);
    }
    uintptr_t final_manager=0,final_entries=0,final_proxy=0,final_weak=0;
    int32_t final_count=-1,final_capacity=-1;
    if(!get(memory,image,0x13df410,final_manager,"owner final QuestManager",error)||
       !get(memory,npc,0x868,final_entries,"owner final Quest array",error)||
       !get(memory,npc,0x870,final_count,"owner final Quest count",error)||
       !get(memory,npc,0x874,final_capacity,"owner final Quest capacity",error)||
       !get(memory,npc,0x28,final_proxy,"owner final NPC proxy",error)||
       !get(memory,proxy,0,final_weak,"owner final NPC weak target",error))return false;
    if(final_manager!=manager||final_entries!=entries||final_count!=count||
       final_capacity!=capacity||final_proxy!=proxy||final_weak!=npc+0x20)
        return reject(error,"NPC Hoststate Quest owner header changed during read");
    for(int32_t index=0;index<count;++index){
        uintptr_t current=0;
        if(!get(memory,entries,size_t(index)*8,current,"owner final Quest entry",error))return false;
        if(current!=result[size_t(index)].second)
            return reject(error,"NPC Hoststate Quest array membership changed during read");
    }
    output=std::move(result);error.clear();return true;
}

template<class Operations,class Resolve>
bool collect(const Operations& operations,const action_session::Binding& binding,
             const std::string& snapshot,Resolve&& resolve,
             std::vector<StateOwner>& output,std::string& error) {
    std::vector<StateOwner> result;
    std::set<uintptr_t> seen_npcs;
    for(const auto& [id,op]:operations) {
        if(op.request.action!=action_session::Action::PlaceNPC||
           op.request.snapshot_uuid!=snapshot||op.binding!=binding||
           !op.confirmed||!op.retired||op.target_absent)continue;
        if(!op.local_uid||!op.local_pointer)
            return reject(error,"NPC Hoststate confirmed Place lacks local UID or pointer");
        StateOwner owner;
        if(!resolve(id,op,owner,error)) {
            if(error.empty())error="NPC Hoststate fresh Place resolver failed without cause";
            return false;
        }
        if(owner.key!="action:"+id||!owner.npc||owner.npc!=op.local_pointer||
           !seen_npcs.insert(owner.npc).second)
            return reject(error,"NPC Hoststate Place key, pointer or uniqueness differs");
        std::set<int32_t> sequences;
        std::set<uintptr_t> quests;
        for(const auto& [sequence,pointer]:owner.quests)
            if(sequence<0||!pointer||!sequences.insert(sequence).second||
               !quests.insert(pointer).second)
                return reject(error,"NPC Hoststate Quest sequence or pointer invalid/duplicated");
        result.push_back(std::move(owner));
    }
    std::sort(result.begin(),result.end(),[](const StateOwner& a,const StateOwner& b){return a.key<b.key;});
    output=std::move(result);error.clear();return true;
}

inline bool resolve(const std::vector<StateOwner>& owners,const std::string& key,
                    int32_t sequence_id,uintptr_t& npc,uintptr_t& quest,std::string& error) {
    npc=0;quest=0;
    if(!npc_target_key::valid(key)||sequence_id< -1)
        return reject(error,"NPC Hoststate FreshLink key or Quest sequence invalid");
    const StateOwner* found=nullptr;
    for(const auto& owner:owners)if(owner.key==key){
        if(found)return reject(error,"NPC Hoststate FreshLink owner key duplicated");
        found=&owner;
    }
    if(!found||!found->npc)return reject(error,"NPC Hoststate FreshLink owner absent");
    if(sequence_id>=0){
        for(const auto& [sequence,pointer]:found->quests)if(sequence==sequence_id){
            if(quest)return reject(error,"NPC Hoststate FreshLink Quest sequence duplicated");
            quest=pointer;
        }
        if(!quest)return reject(error,"NPC Hoststate FreshLink Quest sequence absent");
    }
    npc=found->npc;error.clear();return true;
}

} // namespace mt2mp::game::npc_probe::action_runtime::host_binding

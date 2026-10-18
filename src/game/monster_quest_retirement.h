#pragma once
#include "npc_quest_retirement.h"
#include "typed_action/monster_quest_retirement.h"
#include <map>
#include <set>

namespace mt2mp::game::monster_quest_retirement {
namespace q=npc_quest_records;
struct Owner {
    uintptr_t pointer=0;uint64_t uid=0;std::string key;q::State state;
    std::vector<uint64_t> destination_uids;
};
struct Witness {
    uintptr_t manager=0;int32_t completed=0;
    std::vector<uintptr_t> quests,removed;
    std::vector<std::array<uintptr_t,6>> links;
    std::map<uintptr_t,Owner> owners;
    std::vector<action_session::MonsterRetiredQuest> rows;
};
inline bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class Memory,class BindOwner,class BindDestination>
bool read(Memory& m,uintptr_t image,uintptr_t object,const std::string& target,
          BindOwner&& bind_owner,BindDestination&& bind_destination,Witness& out,std::string& e,
          const Witness* previous=nullptr){
    out={};npc_quest_retirement::State manager;
    if(!npc_quest_retirement::read(m,image,{},manager,e))return false;
    if(manager.manager_count>4096)return fail(e,"Monster Quest global count exceeds bound");
    out.manager=manager.manager;uintptr_t data=0;
    if(!q::get(m,out.manager,0x30,data,"Monster Quest global array",e)||
       !q::get(m,out.manager,0x48,out.completed,"Monster Quest completed count",e))return false;
    if(out.completed<0||out.completed>manager.manager_count)return fail(e,"Monster Quest completed count differs");
    const auto owner_read=[&](uintptr_t pointer){
        if(out.owners.contains(pointer))return true;
        Owner owner;owner.pointer=pointer;
        if(!pointer||!bind_owner(pointer,owner.uid,owner.key,e))return false;
        const auto destination=[&](uintptr_t ptr,std::string& key,std::string& cause){
            if(ptr==object){key=target;return true;}return bind_destination(ptr,key,cause);
        };
        if(!q::read(m,image,pointer,destination,owner.state,e))return false;
        for(size_t i=0;i<owner.state.records.size();++i){
            if(owner.state.records[i].sequence_id!=int32_t(i))return fail(e,"Monster Quest owner sequence is not its array index");
            uint64_t uid=0;if(!q::get(m,owner.state.local[i].destination,0x18,uid,"Monster Quest destination full UID",e)||!uid)
                return fail(e,e.empty()?"Monster Quest destination full UID absent":e);
            owner.destination_uids.push_back(uid);
        }
        out.owners.emplace(pointer,std::move(owner));return true;
    };
    // Owners whose final Quest was removed still need an empty-list readback.
    if(previous)for(const auto& [pointer,old]:previous->owners){
        if(!owner_read(pointer))return false;
        const auto& current=out.owners.at(pointer);
        if(current.uid!=old.uid||current.key!=old.key)return fail(e,"Monster Quest owner identity changed");
    }
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<manager.manager_count;++i){
        uintptr_t quest=0,ap=0,destination=0,proxy=0,weak=0,owner=0,owner_proxy=0,owner_weak=0;
        if(!q::get(m,data,size_t(i)*8,quest,"Monster Quest global member",e)||!quest||!unique.insert(quest).second||
           !q::get(m,quest,0,ap,"Monster Quest class",e)||ap!=image+0x174ac10||
           !q::manager_member(m,image,out.manager,quest,e)||
           !q::get(m,quest,0xf0,destination,"Monster Quest destination",e)||
           !q::get(m,quest,0xf8,proxy,"Monster Quest destination proxy",e)||
           !q::get(m,quest,0xb8,owner,"Monster Quest owner",e)||
           !q::get(m,quest,0xc0,owner_proxy,"Monster Quest owner proxy",e))
            return fail(e,e.empty()?"Monster Quest class or unique membership differs":e);
        if(!proxy||!owner_proxy||!destination||!owner||
           !q::get(m,proxy,0,weak,"Monster Quest destination weak",e)||weak!=destination+0x20||
           !q::get(m,owner_proxy,0,owner_weak,"Monster Quest owner weak",e)||owner_weak!=owner+0x20)
            return fail(e,e.empty()?"Monster Quest weak owner is stale or differs":e);
        if(!owner_read(owner))return false;
        const auto& saved=out.owners.at(owner);size_t index=0;
        while(index<saved.state.local.size()&&saved.state.local[index].quest!=quest)++index;
        if(index==saved.state.local.size())return fail(e,"Monster Quest global member absent from owner list");
        out.quests.push_back(quest);out.links.push_back({destination,proxy,weak,owner,owner_proxy,owner_weak});
        out.rows.push_back({saved.key,saved.state.records[index]});
        if(destination==object)out.removed.push_back(quest);
    }
    for(const auto& [pointer,owner]:out.owners){(void)pointer;
        for(const auto& local:owner.state.local)if(!unique.contains(local.quest))return fail(e,"Monster Quest owner member absent from global array");}
    npc_quest_retirement::State after;int32_t completed=0;
    if(!npc_quest_retirement::read(m,image,out.quests,after,e)||
       !q::get(m,out.manager,0x48,completed,"Monster Quest completed reread",e))return false;
    if(after.manager!=out.manager||after.manager_count!=manager.manager_count||completed!=out.completed||
       std::any_of(after.quests.begin(),after.quests.end(),[](const auto& p){return !p.hash_present||!p.array_present;}))
        return fail(e,"Monster Quest membership changed during read");
    return true;
}
inline std::vector<uintptr_t> inverse_owners(const Witness& before){
    std::vector<uintptr_t> result;
    for(size_t i=0;i<before.quests.size();++i)if(std::find(before.removed.begin(),before.removed.end(),before.quests[i])!=before.removed.end()){
        const auto owner=before.links[i][3];
        if(std::find(result.begin(),result.end(),owner)==result.end())result.push_back(owner);
    }
    return result;
}
inline bool unchanged(const Witness& before,const Witness& after,std::string& e){
    if(before.manager!=after.manager||before.completed!=after.completed||before.quests!=after.quests||
       before.links!=after.links||before.removed!=after.removed||before.rows!=after.rows||before.owners.size()!=after.owners.size())
        return fail(e,"Monster Quest pre-original records or links changed");
    for(const auto& [pointer,owner]:before.owners){const auto found=after.owners.find(pointer);
        if(found==after.owners.end()||found->second.uid!=owner.uid||found->second.key!=owner.key)
            return fail(e,"Monster Quest pre-original owner identity changed");
        for(size_t i=0;i<owner.state.local.size();++i)
            if(found->second.state.local[i].task!=owner.state.local[i].task)return fail(e,"Monster Quest pre-original task phase changed");
    }
    return true;
}
inline bool survivors(const Witness& before,const Witness& after,std::string& e){
    if(before.manager!=after.manager||!after.removed.empty())return fail(e,"Monster Quest manager changed or target remains referenced");
    auto expected=before.quests;auto completed=before.completed;
    for(const auto pointer:before.removed){
        const auto found=std::find(expected.begin(),expected.end(),pointer);
        if(found==expected.end())return fail(e,"Monster Quest retirement token is not unique");
        *found=expected.back();expected.pop_back();
        const auto index=size_t(std::find(before.quests.begin(),before.quests.end(),pointer)-before.quests.begin());
        if(before.rows[index].record.completed.total>0)--completed;
    }
    if(expected!=after.quests||completed!=after.completed)return fail(e,"Monster Quest swap-last order or completed count differs");
    if(before.owners.size()!=after.owners.size())return fail(e,"Monster Quest owner set changed");
    for(const auto& [pointer,old]:before.owners){
        const auto found=after.owners.find(pointer);
        if(found==after.owners.end()||found->second.uid!=old.uid||found->second.key!=old.key)return fail(e,"Monster Quest owner identity differs");
        const auto& now=found->second.state;size_t index=0;
        for(size_t i=0;i<old.state.local.size();++i){const auto& local=old.state.local[i];
            if(std::find(before.removed.begin(),before.removed.end(),local.quest)!=before.removed.end())continue;
            auto record=old.state.records[i];record.sequence_id=int32_t(index);
            if(index>=now.records.size()||now.records[index]!=record||now.local[index].quest!=local.quest||
               now.local[index].owner!=local.owner||now.local[index].destination!=local.destination||now.local[index].task!=local.task)
                return fail(e,"Monster Quest survivor content, task, identity or renumbering differs");
            ++index;
        }
        if(index!=now.records.size())return fail(e,"Monster Quest owner has unexpected survivor count");
    }
    for(size_t i=0;i<after.quests.size();++i){
        const auto old=size_t(std::find(before.quests.begin(),before.quests.end(),after.quests[i])-before.quests.begin());
        if(old>=before.links.size()||after.links[i]!=before.links[old])return fail(e,"Monster Quest survivor weak link changed");
    }
    return true;
}
template<class Memory>
bool absent(Memory& m,uintptr_t image,const Witness& before,std::string& e){
    npc_quest_retirement::State retired;
    return npc_quest_retirement::read(m,image,before.removed,retired,e)&&
        ((retired.manager==before.manager&&retired.all_absent())||fail(e,"Monster retired Quest remains in manager hash or array"));
}
template<class Memory>
bool inverse(Memory& m,uintptr_t image,uintptr_t pointer,const Owner& owner,std::string& e){
    uintptr_t ap=0,data=0;uint64_t uid=0;uint8_t random=1;int32_t count=0,capacity=0;
    if(!q::get(m,pointer,0,ap,"Monster Quest inverse class",e)||ap!=image+0x1713330||
       !q::get(m,pointer,0xb8,uid,"Monster Quest inverse owner UID",e)||uid!=owner.uid||
       !q::get(m,pointer,0xc0,random,"Monster Quest inverse generation flag",e)||random||
       !q::get(m,pointer,0xd0,data,"Monster Quest inverse records",e)||
       !q::get(m,pointer,0xd8,count,"Monster Quest inverse count",e)||
       !q::get(m,pointer,0xdc,capacity,"Monster Quest inverse capacity",e))
        return fail(e,e.empty()?"Monster Quest inverse class, owner or generation flag differs":e);
    if(count<0||count>4096||capacity<count||capacity>100000||(count&&!data)||data>UINTPTR_MAX-size_t(count)*0x58)
        return fail(e,"Monster Quest inverse array bounds differ");
    size_t index=0;
    for(size_t i=0;i<owner.state.records.size();++i){const auto& record=owner.state.records[i];
        // IsValid excludes action 8. Both weak owners were verified while alive.
        if(record.action==8)continue;
        if(index>=size_t(count))return fail(e,"Monster Quest inverse record absent");
        const auto item=data+index*0x58;int32_t type=0,action=0,kills=0;uint64_t destination=0;std::string name,description;
        if(!q::get(m,item,0,type,"Monster Quest inverse type",e)||!q::get(m,item,8,destination,"Monster Quest inverse destination UID",e)||
           !q::get(m,item,0x10,action,"Monster Quest inverse action",e)||!q::get(m,item,0x14,kills,"Monster Quest inverse kill count",e)||
           !q::read_string(m,item,0x18,"Monster Quest inverse name",name,e)||
           !q::read_string(m,item,0x38,"Monster Quest inverse description",description,e))return false;
        if(type!=record.type||destination!=owner.destination_uids[i]||action!=record.action||kills!=record.monsters_to_kill||
           name!=record.name||description!="[Quest description goes here]")return fail(e,"Monster Quest inverse saved record differs");
        ++index;
    }
    return index==size_t(count)||fail(e,"Monster Quest inverse has extra records");
}
}

#pragma once
#include "monster_quest_retirement.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::npc_quest_list_read {
namespace qr=monster_quest_retirement;
inline bool added(const qr::Witness& before,const qr::Witness& after,uintptr_t owner,std::string& e){
    if(before.manager!=after.manager||before.completed!=after.completed||after.quests.size()!=before.quests.size()+1||
       after.rows.size()!=after.quests.size()||after.links.size()!=after.quests.size()||
       before.owners.size()!=after.owners.size()||!std::equal(before.quests.begin(),before.quests.end(),after.quests.begin()))
        return qr::fail(e,"Quest Add global append, manager or owner set differs");
    for(size_t i=0;i<before.quests.size();++i)if(before.rows[i]!=after.rows[i]||before.links[i]!=after.links[i])
        return qr::fail(e,"Quest Add changed an existing global record or weak link");
    const auto created=after.quests.back();
    if(!created||std::find(before.quests.begin(),before.quests.end(),created)!=before.quests.end())return qr::fail(e,"Quest Add original pointer is not new");
    for(const auto& [pointer,old]:before.owners){const auto found=after.owners.find(pointer);
        if(found==after.owners.end()||found->second.uid!=old.uid||found->second.key!=old.key)
            return qr::fail(e,"Quest Add owner full identity changed");
        const auto& now=found->second;
        const auto extra=pointer==owner?size_t(1):size_t(0);
        if(now.state.local.size()!=old.state.local.size()+extra||now.state.records.size()!=old.state.records.size()+extra)
            return qr::fail(e,"Quest Add owner slot count differs");
        for(size_t i=0;i<old.state.local.size();++i){const auto& a=old.state.local[i];const auto& b=now.state.local[i];
            if(old.state.records[i]!=now.state.records[i]||a.quest!=b.quest||a.owner!=b.owner||a.destination!=b.destination||a.task!=b.task||
               old.destination_uids[i]!=now.destination_uids[i])return qr::fail(e,"Quest Add changed a survivor record, task or pointer");}
        if(extra&&(now.state.local.back().quest!=created||now.state.local.back().task==npc_quest_records::TaskStatus::Absent))
            return qr::fail(e,"Quest Add original appended record or rating task absent");
    }
    return true;
}
inline bool edited(const qr::Witness& before,const qr::Witness& after,uintptr_t owner,int32_t slot,std::string& e){
    if(slot<0||!before.owners.contains(owner)||!after.owners.contains(owner)||size_t(slot)>=before.owners.at(owner).state.local.size()||
       size_t(slot)>=after.owners.at(owner).state.records.size())return qr::fail(e,"Quest edit selected owner/current slot absent");
    auto expected=before;const auto pointer=before.owners.at(owner).state.local[size_t(slot)].quest;
    const auto& record=after.owners.at(owner).state.records[size_t(slot)];
    expected.owners.at(owner).state.records[size_t(slot)]=record;
    size_t matched=0;for(size_t i=0;i<expected.quests.size();++i)if(expected.quests[i]==pointer){expected.rows[i].record=record;++matched;}
    if(matched!=1||!qr::unchanged(expected,after,e))return qr::fail(e,e.empty()?"Quest edit global current membership differs":e);
    for(const auto& [native,old]:expected.owners){const auto found=after.owners.find(native);
        if(found==after.owners.end()||old.state.records!=found->second.state.records||old.destination_uids!=found->second.destination_uids||
           old.state.local.size()!=found->second.state.local.size())return qr::fail(e,"Quest edit owner records/count/full destination identity differs");
        for(size_t i=0;i<old.state.local.size();++i){const auto& a=old.state.local[i];const auto& b=found->second.state.local[i];
            if(a.quest!=b.quest||a.owner!=b.owner||a.destination!=b.destination||a.task!=b.task||a.manager_member!=b.manager_member||
               a.owner_weak_confirmed!=b.owner_weak_confirmed||a.destination_weak_confirmed!=b.destination_weak_confirmed||a.npc_list_member!=b.npc_list_member)
                return qr::fail(e,"Quest edit changed original pointer, weak link, task or membership");}}
    return true;
}
// Rows are read through the existing visitor reader. Keys use owner/current-slot,
// so the expected survivor key changes when an earlier native slot is removed.
inline bool references(const nlohmann::json& before,const nlohmann::json& after,const std::string& owner,int32_t slot,std::string& e){
    if(slot<0)return before==after||qr::fail(e,"Quest Add changed active references");
    auto expected=before;
    const auto removed=owner+"/quest/"+std::to_string(slot);
    if(!expected.is_array()||!after.is_array()||expected.size()!=after.size())return qr::fail(e,"Quest Remove Subscriber cohort differs");
    for(size_t i=0;i<expected.size();++i){auto& progress=expected[i]["progress"];
        if(!progress.is_array()||!after[i].contains("progress")||after[i]["progress"].size()!=progress.size())return qr::fail(e,"Quest Remove active progress count differs");
        for(size_t j=0;j<progress.size();++j){auto& row=progress[j];auto& link=row["quest"];
            if(link["state"]!=2)continue;
            const auto key=link["key"].get<std::string>();
            if(key==removed){const auto& actual=after[i]["progress"][j]["quest"];
                if((actual["state"]!=0&&actual["state"]!=1)||actual["key"]!="")return qr::fail(e,"Quest Remove retained a Live selected reference");
                link=actual;row["complete"]=false;
            }else for(int32_t k=slot+1;k<5;++k)if(key==owner+"/quest/"+std::to_string(k)){
                link["key"]=owner+"/quest/"+std::to_string(k-1);break;}
        }
    }
    return expected==after||qr::fail(e,"Quest Remove active references, counters or survivor current slots differ");
}
}

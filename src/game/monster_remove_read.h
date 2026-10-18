#pragma once
#include "monster_zone_read.h"
#include "npc_quest_retirement.h"
#include "monster_quest_retirement.h"

namespace mt2mp::game::monster_remove_read {
template<class Name>
bool read_names(const monster_zone_read::State& state,Name&& read,std::map<uint64_t,std::string>& out,std::string& error){
    out.clear();
    for(const auto& row:state.monsters){std::string name;
        if(!read(row.pointer,name,error))return false;
        if(!row.uid||!out.emplace(row.uid,std::move(name)).second){error="Monster Remove name roster has duplicate full UID";return false;}}
    return true;
}
template<class Name>
bool unchanged_names(const std::map<uint64_t,std::string>& before,const monster_zone_read::State& after,
                     uint64_t removed,Name&& read,std::string& error){
    if(!before.contains(removed)||before.size()!=after.monsters.size()+1){error="Monster Remove before name roster differs";return false;}
    for(const auto& row:after.monsters){const auto old=before.find(row.uid);std::string current;
        if(row.uid==removed||old==before.end()||!read(row.pointer,current,error)||current!=old->second){
            if(error.empty())error="Monster Remove changed another Monster name";return false;}}
    return true;
}
inline bool removed_one(const monster_zone_read::State& a,const monster_zone_read::State& b,
                        uint64_t uid,std::string& error){
    if(!uid||a.manager!=b.manager||a.monster_manager!=b.monster_manager||a.zones!=b.zones||
       a.monsters.size()!=b.monsters.size()+1||a.next_monster_tag!=b.next_monster_tag||
       int64_t(b.created_monsters)-a.created_monsters!=1){
        error="Monster Remove manager, Zone, count or lifecycle counter differs";return false;}
    size_t after=0;unsigned found=0;
    for(const auto& row:a.monsters){
        if(row.uid==uid){++found;continue;}
        if(after>=b.monsters.size()||row!=b.monsters[after++]){
            error="Monster Remove changed another row or roster order";return false;}
    }
    if(found!=1||after!=b.monsters.size()||std::any_of(b.monsters.begin(),b.monsters.end(),
       [uid](const auto& row){return row.uid==uid;})){
        error="Monster Remove target is not uniquely absent";return false;}
    return true;
}
}

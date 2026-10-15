#pragma once
#include "monster_zone_read.h"
namespace mt2mp::game::monster_zone_effect {
namespace z=monster_zone_read;
inline bool created(const z::State& before,const z::State& after,uintptr_t pointer,uint64_t uid,
                    int32_t type,int32_t level,std::string& e){
    const auto fail=[&](const char* cause){e=cause;return false;};
    if(before.manager!=after.manager||before.monster_manager!=after.monster_manager||
       after.zones.size()!=before.zones.size()+1||after.monsters.size()<before.monsters.size())
        return fail("Zone original append owner/count differs");
    for(size_t i=0;i<before.zones.size();++i)if(before.zones[i]!=after.zones[i])return fail("Zone original action changed an existing Zone");
    for(size_t i=0;i<before.monsters.size();++i)if(before.monsters[i]!=after.monsters[i])return fail("Zone original action changed existing Monster order/identity/state");
    const auto& zone=after.zones.back();
    if(!pointer||!uid||zone.value.pointer!=pointer||zone.value.uid!=uid||zone.value.content.type!=1||
       zone.value.content.level!=level||zone.value.content.monster_type!=type)
        return fail("Zone original created identity/type/level differs");
    for(const auto& old:before.zones)if(old.value.pointer==pointer||old.value.uid==uid)return fail("Zone original reused an existing identity");
    const size_t count=after.monsters.size()-before.monsters.size();
    if(zone.monsters.size()!=count||before.region_random!=after.region_random||
       int64_t(after.next_monster_tag)-before.next_monster_tag!=int64_t(count)||
       int64_t(after.created_monsters)-before.created_monsters!=int64_t(count))return fail("Zone original spawn membership/counters/Region RNG differs");
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;
    for(const auto& old:before.monsters){pointers.insert(old.pointer);uids.insert(old.uid);}
    for(size_t i=0;i<count;++i){const auto& row=after.monsters[before.monsters.size()+i];
        if(!row.pointer||!row.uid||!pointers.insert(row.pointer).second||!uids.insert(row.uid).second||
           row.zone!=pointer||row.uid!=zone.monsters[i]||row.slot!=1||row.type!=type||row.level!=level||
           row.status!=3||row.rank!=0||int64_t(row.tag)!=int64_t(before.next_monster_tag)+int64_t(i))
            return fail("Zone original spawned member ordinal/identity/type/level/status differs");
    }
    e.clear();return true;
}
}

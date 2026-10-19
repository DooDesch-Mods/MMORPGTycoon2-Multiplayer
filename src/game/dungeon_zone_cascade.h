#pragma once
#include "npc_quest_targets.h"
#include <algorithm>
namespace mt2mp::game::dungeon_zone_cascade {
template<class Find>bool reconcile_restored(const npc_quest_targets::SavedPool& before,const npc_quest_targets::SavedPool& original,
    const npc_quest_targets::Array& actual,const std::set<uint64_t>& restored,Find&& find,npc_quest_targets::SavedPool& out,std::string& error){
    namespace q=npc_quest_targets;out={};
    if(actual.pointer!=before.array.pointer||original.array.pointer!=before.array.pointer||original.origin!=before.origin)
        return q::fail(error,"Dungeon cell history Zone original/current owned array or snapshot differs");
    std::set<uint64_t> live;std::set<uintptr_t> pointers;std::map<uintptr_t,std::string> keys;
    for(const auto& row:actual.zones){
        const auto prior=std::find_if(before.array.zones.begin(),before.array.zones.end(),[&](const auto& candidate){return candidate.uid==row.uid;});
        const auto saved=std::find_if(original.array.zones.begin(),original.array.zones.end(),[&](const auto& candidate){return candidate.uid==row.uid;});
        const bool same=prior!=before.array.zones.end()&&prior->pointer==row.pointer;
        if(!row.uid||!live.insert(row.uid).second||!pointers.insert(row.pointer).second||(!same&&(!restored.contains(row.uid)||saved==original.array.zones.end())))
            return q::fail(error,"Dungeon cell history Zone lacks its current member or exact original Make/Edit UID authority");
        uintptr_t found=0;if(!find(row.uid,found,error))return false;if(found!=row.pointer)return q::fail(error,"Dungeon cell history restored Zone full UID differs from actual pointer");
        keys.emplace(row.pointer,same?q::zone_key(before,*prior):q::zone_key(original,*saved));
    }
    for(const auto& row:before.array.zones)if(!live.contains(row.uid)){uintptr_t found=0;if(!find(row.uid,found,error))return false;
        if(found)return q::fail(error,"Dungeon cell history retired Zone remains registered");}
    for(const auto uid:restored)if(!live.contains(uid))return q::fail(error,"Dungeon original Make/Edit Zone did not restore its complete target UID");
    auto next=before;next.array=actual;next.action_keys=std::move(keys);out=std::move(next);error.clear();return true;
}
template<class Find>bool reconcile(const npc_quest_targets::SavedPool& before,const npc_quest_targets::Array& actual,Find&& find,
                                  npc_quest_targets::SavedPool& out,std::string& error){
    namespace q=npc_quest_targets;out={};if(actual.pointer!=before.array.pointer)return q::fail(error,"Dungeon cell Quest Zone cascade changed the original owned array");
    std::map<uintptr_t,std::string> keys;std::set<uintptr_t> live;std::set<uint64_t> uids;
    for(const auto& row:actual.zones){const auto prior=std::find_if(before.array.zones.begin(),before.array.zones.end(),[&](const auto& candidate){return candidate.pointer==row.pointer;});uintptr_t found=0;
        if(prior==before.array.zones.end()||prior->uid!=row.uid||prior->link_id!=row.link_id||!live.insert(row.pointer).second||!uids.insert(row.uid).second)
            return q::fail(error,"Dungeon cell surviving Quest Zone identity is new, replaced or repeated");
        if(!find(row.uid,found,error))return false;
        if(found!=row.pointer)return q::fail(error,"Dungeon cell surviving Quest Zone full UID is not registered to its actual pointer");
        keys.emplace(row.pointer,q::zone_key(before,*prior));
    }
    for(const auto& row:before.array.zones)if(!live.contains(row.pointer)){uintptr_t found=0;
        if(!find(row.uid,found,error))return false;
        if(found)return q::fail(error,"Dungeon cell removed Quest Zone remains registered");
    }
    auto next=before;next.array=actual;next.action_keys=std::move(keys);out=std::move(next);error.clear();return true;
}
}

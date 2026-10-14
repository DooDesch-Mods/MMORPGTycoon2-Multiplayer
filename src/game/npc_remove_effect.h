#pragma once
#include "npc_census.h"
#include <algorithm>

namespace mt2mp::game::npc_probe::remove_effect {
inline bool removed_one(const npc_census::State& before,const npc_census::State& after,
                        uint64_t target,std::string& error) {
    if(!target||before.manager!=after.manager||before.next_local_tag!=after.next_local_tag||
       before.rows.size()!=after.rows.size()+1) {
        error="NPC original DestroyProps manager, count or next tag differs";return false;}
    bool removed=false;
    for(const auto& row:before.rows) {
        if(row.local_uid==target){if(removed){error="NPC remove target UID duplicated";return false;}removed=true;continue;}
        const auto match=std::find_if(after.rows.begin(),after.rows.end(),[&](const auto& value){return value.local_uid==row.local_uid;});
        if(match==after.rows.end()||*match!=row){error="NPC original DestroyProps changed another actor";return false;}
    }
    if(!removed||std::any_of(after.rows.begin(),after.rows.end(),[&](const auto& row){return row.local_uid==target;})) {
        error="NPC target still belongs to native Manager";return false;}
    error.clear();return true;
}
}

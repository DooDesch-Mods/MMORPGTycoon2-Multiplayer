#pragma once
#include "npc_saved_identity.h"

namespace mt2mp::game::npc_history_identity {
// The enclosing runtime proves the exact pending history lease and original
// MakeNPC graph before admitting a fresh pointer for a retained saved key.
template<class Find>
bool restore(const std::string& sha,const std::string& key,npc_saved_identity::SavedPool& saved,
             const npc_census::State& census,uintptr_t previous,uint64_t uid,uintptr_t restored,
             Find&& find,std::string& error){
    using npc_saved_identity::fail;
    if(!previous||!restored||!uid||saved.pool.manager!=census.manager)
        return fail(error,"NPC history saved restore lacks its exact original owner/full UID");
    npc_saved_identity::Record* selected=nullptr;
    for(auto& record:saved.pool.records){
        if(npc_saved_identity::key(sha,saved.region,record)!=key)continue;
        if(selected||record.uid!=uid||record.pointer!=previous)return fail(error,"NPC history saved key/full UID/prior pointer differs");
        selected=&record;
    }
    if(!selected)return fail(error,"NPC history saved restore source key is absent");
    const npc_census::Row* current=nullptr;
    for(const auto& row:census.rows){
        if(row.local_uid==uid||row.local_pointer==restored){
            if(current||row.local_uid!=uid||row.local_pointer!=restored||row.type!=selected->type)
                return fail(error,"NPC history restored full UID/type/current pointer is ambiguous or partial");
            current=&row;
        }else if(row.local_pointer==previous)return fail(error,"NPC history old pointer now belongs to a different current NPC");
    }
    uintptr_t registered=0;
    if(!current)return fail(error,"NPC history restored NPC is absent from its complete Region census");
    if(!find(uid,registered,error))return false;
    if(registered!=restored)return fail(error,"NPC history restored full UID lookup differs from current owner");
    selected->pointer=restored;error.clear();return true;
}
}

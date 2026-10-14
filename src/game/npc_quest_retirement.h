#pragma once

#include "npc_quest_records.h"
#include <algorithm>

namespace mt2mp::npc_quest_retirement {

struct Presence {
    bool hash_present=false;
    bool array_present=false;
};

struct State {
    uintptr_t manager=0;
    int32_t manager_count=0;
    std::vector<Presence> quests;

    bool all_absent() const {
        for(const auto& quest:quests)
            if(quest.hash_present||quest.array_present)return false;
        return true;
    }
};

// Call on the game thread after OriginalDestroy returns. The saved Quest addresses
// are comparison tokens only; no read below accesses a retired Quest object.
// true means a complete read, not absence. Output is assigned only on success.
inline bool read(npc_census::Memory& memory,uintptr_t image,
                 const std::vector<uintptr_t>& before_quests,State& output,std::string& error){
    using npc_quest_records::get;
    using npc_quest_records::reject;
    error.clear();
    if(!image||image>UINTPTR_MAX-0x17020b0)
        return reject(error,"Quest retirement image invalid");
    State result;uintptr_t ap=0,entries=0;int32_t capacity=0;
    if(!get(memory,image,0x13df410,result.manager,"retirement manager singleton",error)||
       !get(memory,result.manager,0,ap,"retirement manager AP",error)||
       !get(memory,result.manager,0x30,entries,"retirement manager array",error)||
       !get(memory,result.manager,0x38,result.manager_count,"retirement manager count",error)||
       !get(memory,result.manager,0x3c,capacity,"retirement manager capacity",error))return false;
    if(ap!=image+0x17020b0||result.manager_count<0||capacity<result.manager_count||
       capacity>1000000||(result.manager_count&&!entries)||
       entries>UINTPTR_MAX-size_t(result.manager_count)*sizeof(uintptr_t))
        return reject(error,"Quest retirement manager class or active array header differs");
    std::vector<uintptr_t> active(size_t(result.manager_count),uintptr_t{0});
    if(!active.empty()&&!memory.copy(entries,active.data(),active.size()*sizeof(uintptr_t),error))
        return reject(error,"Quest retirement manager active array unreadable: "+
            (error.empty()?std::string("native memory copy failed without cause"):error));
    result.quests.reserve(before_quests.size());
    for(const uintptr_t quest:before_quests){
        if(!quest)return reject(error,"Quest retirement saved Quest address null");
        Presence presence;
        if(!npc_quest_records::manager_lookup(memory,image,result.manager,quest,presence.hash_present,error))
            return false;
        presence.array_present=std::find(active.begin(),active.end(),quest)!=active.end();
        result.quests.push_back(presence);
    }
    uintptr_t after_manager=0,after_entries=0;int32_t after_count=0,after_capacity=0;
    if(!get(memory,image,0x13df410,after_manager,"retirement manager reread",error)||
       !get(memory,result.manager,0x30,after_entries,"retirement manager array reread",error)||
       !get(memory,result.manager,0x38,after_count,"retirement manager count reread",error)||
       !get(memory,result.manager,0x3c,after_capacity,"retirement manager capacity reread",error))return false;
    if(after_manager!=result.manager||after_entries!=entries||after_count!=result.manager_count||
       after_capacity!=capacity)
        return reject(error,"Quest retirement manager or active array changed during readback");
    output=std::move(result);return true;
}

} // namespace mt2mp::npc_quest_retirement

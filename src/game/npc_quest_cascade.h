#pragma once
#include "quest_cascade_read.h"
#include <string_view>

namespace mt2mp::game::npc_quest_cascade {
namespace qr=monster_quest_retirement;
using json=nlohmann::json;

inline bool select(qr::Witness& before,uintptr_t target,uint64_t uid,std::string_view key,std::string& error){
    if(!target||!uid||key.empty())return qr::fail(error,"NPC Quest retirement target identity is absent");
    for(const auto& [pointer,owner]:before.owners){
        if((pointer==target)!=(owner.key==key)||(pointer==target&&owner.uid!=uid))
            return qr::fail(error,"NPC Quest retirement owner pointer/full UID/canonical key differs");
    }
    auto selected=before;
    if(!quest_cascade_read::select(selected,{{std::string(key),uid}},error))return false;
    const auto owner=selected.owners.find(target);
    // OnDestroy first retires incoming destinations in global order. The NPC
    // destructor increments its index while erasing from the remaining list,
    // then its array destructor deletes the skipped slots in forward order.
    std::vector<uintptr_t> owned;
    if(owner!=selected.owners.end())for(const auto& local:owner->second.state.local){
        if(std::find(selected.quests.begin(),selected.quests.end(),local.quest)==selected.quests.end())
            return qr::fail(error,"NPC owned Quest is absent from its complete global witness");
        if(std::find(selected.removed.begin(),selected.removed.end(),local.quest)==selected.removed.end())
            owned.push_back(local.quest);
    }
    for(size_t parity=0;parity<2;++parity)for(size_t slot=parity;slot<owned.size();slot+=2)
        selected.removed.push_back(owned[slot]);
    before.removed=std::move(selected.removed);error.clear();return true;
}

inline qr::Witness survivor_seed(const qr::Witness& before,std::string_view removed_owner){
    qr::Witness seed;
    for(const auto& [pointer,owner]:before.owners)if(owner.key!=removed_owner)seed.owners.emplace(pointer,owner);
    return seed;
}

inline bool survivors(const qr::Witness& before,const qr::Witness& after,std::string_view removed_owner,std::string& error){
    auto expected=before;
    for(auto it=expected.owners.begin();it!=expected.owners.end();){
        if(it->second.key==removed_owner)it=expected.owners.erase(it);else ++it;
    }
    for(const auto& [pointer,owner]:after.owners){(void)pointer;
        if(owner.key==removed_owner)return qr::fail(error,"Removed NPC remains a current Quest owner");
    }
    return qr::survivors(expected,after,error);
}

inline bool references(const qr::Witness& before,const json& old_rows,const json& new_rows,
                       std::string_view removed_owner,std::string& error)try{
    if(removed_owner.empty()||!old_rows.is_array()||!new_rows.is_array()||old_rows.size()!=new_rows.size())
        return qr::fail(error,"NPC Quest retirement Subscriber cohort or target key differs");
    auto comparable=new_rows;
    for(size_t i=0;i<old_rows.size();++i){
        const auto& old=old_rows.at(i).at("progress");auto& current=comparable.at(i).at("progress");
        if(!old.is_array()||!current.is_array()||old.size()!=current.size())
            return qr::fail(error,"NPC Quest retirement active progress count differs");
        for(size_t j=0;j<old.size();++j){const auto& link=old.at(j).at("npc");
            if(link.at("state")!=2||link.at("key").get_ref<const std::string&>()!=removed_owner)continue;
            const auto& expired=current.at(j).at("npc");
            if((expired.at("state")!=0&&expired.at("state")!=1)||expired.at("key")!="")
                return qr::fail(error,"Removed NPC retains a Live active progress reference");
            current.at(j)["npc"]=link;
        }
    }
    return quest_cascade_read::references(before,old_rows,comparable,error);
}catch(const json::exception& exception){
    return qr::fail(error,std::string("NPC Quest retirement active-reference payload: ")+exception.what());
}
}

#pragma once
#include "visual.h"
#include "../live_host_state/day_rollover/economy/monetary.h"
#include <set>
namespace mt2mp::game::toon_weapon_visual {
struct ToonOwner {int32_t id=-1;uintptr_t subscriber=0,toon=0;};
inline bool visual_fail(std::string& error,const char* cause){error=cause;return false;}
template<class T>bool visual_read(placement_ui_probe::Memory& memory,uintptr_t base,size_t offset,T& out,std::string& error){
    if(!base||base>UINTPTR_MAX-offset||base+offset>UINTPTR_MAX-sizeof out)return visual_fail(error,"Weapon authority address bounds differ");
    return memory.copy(base+offset,&out,sizeof out,error);
}
inline bool toon_census(placement_ui_probe::Memory& memory,uintptr_t image,std::vector<ToonOwner>& out,std::string& error){
    out.clear();uintptr_t manager=0,ap=0,data=0;int32_t count=0,capacity=0;
    if(!visual_read(memory,image,0x13df690,manager,error)||!visual_read(memory,manager,0,ap,error)||
       !visual_read(memory,manager,0x40,data,error)||!visual_read(memory,manager,0x48,count,error)||!visual_read(memory,manager,0x4c,capacity,error))return false;
    if(ap!=image+0x1725f40||count<0||capacity<count||count>1000000||(count&&!data))return visual_fail(error,"Weapon authority actual Subscriber manager differs");
    std::set<int32_t> ids;std::set<uintptr_t> receivers;
    for(int32_t i=0;i<count;++i){ToonOwner row;uintptr_t backlink=0;
        if(!visual_read(memory,data,size_t(i)*8,row.subscriber,error)||!visual_read(memory,row.subscriber,0,ap,error)||
           ap!=image+0x16e75f0)return visual_fail(error,"Weapon authority Subscriber member class differs");
        if(!visual_read(memory,row.subscriber,0x20,row.id,error)||!visual_read(memory,row.subscriber,0x80,row.toon,error))return false;
        if(row.id<0||!ids.insert(row.id).second||!receivers.insert(row.subscriber).second)return visual_fail(error,"Weapon authority Subscriber ID/receiver is ambiguous");
        if(row.toon){
            if(!visual_read(memory,row.toon,0,ap,error)||!visual_read(memory,row.toon,0x940,backlink,error))return false;
            if(ap!=image+0x1744e90||backlink!=row.subscriber||!receivers.insert(row.toon).second)return visual_fail(error,"Weapon authority reciprocal Toon owner differs");
        }
        out.push_back(row);
    }
    return true;
}
inline bool authoritative_tuple(placement_ui_probe::Memory& memory,const ToonOwner& owner,
    const live_host_state::day::economy::State& state,std::string& error){
    const mt2mp::subscriber_quest_state::Toon* target=nullptr;
    for(const auto& row:state.subscribers)if(row.serialized_id==owner.id){
        if(target||!row.quest_state)return visual_fail(error,"Weapon authority Toon state absent or ambiguous");
        target=&*row.quest_state;
    }
    if(!target)return visual_fail(error,"Weapon authority Subscriber absent from admitted Economy");
    int32_t ilvl=0,variant=0;uintptr_t bytes=0;uint64_t size=0;
    if(!visual_read(memory,owner.toon,0x4b0,ilvl,error)||!visual_read(memory,owner.toon,0x4d8,variant,error)||
       !visual_read(memory,owner.toon,0x4b8,bytes,error)||!visual_read(memory,owner.toon,0x4c0,size,error))return false;
    if(ilvl!=target->weapon_ilvl||variant!=target->weapon_variant_id||size!=target->weapon_category.size()||!bytes||size>65536)
        return visual_fail(error,"Weapon actual chosen tuple differs from admitted Economy");
    std::string category(size_t(size),'\0');char end=1;
    if((size&&!memory.copy(bytes,category.data(),category.size(),error))||!visual_read(memory,bytes,size_t(size),end,error))return false;
    return (!end&&category==target->weapon_category)||visual_fail(error,"Weapon actual category differs from admitted Economy");
}
}

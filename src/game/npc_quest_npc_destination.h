#pragma once
#include "npc_target_key.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace mt2mp::game::npc_quest_npc_destination {
struct Witness {
    uintptr_t object=0,region=0,proxy=0;
    uint64_t uid=0;
    std::string key;
    bool operator==(const Witness&) const=default;
};
inline bool fail(std::string& e,const char* cause){e=cause;return false;}
template<class Memory,class T>bool at(Memory& m,uintptr_t object,size_t offset,T& out,std::string& e){
    if(!object||object>UINTPTR_MAX-offset)return fail(e,"Quest NPC destination address is null or overflows");
    return m.copy(object+offset,&out,sizeof out,e);
}
template<class Memory>bool observe(Memory& m,uintptr_t image,uintptr_t object,Witness& out,std::string& e){
    uintptr_t ap=0,weak=0;Witness value;value.object=object;
    if(!at(m,object,0,ap,e)||!at(m,object,0x18,value.uid,e)||!at(m,object,0x178,value.region,e)||
       !at(m,object,0x28,value.proxy,e)||!at(m,value.proxy,0,weak,e))return false;
    if(ap!=image+0x1742980||!value.uid||!value.region||object>UINTPTR_MAX-0x20||weak!=object+0x20)
        return fail(e,"Quest NPC destination class, UID, Region or weak proxy differs");
    out=std::move(value);return true;
}
template<class Memory,class Resolve>bool bind(Memory& m,uintptr_t image,uintptr_t object,
                                             Resolve&& resolve,Witness& out,std::string& e){
    out={};Witness before,after;
    if(!observe(m,image,object,before,e)||!resolve(before.object,before.uid,before.region,before.key,e))return false;
    if(!npc_target_key::valid(before.key))return fail(e,"Quest NPC destination lacks canonical current NPC key");
    if(!observe(m,image,object,after,e))return false;
    after.key=before.key;
    if(after!=before)return fail(e,"Quest NPC destination receiver/UID/Region/proxy changed during binding");
    out=std::move(after);return true;
}
}

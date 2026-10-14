#pragma once

#include "npc_quest_records.h"
#include <cmath>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::npc_quest_visitor_reader {

enum class Kind { Npc, Quest };
enum class LinkState { Empty, Expired, Live };

struct Link {
    LinkState state=LinkState::Empty;
    std::string diagnostic_key;
};

struct Progress {
    Link npc;
    Link quest;
    int32_t completed=0;
    int32_t monsters_defeated=0;
    bool visited=false;
    bool action_performed=false;
    bool complete=false;
};

struct State {
    int32_t gold=0;
    float xp=0;
    int32_t level=0;
    int32_t quests_completed=0;
    std::vector<Progress> progress;
};

using IdentityCallback=std::function<bool(Kind,uintptr_t,std::string&,std::string&)>;

inline bool read_link(npc_census::Memory& memory,uintptr_t image,uintptr_t object,
                      size_t offset,uintptr_t expected_ap,Kind kind,
                      const IdentityCallback& identity,Link& out,std::string& error){
    uintptr_t ap=0,target=0,proxy=0;
    if(!npc_quest_records::get(memory,object,offset,ap,"visitor weak AP",error)||
       !npc_quest_records::get(memory,object,offset+8,target,"visitor weak target",error)||
       !npc_quest_records::get(memory,object,offset+16,proxy,"visitor weak proxy",error))return false;
    if(ap!=image+expected_ap)
        return npc_quest_records::reject(error,"Quest visitor weak link AP differs");
    if(!proxy){
        if(target)return npc_quest_records::reject(error,"Quest visitor weak link target without proxy");
        out.state=LinkState::Empty;
        return true;
    }
    uintptr_t weak_target=0;
    if(!npc_quest_records::get(memory,proxy,0,weak_target,"visitor weak proxy value",error))return false;
    if(!weak_target){
        out.state=LinkState::Expired;
        return true;
    }
    const size_t weak_base=kind==Kind::Npc?0x20:0x18;
    const size_t proxy_offset=kind==Kind::Npc?0x28:0x20;
    if(!target||target>UINTPTR_MAX-proxy_offset||weak_target!=target+weak_base)
        return npc_quest_records::reject(error,"Quest visitor live weak target/proxy differs");
    uintptr_t target_ap=0,target_proxy=0;
    if(!npc_quest_records::get(memory,target,0,target_ap,"visitor live target AP",error)||
       !npc_quest_records::get(memory,target,proxy_offset,target_proxy,"visitor live target proxy",error))return false;
    if(target_ap!=image+(kind==Kind::Npc?0x1742980:0x174ac10)||target_proxy!=proxy)
        return npc_quest_records::reject(error,"Quest visitor live target AP or reciprocal proxy differs");
    out.state=LinkState::Live;
    if(!identity||!identity(kind,target,out.diagnostic_key,error)){
        if(error.empty())error="Quest visitor live identity callback failed";
        return false;
    }
    if(out.diagnostic_key.empty()||out.diagnostic_key.find('\0')!=std::string::npos||
       !npc_census::utf8(out.diagnostic_key))
        return npc_quest_records::reject(error,"Quest visitor live identity key empty or invalid UTF-8");
    return true;
}

struct ArrayHeader { uintptr_t ap=0,data=0; int32_t count=0,capacity=0; };

inline bool read_header(npc_census::Memory& memory,uintptr_t toon,ArrayHeader& out,
                        std::string& error){
    return npc_quest_records::get(memory,toon,0xaf0,out.ap,"visitor progress array AP",error)&&
           npc_quest_records::get(memory,toon,0xaf8,out.data,"visitor progress data",error)&&
           npc_quest_records::get(memory,toon,0xb00,out.count,"visitor progress count",error)&&
           npc_quest_records::get(memory,toon,0xb04,out.capacity,"visitor progress capacity",error);
}

inline bool read(npc_census::Memory& memory,uintptr_t image,uintptr_t subscriber,
                 const IdentityCallback& identity,State& out,std::string& error){
    uintptr_t toon=0,ap=0,backlink=0,subscriber_ap=0;
    int32_t subscriber_id=-1;
    if(!image||image>UINTPTR_MAX-0x174ac10||!subscriber||!identity)
        return npc_quest_records::reject(error,"Quest visitor image, subscriber or identity absent");
    if(!npc_quest_records::get(memory,subscriber,0,subscriber_ap,"subscriber AP",error)||
       !npc_quest_records::get(memory,subscriber,0x20,subscriber_id,"subscriber ID",error)||
       !npc_quest_records::get(memory,subscriber,0x80,toon,"subscriber Toon",error)||
       !npc_quest_records::get(memory,toon,0,ap,"Toon AP",error)||
       !npc_quest_records::get(memory,toon,0x940,backlink,"Toon subscriber backlink",error))return false;
    if(subscriber_ap!=image+0x16e75f0||subscriber_id<0||
       ap!=image+0x1744e90||backlink!=subscriber)
        return npc_quest_records::reject(error,"Quest visitor subscriber/Toon AP, ID or backlink differs");
    ArrayHeader before,after;
    if(!read_header(memory,toon,before,error))return false;
    if(before.ap!=image+0x16ee0e0||before.count<0||before.capacity<before.count||
       before.capacity>100000||(before.count&&!before.data)||
       before.data>UINTPTR_MAX-size_t(before.count)*sizeof(uintptr_t))
        return npc_quest_records::reject(error,"Quest visitor array AP, pointer, count or capacity differs");
    State next;
    if(!npc_quest_records::get(memory,toon,0x850,next.gold,"Toon gold",error)||
       !npc_quest_records::get(memory,toon,0x820,next.xp,"Toon XP",error)||
       !npc_quest_records::get(memory,toon,0x6f8,next.level,"Toon level",error)||
       !npc_quest_records::get(memory,toon,0x874,next.quests_completed,"Toon completed quests",error))return false;
    if(!std::isfinite(next.xp)||next.level<0||next.quests_completed<0)
        return npc_quest_records::reject(error,"Quest visitor Toon scalar domain differs");
    std::set<uintptr_t> progress_objects;
    std::vector<uintptr_t> pointers;
    std::set<std::string> live_npc_keys;
    next.progress.reserve(size_t(before.count));
    for(int32_t i=0;i<before.count;++i){
        uintptr_t progress=0,progress_ap=0,instance_ap=0;
        if(!npc_quest_records::get(memory,before.data,size_t(i)*sizeof(uintptr_t),
                                   progress,"visitor progress pointer",error)||
           !npc_quest_records::get(memory,progress,0,progress_ap,"visitor progress AP",error))return false;
        if(progress_ap!=image+0x172d130||!progress_objects.insert(progress).second||
           progress>UINTPTR_MAX-0x58)
            return npc_quest_records::reject(error,"Quest visitor progress AP, pointer or duplicate differs");
        pointers.push_back(progress);
        Progress row;
        if(!read_link(memory,image,progress,0x38,0x16f9de0,Kind::Npc,identity,row.npc,error)||
           !npc_quest_records::get(memory,progress,0x50,row.completed,"visitor completed count",error)||
           !npc_quest_records::get(memory,progress+0x58,0,instance_ap,"visitor instance AP",error))return false;
        if(instance_ap!=image+0x17087e0)
            return npc_quest_records::reject(error,"Quest visitor instance AP differs subscriber="+
                std::to_string(subscriber_id)+" row="+std::to_string(i)+" actual="+
                std::to_string(instance_ap)+" expected="+std::to_string(image+0x17087e0)+
                " completed="+std::to_string(row.completed));
        // FindNearbyNewQuestGiver publishes -1 before CollectQuest selects quest 0.
        if(row.completed<-1)
            return npc_quest_records::reject(error,"Quest visitor completed count below discovery sentinel subscriber="+
                std::to_string(subscriber_id)+" row="+std::to_string(i)+" actual="+
                std::to_string(row.completed)+" minimum=-1");
        if(!read_link(memory,image,progress+0x58,0x38,0x16f9f20,Kind::Quest,identity,row.quest,error)||
           !npc_quest_records::get(memory,progress+0x58,0x50,row.monsters_defeated,"visitor defeated",error))return false;
        uint8_t visited=2,action=2,complete=2;
        if(!npc_quest_records::get(memory,progress+0x58,0x54,visited,"visitor visited",error)||
           !npc_quest_records::get(memory,progress+0x58,0x55,action,"visitor action",error)||
           !npc_quest_records::get(memory,progress+0x58,0x56,complete,"visitor complete",error))return false;
        if(row.monsters_defeated<0||visited>1||action>1||complete>1)
            return npc_quest_records::reject(error,"Quest visitor instance count or boolean differs");
        row.visited=visited!=0;row.action_performed=action!=0;row.complete=complete!=0;
        if(row.npc.state==LinkState::Live&&
           !live_npc_keys.insert(row.npc.diagnostic_key).second)
            return npc_quest_records::reject(error,"Quest visitor live NPC identity duplicated");
        next.progress.push_back(std::move(row));
    }
    if(!read_header(memory,toon,after,error))return false;
    if(before.ap!=after.ap||
       before.data!=after.data||before.count!=after.count||before.capacity!=after.capacity)
        return npc_quest_records::reject(error,"Quest visitor array header changed during read");
    for(int32_t i=0;i<before.count;++i){
        uintptr_t pointer=0;
        if(!npc_quest_records::get(memory,before.data,size_t(i)*sizeof(uintptr_t),
                                   pointer,"final visitor progress pointer",error))return false;
        if(pointer!=pointers[size_t(i)])
            return npc_quest_records::reject(error,"Quest visitor progress pointer changed during read");
    }
    uintptr_t final_toon=0,final_backlink=0;
    if(!npc_quest_records::get(memory,subscriber,0x80,final_toon,"final subscriber Toon",error)||
       !npc_quest_records::get(memory,toon,0x940,final_backlink,"final Toon backlink",error))return false;
    if(final_toon!=toon||final_backlink!=subscriber)
        return npc_quest_records::reject(error,"Quest visitor subscriber/Toon binding changed during read");
    out=std::move(next);error.clear();return true;
}

} // namespace mt2mp::npc_quest_visitor_reader

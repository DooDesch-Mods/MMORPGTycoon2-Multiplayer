#pragma once
#include "npc_quest_targets.h"
#include "duplicate_transition.h"
#include <cstring>
#include <set>

namespace mt2mp::game::scenery_make_history {
struct Saved {
    uintptr_t object=0,root=0,region=0;uint64_t uid=0;
    std::array<int32_t,2> type{};
    std::array<uint8_t,0x6c> transform{};
    uint32_t height=0;uint8_t automatic=0;
    bool operator==(const Saved&)const=default;
};
template<class Memory>bool uid_set(Memory& memory,uintptr_t owner,uint64_t singleton,std::string& e){
    namespace q=npc_quest_targets;uintptr_t table=0,values=0;int32_t buckets=0,shift=-1,count=-1,capacity=-1;
    if(!q::at(memory,owner,0,table,e)||!q::at(memory,owner,8,buckets,e)||!q::at(memory,owner,0xc,shift,e)||
       !q::at(memory,owner,0x18,values,e)||!q::at(memory,owner,0x20,count,e)||!q::at(memory,owner,0x24,capacity,e))return false;
    if(!table||!values||buckets<=0||buckets>1000000||(buckets&(buckets-1))||shift<0||shift>31||
       uint64_t(buckets)!=(uint64_t(1)<<(32-shift))||count!=(singleton?1:0)||capacity<count||capacity>1000000||table>UINTPTR_MAX-size_t(buckets)*24)
        return q::fail(e,"Scenery original dependency set header/count differs");
    if(singleton){uint64_t value=0;if(!q::at(memory,values,0,value,e))return false;
        if(value!=singleton)return q::fail(e,"Scenery original dependency vector full UID differs");}
    size_t found=0;std::set<uintptr_t> seen;
    for(int32_t bucket=0;bucket<buckets;++bucket){uintptr_t link=0;if(!q::at(memory,table,size_t(bucket)*24+0x10,link,e))return false;
        while(link){uint64_t uid=0;uint32_t hash=0;uintptr_t next=0;
            if(!singleton||!seen.insert(link).second||seen.size()>1)return q::fail(e,"Scenery original dependency hash repeats or has an extra member");
            if(!q::at(memory,link,0,uid,e)||!q::at(memory,link,8,hash,e)||!q::at(memory,link,0x10,next,e))return false;
            if(uid!=singleton||hash!=uint32_t(uid)||int32_t((hash*0x9e3779ffu)>>shift)!=bucket)return q::fail(e,"Scenery original dependency hash/full UID/bucket differs");
            ++found;link=next;
        }
    }
    if(found!=(singleton?1u:0u))return q::fail(e,"Scenery original dependency hash lacks its exact expected set");
    uintptr_t final_table=0,final_values=0;int32_t final_buckets=0,final_shift=0,final_count=0,final_capacity=0;
    if(!q::at(memory,owner,0,final_table,e)||!q::at(memory,owner,8,final_buckets,e)||!q::at(memory,owner,0xc,final_shift,e)||
       !q::at(memory,owner,0x18,final_values,e)||!q::at(memory,owner,0x20,final_count,e)||!q::at(memory,owner,0x24,final_capacity,e))return false;
    if(singleton){uint64_t value=0;if(!q::at(memory,values,0,value,e))return false;
        if(value!=singleton)return q::fail(e,"Scenery original dependency vector changed during complete read");}
    return (table==final_table&&values==final_values&&buckets==final_buckets&&shift==final_shift&&count==final_count&&capacity==final_capacity)||
        q::fail(e,"Scenery original dependency set changed during complete read");
}
template<class Memory,class Find>bool live(Memory& memory,uintptr_t image,uintptr_t object,uint64_t uid,uintptr_t root,uintptr_t region,
                                          Find&& find,Saved& out,std::string& e){
    namespace q=npc_quest_targets;out={};uintptr_t found=0;uint64_t full=0;
    if(!uid||!object||!root||!region||!q::typed(memory,image,object,0x16c1390,e)||!q::at(memory,object,0x18,full,e)||full!=uid||
       !find(uid,found,e)||found!=object)return q::fail(e,e.empty()?"Scenery history full current UID/registry differs":e);
    out.object=object;out.uid=uid;
    if(!q::at(memory,object,0x170,out.root,e)||!q::at(memory,object,0x178,out.region,e)||!q::at(memory,object,0x268,out.type,e)||
       !q::at(memory,object,0x100,out.transform,e)||!q::at(memory,object,0x270,out.height,e)||!q::at(memory,object,0x275,out.automatic,e))return false;
    std::array<uint32_t,10> pose{};std::memcpy(pose.data(),out.transform.data(),sizeof pose);
    if(out.root!=root||out.region!=region||out.type[0]<0||out.type[0]>=47||out.automatic>1||!std::isfinite(std::bit_cast<float>(out.height))||
       !duplicate_transition::pose(pose))return q::fail(e,"Scenery history current original stored inputs differ");
    return true;
}
template<class Memory>bool make(Memory& memory,uintptr_t image,uintptr_t node,const Saved& expected,std::string& e){
    namespace q=npc_quest_targets;Saved stored;uintptr_t method=0;
    if(!expected.uid||!q::typed(memory,image,node,0x1706590,e)||!q::at(memory,image+0x1706590,0x10,method,e)||method!=image+0x55a3c0||
       !q::at(memory,node,0xb8,stored.uid,e)||!q::at(memory,node,0xc0,stored.type,e)||!q::at(memory,node,0xc8,stored.root,e)||
       !q::at(memory,node,0xd0,stored.transform,e)||!q::at(memory,node,0x13c,stored.height,e)||!q::at(memory,node,0x140,stored.automatic,e))
        return q::fail(e,e.empty()?"Scenery original Make node class/method differs":e);
    if(stored.uid!=expected.uid||stored.type!=expected.type||stored.root!=expected.root||stored.transform!=expected.transform||
       stored.height!=expected.height||stored.automatic!=expected.automatic)return q::fail(e,"Scenery original Make full stored UID/type/root/transform/height differs");
    return uid_set(memory,node+8,expected.uid,e)&&uid_set(memory,node+0x30,0,e)&&uid_set(memory,node+0x58,0,e);
}
template<class Memory>bool destroy(Memory& memory,uintptr_t image,uintptr_t node,uint64_t expected,std::string& e){
    namespace q=npc_quest_targets;uintptr_t method=0;uint64_t uid=0;
    if(!expected||!q::typed(memory,image,node,0x16de860,e)||!q::at(memory,image+0x16de860,0x10,method,e)||method!=image+0x53e1c0||
       !q::at(memory,node,0xb8,uid,e)||uid!=expected)return q::fail(e,e.empty()?"Scenery original Destroy node class/method/full UID differs":e);
    return uid_set(memory,node+8,0,e)&&uid_set(memory,node+0x30,0,e)&&uid_set(memory,node+0x58,expected,e);
}
}

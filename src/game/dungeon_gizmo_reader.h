#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mt2mp::game::dungeon_gizmo_reader {
struct Record {
    std::string name,type,variant,parent;
    std::array<float,4> rotation{};std::array<float,3> position{},scale{};
    bool triggered=false;int32_t state=0,gold=0,loot=0,vendor_trash=0,keys=0,boss_keys=0,lock_id=0,edge_a=-1,edge_b=-1;
    bool operator==(const Record&)const=default;
};
struct Member {uintptr_t object=0,parent=0,definition=0;uint64_t uid=0,parent_uid=0;bool operator==(const Member&)const=default;};
struct Local {uintptr_t manager=0,shard=0,region=0,data=0;int32_t count=0,capacity=0;uint32_t mutation_count=0;std::vector<Member> members;bool operator==(const Local&)const=default;};

// Parent keys are resolved from actual current scene ownership and full UID. Native pointers and UIDs remain local witnesses.
template<class Read,class Text,class Find,class Parent>
bool read(Read& at,Text& text,Find& find,Parent& parent_key,uintptr_t image,uintptr_t manager,uintptr_t shard,uintptr_t region,std::vector<Record>& output,Local& local,std::string& error){
    output.clear();local={};Local owner;owner.manager=manager;owner.shard=shard;owner.region=region;uintptr_t ap=0,reverse_shard=0,reverse_region=0;
    if(!at(region,0x9a8,owner.manager,error)||!at(manager,0,ap,error)||!at(manager,0x68,reverse_shard,error)||!at(manager,0x70,reverse_region,error)||
       !at(manager,0x20,owner.data,error)||!at(manager,0x28,owner.count,error)||!at(manager,0x2c,owner.capacity,error)||!at(manager,0x78,owner.mutation_count,error))return false;
    if(owner.manager!=manager||ap!=image+0x1700260||reverse_shard!=shard||reverse_region!=region){error="Dungeon Gizmo manager class/reciprocal scene owner differs";return false;}
    if(!at(manager,0x18,ap,error))return false;
    if(ap!=image+0x172e300){error="Dungeon Gizmo actual saved volatile-array addresspoint differs";return false;}
    if(owner.count<0||owner.capacity<owner.count||(owner.count&&(!owner.data||owner.data>UINTPTR_MAX-size_t(owner.count)*8))){error="Dungeon Gizmo volatile owner roster/span differs";return false;}
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;std::vector<Record> records;
    for(int32_t i=0;i<owner.count;++i){Member member;Record record;uintptr_t actual=0,actor_shard=0,actor_region=0;uint8_t triggered=2;
        if(!at(owner.data,size_t(i)*8,member.object,error))return false;
        if(!member.object||!pointers.insert(member.object).second){error="Dungeon Gizmo roster has an absent or duplicate native member";return false;}
        if(!at(member.object,0,ap,error)||!at(member.object,0x18,member.uid,error)||!at(member.object,0x170,actor_shard,error)||!at(member.object,0x178,actor_region,error)||
           !at(member.object,0x258,member.parent,error)||!at(member.object,0x278,member.definition,error))return false;
        if(ap!=image+0x1749ab0||!member.uid||!uids.insert(member.uid).second||actor_shard!=shard||actor_region!=region){error="Dungeon Gizmo native class/full UID/scene membership differs";return false;}
        if(!find(member.uid,actual,error))return false;if(actual!=member.object){error="Dungeon Gizmo original UID registry resolves a different native member";return false;}
        if(!text(member.object,0x38,record.name,error)||!text(member.object,0x210,record.type,error)||!text(member.object,0x230,record.variant,error)||
           !at(member.object,0x100,record.rotation,error)||!at(member.object,0x110,record.position,error)||!at(member.object,0x11c,record.scale,error)||
           !at(member.object,0x250,triggered,error)||!at(member.object,0x254,record.state,error)||!at(member.object,0x260,record.gold,error)||!at(member.object,0x264,record.loot,error)||
           !at(member.object,0x268,record.vendor_trash,error)||!at(member.object,0x26c,record.keys,error)||!at(member.object,0x270,record.boss_keys,error)||
           !at(member.object,0x330,record.lock_id,error)||!at(member.object,0x334,record.edge_a,error)||!at(member.object,0x338,record.edge_b,error))return false;
        if(triggered>1){error="Dungeon Gizmo authored trigger flag is not boolean";return false;}record.triggered=triggered!=0;
        for(float x:record.rotation)if(!std::isfinite(x)){error="Dungeon Gizmo native rotation is non-finite";return false;}
        for(float x:record.position)if(!std::isfinite(x)){error="Dungeon Gizmo native position is non-finite";return false;}
        for(float x:record.scale)if(!std::isfinite(x)){error="Dungeon Gizmo native scale is non-finite";return false;}
        if(member.parent){
            if(!at(member.parent,0,ap,error)||!at(member.parent,0x18,member.parent_uid,error))return false;
            if(member.parent==member.object||ap!=image+0x1749ab0||!member.parent_uid){error="Dungeon Gizmo prototype parent class/full UID differs";return false;}
            if(!find(member.parent_uid,actual,error))return false;if(actual!=member.parent){error="Dungeon Gizmo prototype parent is absent from original UID registry";return false;}
            if(!parent_key(member.parent,member.parent_uid,record.parent,error))return false;
            if(record.parent.empty()){error="Dungeon Gizmo prototype parent has no current canonical scene identity";return false;}
        }
        owner.members.push_back(member);records.push_back(std::move(record));
    }
    uintptr_t data=0,current_manager=0;int32_t count=0,capacity=0;uint32_t mutations=0;
    if(!at(manager,0x20,data,error)||!at(manager,0x28,count,error)||!at(manager,0x2c,capacity,error)||!at(region,0x9a8,current_manager,error)||
       !at(manager,0x68,reverse_shard,error)||!at(manager,0x70,reverse_region,error)||!at(manager,0x78,mutations,error))return false;
    if(data!=owner.data||count!=owner.count||capacity!=owner.capacity||current_manager!=manager||reverse_shard!=shard||reverse_region!=region||mutations!=owner.mutation_count){error="Dungeon Gizmo manager roster/owner changed during read";return false;}
    for(size_t i=0;i<owner.members.size();++i){const auto& before=owner.members[i];uintptr_t object=0,parent=0,definition=0;uint64_t uid=0;
        if(!at(data,i*8,object,error)||!at(before.object,0x18,uid,error)||!at(before.object,0x258,parent,error)||!at(before.object,0x278,definition,error))return false;
        if(object!=before.object||uid!=before.uid||parent!=before.parent||definition!=before.definition){error="Dungeon Gizmo member full UID/prototype/definition changed during read";return false;}}
    output=std::move(records);local=std::move(owner);return true;
}
}

#pragma once
#include "typed_action/dungeon_config_action.h"
#include <set>
namespace mt2mp::game::dungeon_config_reader {
struct Local {uintptr_t exterior=0,dungeon=0,data=0;uint64_t uid=0;int32_t count=0,capacity=0;std::vector<std::array<uintptr_t,4>> instances;bool operator==(const Local&)const=default;};
template<class Read>bool read(Read& at,uintptr_t image,uintptr_t root,uintptr_t exterior,uint64_t uid,action_session::DungeonConfigState& state,Local& local,std::string& error){
    local={};state={};uintptr_t ap=0,back=0,shard=0;uint64_t actual_uid=0;
    if(!at(exterior,0,ap,error)||ap!=image+0x16d93c0||!at(exterior,0x18,actual_uid,error)||actual_uid!=uid||!uid||!at(exterior,0x170,shard,error)||shard!=root||!at(exterior,0xa38,local.dungeon,error)||!local.dungeon||!at(local.dungeon,0,ap,error)||ap!=image+0x16c0560||!at(local.dungeon,0x6f8,back,error)||back!=exterior){error="Dungeon config current root exterior/full UID/reciprocal owner differs";return false;}
    local.exterior=exterior;local.uid=uid;
    if(!at(local.dungeon,0x40c,state.desired,error)||state.desired<0||!at(local.dungeon,0x4c0,local.data,error)||!at(local.dungeon,0x4c8,local.count,error)||!at(local.dungeon,0x4cc,local.capacity,error)||local.count<0||local.count>10000||local.capacity<local.count||(local.count&&!local.data)){error="Dungeon config actual desired count/instance array differs";return false;}
    std::set<uintptr_t> pointers;std::set<int32_t> indices;
    for(int32_t i=0;i<local.count;++i){uintptr_t instance=0,owner=0,region=0,region_shard=0,instance_shard=0;int32_t index=-1;uint8_t started=2;
        if(!at(local.data,size_t(i)*8,instance,error)||!instance||!pointers.insert(instance).second||!at(instance,0,ap,error)||ap!=image+0x1719450||!at(instance,0x18,index,error)||index<0||!indices.insert(index).second||!at(instance,0x20,started,error)||started>1||!at(instance,0x30,owner,error)||owner!=local.dungeon||!at(instance,0x38,instance_shard,error)||!instance_shard||!at(instance,0x40,region,error)||!region||!at(region,0,ap,error)||ap!=image+0x1787930||!at(region,0x70,region_shard,error)||region_shard!=instance_shard||!at(instance_shard,0x20,owner,error)||owner!=instance){error="Dungeon config instance saved index/owner/Shard/Region reciprocity differs";return false;}
        local.instances.push_back({instance,instance_shard,region,uintptr_t(index)});state.instances.push_back({index,started!=0});}
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!at(local.dungeon,0x4c0,data,error)||data!=local.data||!at(local.dungeon,0x4c8,count,error)||count!=local.count||!at(local.dungeon,0x4cc,capacity,error)||capacity!=local.capacity){error="Dungeon config instance array changed during read";return false;}return true;
}
}

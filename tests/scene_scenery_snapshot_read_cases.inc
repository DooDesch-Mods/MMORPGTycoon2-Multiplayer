#pragma once
#include "object_identity/remove_runtime/copy/scenery_witness.h"
#include <map>
namespace scene_scenery_read_fixture {
namespace p=mt2mp::build_actions::private_native::place;
inline constexpr uintptr_t image=0x140000000,region=0x10000,shard=0x20000,manager=0x30000,object=0x50000;
inline constexpr uint64_t uid=(uint64_t(9)<<40)+77;
struct Memory:mt2mp::build_actions::private_native::Memory {
    std::map<uintptr_t,uint8_t> bytes;unsigned transforms=0;bool change_pose=false;
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const uint8_t*>(&value);for(size_t n=0;n<sizeof value;++n)bytes[address+n]=data[n];}
    bool copy(uintptr_t address,void* out,size_t count,std::string& error)override{
        auto* target=static_cast<uint8_t*>(out);for(size_t n=0;n<count;++n){const auto it=bytes.find(address+n);if(it==bytes.end()){error="fixture exact Scenery field is absent";return false;}target[n]=it->second;}
        if(address==object+0x100&&count==sizeof(p::Transform)&&++transforms==2&&change_pose)static_cast<p::Transform*>(out)->position.x=2;return true;
    }
    Memory(){put(region+0x960,manager);put(manager,image+0x17152d0);put(manager+0x900,region);put(manager+0x908,shard);
        for(size_t category=0;category<47;++category){const auto address=manager+0x18+category*0x30;put(address,image+0x172d6a0);put(address+8,category==0?uintptr_t(0x40000):uintptr_t(0));put(address+0x10,int32_t(category==0));put(address+0x14,int32_t(category==0));}
        put(0x40000,object);put(object,image+0x16c1390);put(object+0x18,uid);put(object+0x170,shard);put(object+0x178,region);put(object+0x268,int32_t(0));
        p::Box box;box.valid=1;box.max={1,1,1};put(object+0x90,box);p::Transform pose;pose.rotation={0,0,0,1};pose.scale={1,1,1};pose.dirty=1;put(object+0x100,pose);
    }
};
struct Host:p::SceneryHost {
    p::SceneryContext context{0,region,shard,manager,true,true,true,true};unsigned reads=0,lookups=0;bool replace=false;
    bool current(p::SceneryContext& out,std::string&)override{out=context;if(++reads==2&&replace)++out.manager;return true;}
    bool registry_object(uint64_t identity,uintptr_t& out,std::string&)override{++lookups;out=identity==uid?object:0;return true;}
};
inline unsigned geometry_calls=0;
inline void geometry_construct(void*,const p::Box*,const p::Transform*){++geometry_calls;}
inline bool geometry_intersects(const void*,const void*){++geometry_calls;return true;}
template<class Check>void run(Check&& check){std::string error;Memory memory;Host host;p::SceneryWitness result;
    check(p::read_scenery(memory,host,image,result,error)&&result.context.epoch==0&&result.context.snapshot_observation&&result.members.size()==1&&result.members[0].uid==uid&&host.reads==4&&host.lookups==2,error);
    p::GeometryApi geometry{geometry_construct,geometry_intersects};geometry_calls=0;
    auto hit=p::overlaps(geometry,result,result.members[0].bounds,result.members[0].transform);
    check(!hit.complete&&hit.error=="scenery_geometry_context_guard"&&geometry_calls==0,"Snapshot-only zero-epoch proof reached a native geometry call");
    result.context.epoch=1;hit=p::overlaps(geometry,result,result.members[0].bounds,result.members[0].transform);
    check(!hit.complete&&geometry_calls==0,"Snapshot marker enabled geometry after an unrelated epoch store");
    memory=Memory{};host=Host{};host.context.snapshot_observation=false;
    check(!p::read_scenery(memory,host,image,result,error)&&error=="scenery_world_guard","Ordinary live read accepted zero epoch without snapshot proof");
    memory=Memory{};host=Host{};host.context.epoch=1;host.context.snapshot_observation=false;
    check(p::read_scenery(memory,host,image,result,error),error);hit=p::overlaps(geometry,result,result.members[0].bounds,result.members[0].transform);
    check(hit.complete&&hit.tested==1&&geometry_calls==3,"Original live geometry changed for a normal positive epoch witness");
    memory=Memory{};host=Host{};memory.change_pose=true;
    check(!p::read_scenery(memory,host,image,result,error)&&error=="scenery_changed_during_read","Snapshot read accepted changed actual pose between full captures");
    memory=Memory{};host=Host{};host.replace=true;
    check(!p::read_scenery(memory,host,image,result,error)&&error=="scenery_world_changed_during_read","Snapshot read accepted replaced actual manager owner");
    memory=Memory{};host=Host{};memory.bytes.erase(manager+0x18+46*0x30);
    check(!p::read_scenery(memory,host,image,result,error)&&error=="fixture exact Scenery field is absent","Snapshot read omitted final category or suppressed its cause");
}
}

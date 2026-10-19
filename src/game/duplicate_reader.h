#pragma once
#include "duplicate_native.h"
#include "duplicate_transition.h"
#include "group_reader.h"
#include <map>
namespace mt2mp::game::duplicate_reader {
namespace q=npc_quest_targets;
namespace a=action_session;
using Handle=duplicate_native::Handle;
struct Object {Handle handle;uintptr_t region=0,parent=0;int32_t region_id=-1; a::DuplicateFamily family{};std::array<uint32_t,10> pose{};bool operator==(const Object&)const=default;};
struct Census {std::map<uintptr_t,Object> objects;std::vector<uintptr_t> regions;bool operator==(const Census&)const=default;};
// Enumerate actual manager membership, including newly created objects that
// have not yet entered the shared registry. A registry-only census misses them.
template<class Memory,class Find>bool census(Memory& m,uintptr_t image,uintptr_t map,uintptr_t root,Find&& find,Census& out,std::string& e){
    out={};q::Bounds regions;if(!q::bounds(m,map+0x350,100000,regions,e))return false;
    std::set<uint64_t> uids;std::set<int32_t> region_ids;
    auto object=[&](uintptr_t p,uintptr_t region,int32_t id,a::DuplicateFamily family){Object row;row.handle.object=p;row.region=region;row.region_id=id;row.family=family;
        uintptr_t shard=0,actual_region=0,found=0;const uintptr_t ap=family==a::DuplicateFamily::Building?0x16d93c0:0x16c1390;
        if(!q::typed(m,image,p,ap,e)||!q::at(m,p,0x18,row.handle.uid,e)||!q::at(m,p,0x170,shard,e)||!q::at(m,p,0x178,actual_region,e)||
           !q::at(m,p,0x180,row.parent,e)||!group_reader::pose(m,p,0x100,row.pose,e)||!find(row.handle.uid,found,e))return false;
        if(!row.handle.uid||found!=p||shard!=root||actual_region!=region||!uids.insert(row.handle.uid).second||!out.objects.emplace(p,row).second)
            return q::fail(e,"Duplicate complete manager class/full UID/root/Region ownership differs");return true;};
    for(int32_t n=0;n<regions.count;++n){uintptr_t region=0,shard=0,buildings=0,scenery=0,reverse=0;int32_t id=-1;
        if(!q::at(m,regions.data,size_t(n)*8,region,e)||!q::typed(m,image,region,0x1787930,e)||!q::at(m,region,0x70,shard,e)||
           !q::at(m,region,0x834,id,e)||!q::at(m,region,0x950,buildings,e)||!q::at(m,region,0x960,scenery,e))return false;
        if(shard!=root||id<0||!region_ids.insert(id).second)return q::fail(e,"Duplicate complete root Region census differs");out.regions.push_back(region);
        if(!q::typed(m,image,buildings,0x17183c0,e)||!q::at(m,buildings,0x450,reverse,e)||reverse!=root||
           !q::at(m,buildings,0x458,reverse,e)||reverse!=region||!q::typed(m,image,buildings+0x408,0x172d7c0,e))return q::fail(e,e.empty()?"Duplicate Building manager reciprocity differs":e);
        q::Bounds list;if(!q::bounds(m,buildings+0x408,100000,list,e))return false;
        for(int32_t i=0;i<list.count;++i){uintptr_t p=0;if(!q::at(m,list.data,size_t(i)*8,p,e)||!object(p,region,id,a::DuplicateFamily::Building))return false;}
        if(!q::unchanged(m,buildings+0x408,100000,list,e)||!q::typed(m,image,scenery,0x17152d0,e)||
           !q::at(m,scenery,0x900,reverse,e)||reverse!=region||!q::at(m,scenery,0x908,reverse,e)||reverse!=root)return q::fail(e,e.empty()?"Duplicate Scenery manager reciprocity differs":e);
        for(size_t category=0;category<47;++category){const auto array=scenery+0x18+category*0x30;
            if(!q::typed(m,image,array,0x172d6a0,e)||!q::bounds(m,array,100000,list,e))return false;
            for(int32_t i=0;i<list.count;++i){uintptr_t p=0;int32_t actual=-1;
                if(!q::at(m,list.data,size_t(i)*8,p,e)||!q::at(m,p,0x268,actual,e)||actual!=int32_t(category)||!object(p,region,id,a::DuplicateFamily::Scenery))
                    return q::fail(e,e.empty()?"Duplicate Scenery category membership differs":e);}
            if(!q::unchanged(m,array,100000,list,e))return false;
        }
    }
    return q::unchanged(m,map+0x350,100000,regions,e);
}
struct Capture {
    uintptr_t behaviour=0,data=0;int32_t capacity=0;
    std::vector<Handle> targets;
    std::vector<uintptr_t> ghosts;
    std::vector<std::array<uint32_t,10>> poses;
    std::array<uint32_t,3> cash_position{};
    bool operator==(const Capture&)const=default;
};
template<class Memory,class Point>bool capture(Memory& m,uintptr_t image,uintptr_t behaviour,Point&& point,Capture& out,std::string& e){
    out={};out.behaviour=behaviour;uint8_t pending=0,duplicate=0;q::Bounds records,charged;
    if(!q::typed(m,image,behaviour,0x172f1f0,e)||!q::at(m,behaviour,0x401,pending,e)||pending!=1||
       !q::at(m,behaviour,0x499,duplicate,e)||duplicate!=1||!q::typed(m,image,behaviour+0x458,0x1747300,e)||
       !q::bounds(m,behaviour+0x458,100000,records,e)||!q::typed(m,image,behaviour+0x478,0x1748c50,e)||
       !q::bounds(m,behaviour+0x478,100000,charged,e))return q::fail(e,e.empty()?"Duplicate original pending/records/cost source differs":e);
    if(records.count<=0||records.count!=charged.count)return q::fail(e,"Duplicate original complete record/cost counts differ");
    out.data=records.data;out.capacity=records.capacity;std::set<uintptr_t> targets,ghosts;std::set<uint64_t> uids;
    for(int32_t i=0;i<records.count;++i){Handle target;uintptr_t blueprint=0,ghost=0,ghost_blueprint=0,cost_target=0;
        const auto record=records.data+size_t(i)*24;
        if(!q::at(m,record,0,blueprint,e)||!q::at(m,record,8,ghost,e)||!q::at(m,record,16,target.object,e)||!q::at(m,target.object,0x18,target.uid,e)||
           !q::typed(m,image,ghost,0x1705230,e)||!q::at(m,ghost,0x210,ghost_blueprint,e)||!q::at(m,charged.data,size_t(i)*8,cost_target,e))return false;
        if(!blueprint||blueprint!=ghost_blueprint||cost_target!=target.object||!target.uid||target.object==ghost||
           !targets.insert(target.object).second||!ghosts.insert(ghost).second||!uids.insert(target.uid).second)return q::fail(e,"Duplicate full original target/Ghost/Blueprint/cost order differs");
        std::array<uint32_t,10> pose;if(!group_reader::pose(m,ghost,0x100,pose,e)||!duplicate_transition::pose(pose))return q::fail(e,e.empty()?"Duplicate Ghost transform invalid":e);
        out.targets.push_back(target);out.ghosts.push_back(ghost);out.poses.push_back(pose);
    }
    if(!point(out.cash_position,e))return false;
    for(auto bits:out.cash_position)if(!std::isfinite(std::bit_cast<float>(bits)))return q::fail(e,"Duplicate source cursor point is nonfinite");
    return q::unchanged(m,behaviour+0x458,100000,records,e)&&q::unchanged(m,behaviour+0x478,100000,charged,e);
}
template<class Memory>bool cleaned(Memory& m,uintptr_t image,const Capture& value,std::string& e){
    uintptr_t records=0,options=0,cursor=0,current=0;int32_t count=-1,capacity=0;
    if(!q::typed(m,image,value.behaviour,0x172f1f0,e)||!q::at(m,value.behaviour,0x460,records,e)||
       !q::at(m,value.behaviour,0x468,count,e)||!q::at(m,value.behaviour,0x46c,capacity,e)||
       !q::at(m,value.behaviour,0x490,options,e)||!q::at(m,image,0x13df9d0,cursor,e)||!q::at(m,cursor,0x190,current,e))return false;
    if(records!=value.data||capacity!=value.capacity||count||options||current==value.behaviour)
        return q::fail(e,"Duplicate original Deactivate/header cleanup differs");
    for(size_t i=0;i<value.targets.size();++i){uintptr_t blueprint=0,ghost=0;
        if(!q::at(m,records,i*24,blueprint,e)||!q::at(m,records,i*24+8,ghost,e))return false;
        if(blueprint||ghost)return q::fail(e,"Duplicate original owned record cleanup incomplete index="+std::to_string(i));}
    return true;
}
}

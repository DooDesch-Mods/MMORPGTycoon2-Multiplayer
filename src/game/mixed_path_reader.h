#pragma once
#include "path_reader.h"
#include <algorithm>
#include <map>
#include <tuple>

namespace mt2mp::game::mixed_path_native {
using Point=action_session::PathPoint;
struct Region {int32_t id=-1;uintptr_t object=0;bool operator==(const Region&)const=default;};
struct Ref {
    int32_t region=-1,type=-1,index=-1;
    bool operator==(const Ref&)const=default;
    bool operator<(const Ref& other)const{return std::tie(region,type,index)<std::tie(other.region,other.type,other.index);}
};
struct Member {uintptr_t object=0;uint64_t uid=0;Ref ref;bool operator==(const Member&)const=default;};
struct Path {
    Member member;uintptr_t owner=0,second=0,start=0,end=0;
    std::vector<Point> points,walks;std::vector<float> distances,walk_distances;
    bool operator==(const Path&)const=default;
};
struct Node {
    Member member;uintptr_t owner=0;Point position{};std::vector<uintptr_t> edges;
    bool operator==(const Node&)const=default;
};
struct Remote {int32_t region=-1,type=-1,index=-1;uintptr_t path=0;bool operator==(const Remote&)const=default;};
struct Census {
    std::vector<Region> regions;std::vector<Path> paths;std::vector<Node> nodes;std::vector<Remote> remote;
    bool operator==(const Census&)const=default;
};
inline bool reject(std::string& error,const std::string& cause){error="mixed path census: "+cause;return false;}
inline bool finite(const Point& point){return std::all_of(point.begin(),point.end(),[](float value){return std::isfinite(value);});}

// Resolve the entire saved Map Region table, including Regions not owned by
// this player. Cross-Region Pass endpoints and remote membership live there.
template<class Memory>
bool saved_regions(Memory& memory,uintptr_t image,uintptr_t map,uintptr_t root,
                   std::vector<Region>& out,std::string& error){
    using namespace path_native;
    if(!map||map>UINTPTR_MAX-0x364||root!=map+0x1c0)return reject(error,"Map/root identity differs");
    uintptr_t table=map+0x230,map_ap=0,root_ap=0,table_ap=0,data=0,owned=0;
    int32_t root_type=-1,count=-1,capacity=-1,owned_count=-1,owned_capacity=-1;
    if(!field(memory,map,0,map_ap,error)||!field(memory,root,0,root_ap,error)||
       !field(memory,root,0x48,root_type,error)||!field(memory,table,0,table_ap,error)||
       !field(memory,table,8,data,error)||!field(memory,table,0x10,count,error)||
       !field(memory,table,0x14,capacity,error)||!field(memory,map,0x358,owned,error)||
       !field(memory,map,0x360,owned_count,error)||!field(memory,map,0x364,owned_capacity,error))return false;
    if(map_ap!=image+0x1742860||root_ap!=image+0x174ad20||root_type!=0||
       table_ap!=image+0x16f04e0||count<1||count>4096||capacity<count||!data||
       owned_count<0||owned_count>count||owned_capacity<owned_count||(owned_count&&!owned))
        return reject(error,"saved Map Region table class or bounds differ");
    std::vector<Region> value;value.reserve(size_t(count));
    std::set<int32_t> ids;std::set<uintptr_t> pointers;
    for(int32_t index=0;index<count;++index){Region row;uintptr_t ap=0,shard=0;
        if(!field(memory,data,size_t(index)*8,row.object,error)||!row.object||
           !field(memory,row.object,0,ap,error)||!field(memory,row.object,0x70,shard,error)||
           !field(memory,row.object,0x834,row.id,error))return false;
        if(ap!=image+0x1787930||shard!=root||row.id<0||
           !ids.insert(row.id).second||!pointers.insert(row.object).second)
            return reject(error,"saved Region AP/shard/ID or uniqueness differs");
        value.push_back(row);
    }
    std::set<uintptr_t> owned_unique;
    for(int32_t index=0;index<owned_count;++index){uintptr_t region=0,ap=0,shard=0;
        if(!field(memory,owned,size_t(index)*8,region,error)||!region||
           !field(memory,region,0,ap,error)||!field(memory,region,0x70,shard,error))return false;
        if(ap!=image+0x1787930||shard!=root||!pointers.contains(region)||
           !owned_unique.insert(region).second)return reject(error,"owned Region subset differs");
    }
    uintptr_t fresh_data=0,fresh_owned=0;int32_t fresh_count=-1,fresh_owned_count=-1;
    if(!field(memory,table,8,fresh_data,error)||!field(memory,table,0x10,fresh_count,error)||
       !field(memory,map,0x358,fresh_owned,error)||!field(memory,map,0x360,fresh_owned_count,error))return false;
    if(fresh_data!=data||fresh_count!=count||fresh_owned!=owned||fresh_owned_count!=owned_count)
        return reject(error,"saved Region table changed during complete capture");
    out=std::move(value);return true;
}

template<class Memory>
bool read(Memory& memory,uintptr_t image,const std::vector<Region>& supplied,Census& out,std::string& error){
    using namespace path_native;
    Census value;value.regions=supplied;
    if(value.regions.empty()||value.regions.size()>4096)return reject(error,"Region count requires1..4096");
    std::sort(value.regions.begin(),value.regions.end(),[](const Region& a,const Region& b){return a.id<b.id;});
    std::map<uintptr_t,int32_t> regions;std::set<int32_t> ids;std::set<uint64_t> uids;std::set<uintptr_t> all;
    struct Bucket {uintptr_t owner=0;size_t data=0,count=0,capacity=0;std::vector<uintptr_t> members;};
    std::vector<Bucket> buckets;size_t total_members=0;
    auto scan=[&](uintptr_t owner,size_t data,size_t count,size_t capacity,std::vector<uintptr_t>& members){
        if(!array(memory,owner,data,count,capacity,1000000,members,error))return false;
        int32_t native_capacity=-1;
        if(!field(memory,owner,capacity,native_capacity,error))return false;
        if(native_capacity<0||members.size()>1000000-total_members)
            return reject(error,"native bucket capacity or complete member budget differs");
        total_members+=members.size();
        buckets.push_back({owner,data,count,capacity,members});return true;
    };
    for(const auto& region:value.regions){
        int32_t actual=-1;uintptr_t manager=0,reciprocal=0;
        if(region.id<0||!region.object||!ids.insert(region.id).second||!regions.emplace(region.object,region.id).second)
            return reject(error,"Region ID/pointer is absent or duplicated");
        if(!ap(memory,region.object,image,0x1787930,error)||!field(memory,region.object,0x834,actual,error)||
           !field(memory,region.object,0x9a0,manager,error)||!field(memory,manager,0x6a0,reciprocal,error))return false;
        if(actual!=region.id)return reject(error,"supplied Region ID differs from its native row");
        if(reciprocal!=region.object)return reject(error,"PathManager Region reciprocal differs");
        for(int32_t type=0;type<11;++type){
            std::vector<uintptr_t> members;
            const size_t offset=size_t(type)*0x30;
            if(!scan(manager,0x28+offset,0x30+offset,0x34+offset,members))return false;
            for(size_t index=0;index<members.size();++index){
                Path row;row.member.object=members[index];row.member.ref={region.id,type,int32_t(index)};int32_t actual_type=-1;uint8_t preview=0;
                if(!all.insert(row.member.object).second||!row.member.object)return reject(error,"owned Path pointer is absent or duplicated");
                if(!ap(memory,row.member.object,image,0x1743f10,error)||!field(memory,row.member.object,0x18,row.member.uid,error)||
                   !field(memory,row.member.object,0x21c,actual_type,error)||!field(memory,row.member.object,0x178,row.owner,error)||
                   !field(memory,row.member.object,0x2f8,row.second,error)||!field(memory,row.member.object,0x268,row.start,error)||
                   !field(memory,row.member.object,0x270,row.end,error)||!field(memory,row.member.object,0x46e,preview,error))return false;
                if(!row.member.uid||!uids.insert(row.member.uid).second||actual_type!=type||row.owner!=region.object||preview)
                    return reject(error,"owned Path full UID/type/Region/preview differs");
                if(!array(memory,row.member.object,0x298,0x2a0,0x2a4,65536,row.points,error)||
                   !array(memory,row.member.object,0x2b0,0x2b8,0x2bc,65536,row.distances,error))return false;
                if(type==6&&(!array(memory,row.member.object,0x2c8,0x2d0,0x2d4,65536,row.walks,error)||
                   !array(memory,row.member.object,0x2e0,0x2e8,0x2ec,65536,row.walk_distances,error)))return false;
                if(row.points.size()<2||(type==6&&row.walks.size()<2))return reject(error,"owned Path has fewer than two native waypoints");
                for(const auto* list:{&row.points,&row.walks})for(const auto& point:*list)if(!finite(point))return reject(error,"native waypoint is not finite");
                for(const auto* list:{&row.distances,&row.walk_distances})for(float distance:*list)if(!std::isfinite(distance))return reject(error,"native distance is not finite");
                value.paths.push_back(std::move(row));
            }
            // Remote containers include their own header and are0x48 bytes apart.
            if(!scan(manager,0x370+size_t(type)*0x48,0x378+size_t(type)*0x48,0x37c+size_t(type)*0x48,members))return false;
            std::set<uintptr_t> unique;
            for(size_t index=0;index<members.size();++index){
                if(!members[index]||!unique.insert(members[index]).second)return reject(error,"remote Path pointer is absent or duplicated");
                value.remote.push_back({region.id,type,int32_t(index),members[index]});
            }
        }
        for(int32_t type=0;type<6;++type){
            std::vector<uintptr_t> members;const size_t offset=size_t(type)*0x30;
            if(!scan(manager,0x238+offset,0x240+offset,0x244+offset,members))return false;
            for(size_t index=0;index<members.size();++index){
                Node row;row.member.object=members[index];row.member.ref={region.id,type,int32_t(index)};int32_t actual_type=-1;
                if(!row.member.object||!all.insert(row.member.object).second)return reject(error,"Intersection pointer is absent or duplicated");
                if(!ap(memory,row.member.object,image,0x1700a10,error)||!field(memory,row.member.object,0x18,row.member.uid,error)||
                   !field(memory,row.member.object,0x298,actual_type,error)||!field(memory,row.member.object,0x178,row.owner,error)||
                   !field(memory,row.member.object,0x110,row.position,error)||!array(memory,row.member.object,0x2c8,0x2d0,0x2d4,65536,row.edges,error))return false;
                if(!row.member.uid||!uids.insert(row.member.uid).second||actual_type!=type||row.owner!=region.object||!finite(row.position))
                    return reject(error,"Intersection full UID/type/Region/position differs");
                value.nodes.push_back(std::move(row));
            }
        }
    }
    std::map<uintptr_t,const Path*> paths;std::map<uintptr_t,const Node*> nodes;std::map<uintptr_t,std::set<uintptr_t>> expected;
    for(const auto& row:value.paths)paths.emplace(row.member.object,&row);
    for(const auto& row:value.nodes)nodes.emplace(row.member.object,&row);
    for(const auto& row:value.paths){
        if(row.second&&(!regions.contains(row.second)||row.second==row.owner))return reject(error,"Path second Region is foreign or equals its owner");
        for(uintptr_t endpoint:{row.start,row.end}){
            if(!endpoint)continue;
            const auto found=nodes.find(endpoint);if(found==nodes.end())return reject(error,"Path endpoint is absent from all six Intersection buckets");
            if(found->second->owner!=row.owner&&found->second->owner!=row.second)return reject(error,"Path endpoint Region differs from owner/second Region");
            expected[endpoint].insert(row.member.object);
        }
    }
    for(const auto& row:value.nodes){
        std::set<uintptr_t> actual;
        for(uintptr_t edge:row.edges)if(!paths.contains(edge)||!actual.insert(edge).second)return reject(error,"Intersection edge is foreign or duplicated");
        if(actual!=expected[row.member.object])return reject(error,"Intersection edges are not reciprocal with complete Path endpoints");
    }
    std::set<std::pair<int32_t,uintptr_t>> remote;
    for(const auto& row:value.remote){
        const auto found=paths.find(row.path);
        if(found==paths.end()||found->second->member.ref.type!=row.type||!found->second->second||
           regions.at(found->second->second)!=row.region||!remote.emplace(row.region,row.path).second)
            return reject(error,"remote Path type/second Region/owned membership differs");
    }
    for(const auto& row:value.paths)if(row.second&&!remote.contains({regions.at(row.second),row.member.object}))
        return reject(error,"second Region is missing its reciprocal remote Path");
    for(const auto& bucket:buckets){std::vector<uintptr_t> fresh;
        if(!array(memory,bucket.owner,bucket.data,bucket.count,bucket.capacity,1000000,fresh,error))return false;
        if(fresh!=bucket.members)return reject(error,"complete native bucket membership changed during capture");
    }
    out=std::move(value);return true;
}
template<class Memory>
bool read_world(Memory& memory,uintptr_t image,uintptr_t map,uintptr_t root,Census& out,std::string& error){
    std::vector<Region> regions;
    return saved_regions(memory,image,map,root,regions,error)&&read(memory,image,regions,out,error);
}
}

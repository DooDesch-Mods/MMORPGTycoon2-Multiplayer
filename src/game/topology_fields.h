#pragma once
#include "network_node_reader.h"

namespace mt2mp::game::network_topology_native {
namespace n=network_node_native;
using json=n::json;
using Point=std::array<float,3>;
inline bool reject(std::string& e,const std::string& cause){e=cause;return false;}
template<class Values>bool finite(const Values& values,std::string& e){for(float v:values)if(!std::isfinite(v))return reject(e,"network topology scalar is not finite");return true;}
struct Pipe {
    uintptr_t object=0,region=0,manager=0,definition=0,start=0,end=0;
    uint64_t uid=0;int32_t region_id=-1,cost=0;
    std::string type,key;
    Point start_position{},end_position{};
    std::array<float,5> definition_values{};
    uint8_t cross_region=0;
    std::vector<Point> points;
    std::vector<float> distances,current,last_current;
    std::vector<uintptr_t> connections;
    json row;
};
template<class M>bool pipe(M& m,uintptr_t image,uintptr_t object,uintptr_t region,uintptr_t manager,int32_t region_id,Pipe& p,std::string& e){
    namespace r=n::read;p={};p.object=object;p.region=region;p.manager=manager;p.region_id=region_id;
    uintptr_t actual_region=0;uint8_t baked=0,dirty=1;std::string type;
    if(!r::ap(m,object,image,0x16fca60,e)||!r::at(m,object,0x18,p.uid,e)||!r::at(m,object,0x178,actual_region,e)||
       !r::at(m,object,0x320,p.definition,e)||!r::ap(m,p.definition,image,0x17380d0,e)||
       !r::text(m,p.definition+0x18,p.type,e)||!r::text(m,p.definition+0x38,p.key,e)||!r::text(m,object+0x210,type,e)||
       !r::at(m,p.definition,0x78,p.cost,e)||!r::at(m,p.definition,0xa0,p.definition_values,e)||!r::at(m,p.definition,0xb4,p.cross_region,e)||
       !r::at(m,object,0x260,p.start,e)||!r::at(m,object,0x268,p.end,e)||!r::at(m,object,0x270,p.start_position,e)||
       !r::at(m,object,0x27c,p.end_position,e)||!r::at(m,object,0x56c,dirty,e)||!r::at(m,object,0x56e,baked,e))return false;
    if(!p.uid||actual_region!=region||type!=p.type||p.cost<0||p.cross_region>1||!p.start||!p.end||p.start==p.end||dirty||baked!=1)
        return reject(e,"network committed pipe identity/endpoints/bake differs");
    if(!r::array(m,object,0x290,0x298,0x29c,65536,p.points,e)||!r::array(m,object,0x2a8,0x2b0,0x2b4,65536,p.distances,e)||
       !r::array(m,object,0x238,0x240,0x244,65536,p.current,e)||!r::array(m,object,0x250,0x258,0x25c,65536,p.last_current,e)||
       !r::array(m,object,0x2c0,0x2c8,0x2cc,65536,p.connections,e))return false;
    if(p.points.size()<2||p.distances.size()!=p.points.size())return reject(e,"network committed pipe waypoint/distance count differs");
    if(!finite(p.definition_values,e)||!finite(p.start_position,e)||!finite(p.end_position,e)||!finite(p.distances,e)||!finite(p.current,e)||!finite(p.last_current,e))return false;
    for(const auto& point:p.points)if(!finite(point,e))return false;
    return true;
}
struct Connection {
    uintptr_t object=0,region=0,node=0,pipe=0;
    int32_t waypoint=-1;Point position{},source_position{};
    float pending=0;std::array<float,7> dynamic{};uint8_t connected=0;
    json row;
};
template<class M>bool connection(M& m,uintptr_t image,uintptr_t object,Connection& c,std::string& e){
    namespace r=n::read;c={};c.object=object;
    if(!r::ap(m,object,image,0x17251d0,e)||!r::at(m,object,0x158,c.node,e)||!r::at(m,object,0x160,c.pipe,e)||
       !r::at(m,object,0x168,c.waypoint,e)||!r::at(m,object,0x16c,c.position,e)||!r::at(m,object,0xf8,c.source_position,e)||
       !r::at(m,object,0x178,c.pending,e)||!r::at(m,object,0x180,c.region,e)||!r::at(m,object,0x188,c.dynamic,e)||!r::at(m,object,0x1a4,c.connected,e))return false;
    if(!c.region||c.connected!=1||bool(c.node)==bool(c.pipe)||!std::isfinite(c.pending))return reject(e,"network linked connection target/region/state differs");
    return finite(c.position,e)&&finite(c.source_position,e)&&finite(c.dynamic,e);
}
}

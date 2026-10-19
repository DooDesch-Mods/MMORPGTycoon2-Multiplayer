#pragma once
#include "mixed_path_graph.h"
#include <bit>
#include <optional>

namespace mt2mp::game::wall_pass_cursor {
using Point=action_session::PathPoint;
using Bits=std::array<uint32_t,3>;
using Spline=std::array<uint32_t,12>;
inline constexpr uintptr_t wall_ap=0x172f8d0,pass_ap=0x172f3c0,cursor_ap=0x1786dd0,path_ap=0x1743f10;
inline bool reject(std::string& error,const std::string& why){error="Wall/Pass native cursor: "+why;return false;}
inline Bits bits(const Point& point){return {std::bit_cast<uint32_t>(point[0]),std::bit_cast<uint32_t>(point[1]),std::bit_cast<uint32_t>(point[2])};}
inline bool finite(const Point& point){return mixed_path_native::finite(point);}
inline std::string key_for(uintptr_t object,const mixed_path_native::Census& census,
                           const std::map<uint64_t,std::string>& keys,bool path){
    if(!object)return {};
    if(path){for(const auto& row:census.paths)if(row.member.object==object){const auto found=keys.find(row.member.uid);return found==keys.end()?std::string{}:found->second;}}
    else for(const auto& row:census.nodes)if(row.member.object==object){const auto found=keys.find(row.member.uid);return found==keys.end()?std::string{}:found->second;}
    return {};
}
template<class Memory>bool name(Memory& memory,uintptr_t owner,size_t offset,std::string& value,std::string& error){
    uintptr_t data=0;size_t count=0;
    if(!path_native::field(memory,owner,offset,data,error)||!path_native::field(memory,owner,offset+8,count,error))return false;
    if(!data||count>256||data>UINTPTR_MAX-count)return reject(error,"native name pointer or length differs");
    std::string text(count,'\0');
    if(count&&!memory.copy(data,text.data(),count,error))return false;
    if(text.find('\0')!=std::string::npos)return reject(error,"native name contains NUL");
    value=std::move(text);return true;
}
template<class Memory>bool active(Memory& memory,uintptr_t image,uintptr_t receiver,uintptr_t cursor,
                                  uintptr_t expected_ap,std::string& error){
    uintptr_t actual_cursor=0,selected=0;
    if(!path_native::field(memory,image,0x13df9d0,actual_cursor,error)||
       !path_native::ap(memory,cursor,image,cursor_ap,error)||
       !path_native::field(memory,cursor,0x190,selected,error)||
       !path_native::ap(memory,receiver,image,expected_ap,error))return false;
    return actual_cursor==cursor&&selected==receiver||reject(error,"original active Cursor or behaviour differs");
}
struct WallIntent {
    int32_t region=-1,type=-1,node_type=-1;
    std::string name,start_node,start_split,end_node,end_split;
    Bits start{},release{};uint32_t quoted_length=0;
    std::vector<Spline> segments;
    std::vector<Bits> preview;
    bool preview_valid=false;
    bool operator==(const WallIntent&)const=default;
};
struct WallSource {
    uintptr_t receiver=0,cursor=0,ghost=0,region=0,start_node=0,start_split=0,end_node=0,end_split=0;
    WallIntent intent;
};
template<class Memory>
bool wall(Memory& memory,uintptr_t image,uintptr_t receiver,uintptr_t cursor,uintptr_t release,
          const mixed_path_native::Census& census,const std::map<uint64_t,std::string>& keys,
          WallSource& out,std::string& error){
    if(!active(memory,image,receiver,cursor,wall_ap,error)||!release)return false;
    WallSource value;value.receiver=receiver;value.cursor=cursor;
    Point start{},end{};float length=0;
    if(!path_native::field(memory,receiver,0x148,value.intent.type,error)||
       !path_native::field(memory,receiver,0x14c,value.intent.node_type,error)||
       !path_native::field(memory,receiver,0x158,length,error)||
       !path_native::field(memory,receiver,0x238,value.ghost,error)||
       !path_native::field(memory,receiver,0x240,value.start_split,error)||
       !path_native::field(memory,receiver,0x248,value.start_node,error)||
       !path_native::field(memory,receiver,0x278,value.end_node,error)||
       !path_native::field(memory,receiver,0x280,value.end_split,error)||
       !path_native::field(memory,receiver,0x288,value.region,error)||
       !path_native::field(memory,receiver,0x298,start,error)||
       !path_native::field(memory,release,0,end,error)||
       !name(memory,receiver,0x190,value.intent.name,error)||
       !path_native::ap(memory,value.region,image,0x1787930,error)||
       !path_native::ap(memory,value.ghost,image,path_ap,error)||
       !path_native::field(memory,value.region,0x834,value.intent.region,error))return false;
    uint8_t preview=0,preview_valid=0;uintptr_t owner=0;
    if(!path_native::field(memory,value.ghost,0x46e,preview,error)||
       !path_native::field(memory,value.ghost,0x46c,preview_valid,error)||
       !path_native::field(memory,value.ghost,0x178,owner,error)||owner!=value.region||preview!=1)
        return reject(error,"Wall ghost Region or preview flag differs");
    if(preview_valid>1)return reject(error,"Wall ghost SetWaypoints validity differs");
    value.intent.preview_valid=preview_valid==1;
    if(value.intent.region<0||value.intent.type<0||value.intent.type>=11||
       value.intent.node_type<0||value.intent.node_type>=6||!finite(start)||!finite(end)||
       !std::isfinite(length)||length<=0)return reject(error,"Wall semantic type/point/quoted length differs");
    value.intent.start=bits(start);value.intent.release=bits(end);
    value.intent.quoted_length=std::bit_cast<uint32_t>(length);
    std::vector<std::array<float,12>> splines;
    if(!path_native::array(memory,receiver,0x2b0,0x2b8,0x2bc,10,splines,error)||
       splines.empty())return reject(error,error.empty()?"Wall complete spline array absent":error);
    for(const auto& spline:splines){Spline saved{};
        for(size_t i=0;i<12;++i){if(!std::isfinite(spline[i]))return reject(error,"Wall spline component is not finite");
            saved[i]=std::bit_cast<uint32_t>(spline[i]);}
        value.intent.segments.push_back(saved);}
    std::vector<Point> preview_points;
    if(!path_native::array(memory,value.ghost,0x298,0x2a0,0x2a4,65536,preview_points,error)||
       preview_points.size()<2)return reject(error,error.empty()?"Wall preview Path has fewer than two waypoints":error);
    for(const auto& point:preview_points){if(!finite(point))return reject(error,"Wall preview waypoint is not finite");
        value.intent.preview.push_back(bits(point));}
    value.intent.start_node=key_for(value.start_node,census,keys,false);
    value.intent.end_node=key_for(value.end_node,census,keys,false);
    value.intent.start_split=value.start_split==value.ghost?"preview":key_for(value.start_split,census,keys,true);
    value.intent.end_split=value.end_split==value.ghost?"preview":key_for(value.end_split,census,keys,true);
    for(const auto& pair:{std::pair{value.start_node,&value.intent.start_node},
                          std::pair{value.end_node,&value.intent.end_node},
                          std::pair{value.start_split,&value.intent.start_split},
                          std::pair{value.end_split,&value.intent.end_split}})
        if(pair.first&&pair.second->empty())return reject(error,"Wall selected original node or split Path lacks canonical identity");
    out=std::move(value);return true;
}
struct PassIntent {
    int32_t start_region=-1,end_region=-1,type=-1;
    std::string name,start_node,end_node;
    Bits start{},end{};
    std::vector<Bits> preview;
    bool preview_valid=false;
    bool operator==(const PassIntent&)const=default;
};
struct PassSource {
    uintptr_t receiver=0,cursor=0,ghost=0,start_node=0,end_node=0,start_region=0,end_region=0;
    PassIntent intent;
};
template<class Memory>
bool pass(Memory& memory,uintptr_t image,uintptr_t receiver,uintptr_t cursor,
          const mixed_path_native::Census& census,const std::map<uint64_t,std::string>& keys,
          PassSource& out,std::string& error){
    if(!active(memory,image,receiver,cursor,pass_ap,error))return false;
    PassSource value;value.receiver=receiver;value.cursor=cursor;
    uint8_t valid=0,preview=0,preview_valid=0;Point start{},end{};uintptr_t owner=0;
    if(!path_native::field(memory,receiver,0x1ea,valid,error)||
       !path_native::field(memory,receiver,0x188,value.ghost,error)||
       !path_native::field(memory,receiver,0x190,value.start_node,error)||
       !path_native::field(memory,receiver,0x198,value.end_node,error)||
       !path_native::field(memory,receiver,0x1c0,value.start_region,error)||
       !path_native::field(memory,receiver,0x1c8,value.end_region,error)||
       !path_native::field(memory,receiver,0x1d0,start,error)||
       !path_native::field(memory,receiver,0x1dc,end,error)||
       !path_native::ap(memory,value.ghost,image,path_ap,error)||
       !path_native::ap(memory,value.start_region,image,0x1787930,error)||
       !path_native::ap(memory,value.end_region,image,0x1787930,error)||
       !path_native::field(memory,value.ghost,0x46e,preview,error)||
       !path_native::field(memory,value.ghost,0x46c,preview_valid,error)||
       !path_native::field(memory,value.ghost,0x178,owner,error)||
       !path_native::field(memory,value.ghost,0x21c,value.intent.type,error)||
       !path_native::field(memory,value.start_region,0x834,value.intent.start_region,error)||
       !path_native::field(memory,value.end_region,0x834,value.intent.end_region,error)||
       !name(memory,value.ghost,0x190,value.intent.name,error))return false;
    if(valid!=1||preview!=1||preview_valid!=1||owner!=value.start_region||
       value.start_region==value.end_region||value.intent.start_region<0||value.intent.end_region<0||
       value.intent.type!=2||!finite(start)||!finite(end)||start==end)
        return reject(error,"Pass valid ghost, Regions, type or endpoints differ");
    value.intent.start=bits(start);value.intent.end=bits(end);
    value.intent.preview_valid=true;
    std::vector<Point> preview_points;
    if(!path_native::array(memory,value.ghost,0x298,0x2a0,0x2a4,65536,preview_points,error)||
       preview_points.size()<2)return reject(error,error.empty()?"Pass preview Path has fewer than two waypoints":error);
    for(const auto& point:preview_points){if(!finite(point))return reject(error,"Pass preview waypoint is not finite");
        value.intent.preview.push_back(bits(point));}
    value.intent.start_node=key_for(value.start_node,census,keys,false);
    value.intent.end_node=key_for(value.end_node,census,keys,false);
    if((value.start_node&&value.intent.start_node.empty())||(value.end_node&&value.intent.end_node.empty()))
        return reject(error,"Pass original selected endpoint lacks canonical Intersection identity");
    out=std::move(value);return true;
}
}

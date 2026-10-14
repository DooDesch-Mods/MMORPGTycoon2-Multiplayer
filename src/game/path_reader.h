#pragma once
#include "typed_action/path_action.h"
#include <set>
#include <limits>
namespace mt2mp::game::path_native {
using action_session::PathIntent;
using action_session::PathPoint;
template<class M,class T>bool field(M& m,uintptr_t p,size_t off,T& value,std::string& e) {
    if(!p||p>UINTPTR_MAX-off){e="path native address overflow";return false;}
    return m.copy(p+off,&value,sizeof(value),e);
}
inline bool mismatch(std::string& e,const std::string& key,uint64_t actual,uint64_t expected) {
    e="path "+key+" actual="+std::to_string(actual)+" required="+std::to_string(expected);return false;
}
template<class M>bool ap(M& m,uintptr_t p,uintptr_t image,uintptr_t expected,std::string& e) {
    uintptr_t actual=0;if(!field(m,p,0,actual,e))return false;
    return actual==image+expected||mismatch(e,"address point",actual,image+expected);
}
template<class M,class T>bool array(M& m,uintptr_t owner,size_t data_off,size_t count_off,size_t cap_off,size_t bound,std::vector<T>& out,std::string& e) {
    uintptr_t data=0;int32_t count=0,cap=0;
    if(!field(m,owner,data_off,data,e)||!field(m,owner,count_off,count,e)||!field(m,owner,cap_off,cap,e))return false;
    if(count<0||size_t(count)>bound||cap<count||(!data&&count)||size_t(count)>SIZE_MAX/sizeof(T)||data>UINTPTR_MAX-size_t(count)*sizeof(T)){
        e="path native array count="+std::to_string(count)+" capacity="+std::to_string(cap)+" bound="+std::to_string(bound);return false;}
    out.resize(size_t(count));return !count||m.copy(data,out.data(),out.size()*sizeof(T),e);
}
template<class M>bool snap_target(M& m,uintptr_t receiver,uintptr_t expected,std::string& e) {
    uintptr_t node=0,split=0;if(!field(m,receiver,0x1e8,node,e)||!field(m,receiver,0x1f0,split,e))return false;
    if(split)return mismatch(e,"unexpected native split target",split,0);
    return node==expected||mismatch(e,"native snapped endpoint",node,expected);
}
struct Preview {
    PathIntent intent;
    uintptr_t receiver=0,ghost=0,region=0,start_node=0,end_node=0;
    int32_t type=-1,node_type=-1;
};
struct TargetPosition {uint32_t family=0,bucket=0,index=0;uint64_t uid=0;PathPoint position{};};
template<class M>bool target_positions(M& m,uintptr_t image,uintptr_t region,std::vector<TargetPosition>& out,std::string& e) {
    out.clear();
    for(uint32_t family=0;family<2;++family){uintptr_t manager=0;if(!field(m,region,family?0x998:0x950,manager,e))return false;
        for(uint32_t bucket=0;bucket<(family?3u:1u);++bucket){const size_t offset=family?0x40+bucket*0x30:0x410;std::vector<uintptr_t> props;
            if(!array(m,manager,offset,offset+8,offset+12,4096,props,e))return false;
            for(size_t i=0;i<props.size();++i){TargetPosition row;row.family=family;row.bucket=bucket;row.index=uint32_t(i);uintptr_t table=0;
                if(!field(m,props[i],0,table,e)||!field(m,props[i],0x18,row.uid,e)||!field(m,props[i],0x110,row.position,e))return false;
                if(table<image||table-image>0x2000000||!row.uid){e="path target position native class/UID differs";return false;}
                for(float v:row.position)if(!std::isfinite(v)){e="path target position not finite";return false;}
                out.push_back(row);
            }
        }
    }return true;
}
template<class M>bool preview(M& m,uintptr_t image,uintptr_t receiver,int32_t road,int32_t road_node,Preview& out,std::string& e) {
    Preview p;p.receiver=receiver;
    if(!ap(m,receiver,image,0x172f570,e)||!field(m,receiver,0x1a8,p.ghost,e)||!ap(m,p.ghost,image,0x1743f10,e)||
       !field(m,receiver,0x188,p.type,e)||!field(m,receiver,0x18c,p.node_type,e))return false;
    if(p.type!=road)return mismatch(e,"type",uint32_t(p.type),uint32_t(road));
    if(p.node_type!=road_node)return mismatch(e,"node type",uint32_t(p.node_type),uint32_t(road_node));
    std::array<uint8_t,5> flags{};if(!field(m,receiver,0x230,flags,e))return false;
    if(!field(m,receiver,0x1b8,p.start_node,e)||!field(m,receiver,0x1e8,p.end_node,e))return false;
    const uint8_t snapped=p.end_node?1:0;
    if(flags!=std::array<uint8_t,5>{1,snapped,1,snapped,0}){e="path requires active valid pinned or snapped endpoint, flags=";for(auto v:flags)e+=std::to_string(v)+",";return false;}
    for(size_t offset:{0x1b0,0x1f0}){uintptr_t v=0;if(!field(m,receiver,offset,v,e))return false;if(v)return mismatch(e,"path split target",v,0);}
    if(!field(m,receiver,0x1f8,p.region,e)||!ap(m,p.region,image,0x1787930,e)||!field(m,p.region,0x834,p.intent.region,e)||
       !field(m,receiver,0x208,p.intent.start,e)||!field(m,receiver,0x1d8,p.intent.end,e)||!field(m,receiver,0x190,p.intent.quoted_length,e))return false;
    uintptr_t actual_region=0;if(!field(m,p.ghost,0x178,actual_region,e))return false;
    if(actual_region!=p.region)return mismatch(e,"preview region",actual_region,p.region);
    std::vector<std::array<float,12>> splines;
    if(!array(m,receiver,0x220,0x228,0x22c,1,splines,e)||splines.size()!=1){if(e.empty())e="path first slice requires exactly1 native spline";return false;}
    p.intent.spline=splines.front();
    if(!array(m,p.ghost,0x298,0x2a0,0x2a4,128,p.intent.preview,e)||!action_session::valid_path(p.intent)){if(e.empty())e="path native scalar domain invalid";return false;}
    for(size_t offset:{0x268,0x270}){uintptr_t v=0;if(!field(m,p.ghost,offset,v,e))return false;if(v)return mismatch(e,"preview connected endpoint",v,0);}
    uint8_t is_preview=0;if(!field(m,p.ghost,0x46e,is_preview,e))return false;
    if(is_preview!=1)return mismatch(e,"preview flag",is_preview,1);
    out=std::move(p);return true;
}
}

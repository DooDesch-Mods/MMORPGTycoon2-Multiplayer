#pragma once
#include "connected_network_reader.h"
namespace mt2mp::game::network_pipe_native {
using Intent=action_session::NetworkPipeIntent;
namespace graph=connected_network_native;
struct Preview {Intent intent;uintptr_t object=0,definition=0,node_definition=0,start=0,end=0;uint8_t pending=0;};
template<class M>bool preview(M& memory,uintptr_t image,uintptr_t tool,const graph::Graph& before,const std::string& digest,Preview& out,std::string& e){
    namespace r=graph::read;out={};uintptr_t region=0,start_pipe=0,end_pipe=0,actual_definition=0;uint8_t active=0,legal=0;std::string key,type,node_type;
    auto& p=out.intent;p.graph_before_sha256=digest;
    if(!r::ap(memory,tool,image,0x173ed80,e)||!r::text(memory,tool+0x148,p.type,e)||!r::text(memory,tool+0x168,p.node_type,e)||
       !r::at(memory,tool,0x190,out.definition,e)||!r::at(memory,tool,0x198,out.node_definition,e)||!r::ap(memory,out.definition,image,0x17380d0,e)||
       !r::ap(memory,out.node_definition,image,0x1738010,e)||!r::text(memory,out.definition+0x18,type,e)||!r::text(memory,out.definition+0x38,p.key,e)||
       !r::text(memory,out.node_definition+0x18,node_type,e)||!r::text(memory,out.node_definition+0x38,key,e)||
       !r::at(memory,tool,0x200,region,e)||!r::at(memory,region,0x834,p.region,e)||!r::at(memory,tool,0x1b0,out.object,e)||
       !r::at(memory,tool,0x1b8,start_pipe,e)||!r::at(memory,tool,0x1c0,out.start,e)||!r::at(memory,tool,0x1f0,out.end,e)||!r::at(memory,tool,0x1f8,end_pipe,e)||
       !r::at(memory,tool,0x238,active,e)||!r::at(memory,tool,0x239,out.pending,e)||!r::at(memory,tool,0x23a,legal,e)||
       !r::at(memory,tool,0x210,p.start,e)||!r::at(memory,tool,0x1e0,p.end,e)||!r::at(memory,tool,0x1a0,p.quoted_length,e)||
       !r::ap(memory,out.object,image,0x16fca60,e)||!r::at(memory,out.object,0x320,actual_definition,e)||
       !r::array(memory,out.object,0x290,0x298,0x29c,2048,p.preview,e)||!r::array(memory,tool,0x228,0x230,0x234,100,p.splines,e))return false;
    if(active!=1||legal!=1||out.pending>1||start_pipe||end_pipe||!out.start||actual_definition!=out.definition||type!=p.type||node_type!=p.node_type||key!="item_junction")return graph::fields::reject(e,"network Pipe actual preview requires legal Node endpoints and exact definitions");
    if(!before.node_keys.contains(out.start)||(out.end&&!before.node_keys.contains(out.end)))return graph::fields::reject(e,"network Pipe endpoint absent from actual before graph");
    const auto start=before.node_keys.at(out.start);if(start[0]!=p.region)return graph::fields::reject(e,"network Pipe start belongs to another region");p.start_node=start[1].get<int32_t>();
    if(out.end){const auto end=before.node_keys.at(out.end);if(end[0]!=p.region)return graph::fields::reject(e,"network Cable end belongs to another region");p.end_node=end[1].get<int32_t>();}
    return true;
}
}

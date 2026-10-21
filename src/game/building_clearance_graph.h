#pragma once
#include "scenery_make_history.h"
#include "scenery_stroke_history.h"

namespace mt2mp::game::building_clearance_graph {
using Saved=scenery_make_history::Saved;
// The existing Building path owns its primary inverse and cash. This projection
// validates every Scenery node in that same complete original operation array.
template<class Memory>bool read(Memory& memory,uintptr_t image,uintptr_t graph,
    const std::map<uint64_t,Saved>& makes,const std::set<uint64_t>& destroys,
    const std::set<uint64_t>& all_scenery,bool initial,std::string& error){
    namespace q=npc_quest_targets;q::Bounds span;
    if(!q::typed(memory,image,graph+0x18,0x16e2dc0,error)||
       !q::bounds(memory,graph+0x18,100000,span,error)||!span.count)
        return q::fail(error,error.empty()?"Building clearance original graph is empty":error);
    std::set<uintptr_t> nodes;std::set<uint64_t> made,destroyed;
    for(int32_t index=0;index<span.count;++index){uintptr_t node=0,ap=0;uint64_t uid=0;
        if(!q::at(memory,span.data,size_t(index)*8,node,error)||!q::at(memory,node,0,ap,error))return false;
        if(!node||!nodes.insert(node).second)return q::fail(error,"Building clearance original operation repeats");
        if(ap==image+0x1706590){
            if(!q::at(memory,node,0xb8,uid,error))return false;
            const auto found=makes.find(uid);
            if(found==makes.end()||!made.insert(uid).second)
                return q::fail(error,"Building clearance MakeScenery full UID is foreign or repeated uid="+std::to_string(uid));
            auto stored=found->second;
            if(!q::at(memory,node,0xd0,stored.transform,error))return false;
            if((initial&&stored.transform!=found->second.transform)||!scenery_stroke_history::same_inputs(stored,found->second)||
               !scenery_make_history::make(memory,image,node,stored,error))
                return q::fail(error,error.empty()?"Building clearance MakeScenery original payload differs":error);
        }else if(ap==image+0x16de860){
            if(!q::at(memory,node,0xb8,uid,error))return false;
            if(!all_scenery.contains(uid))continue;
            if(!destroys.contains(uid)||!destroyed.insert(uid).second||
               !scenery_make_history::destroy(memory,image,node,uid,error))
                return q::fail(error,error.empty()?"Building clearance Destroy Scenery is foreign or repeated":error);
        }
    }
    if(made.size()!=makes.size()||destroyed!=destroys)
        return q::fail(error,"Building clearance graph does not cover the exact removed/restored Scenery set");
    q::Bounds dependency_nodes,sorted;
    if(!q::typed(memory,image,graph,0x16e32d0,error)||!q::bounds(memory,graph,100000,dependency_nodes,error)||
       !q::typed(memory,image,graph+0x30,0x1748aa0,error)||!q::bounds(memory,graph+0x30,100000,sorted,error))return false;
    // Lock drains deletion work. Do builds the dependency nodes and sorted
    // execution cache later, immediately before running the original graph.
    if(!dependency_nodes.count&&!sorted.count)
        return q::unchanged(memory,graph+0x18,100000,span,error)&&q::unchanged(memory,graph,100000,dependency_nodes,error)&&
            q::unchanged(memory,graph+0x30,100000,sorted,error);
    if(!dependency_nodes.count||sorted.count!=span.count)
        return q::fail(error,"Building clearance original sorted operation count differs nodes="+std::to_string(dependency_nodes.count)+
            " sorted="+std::to_string(sorted.count)+" operations="+std::to_string(span.count));
    std::set<uintptr_t> ordered;
    for(int32_t index=0;index<sorted.count;++index){uintptr_t node=0;
        if(!q::at(memory,sorted.data,size_t(index)*8,node,error))return false;
        if(!nodes.contains(node)||!ordered.insert(node).second)return q::fail(error,"Building clearance original sorted operation is foreign or repeated");
    }
    return q::unchanged(memory,graph+0x18,100000,span,error)&&q::unchanged(memory,graph,100000,dependency_nodes,error)&&
        q::unchanged(memory,graph+0x30,100000,sorted,error);
}
}

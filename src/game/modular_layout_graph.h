#pragma once
#include "duplicate_building_history.h"
#include "duplicate_history_reader.h"
namespace mt2mp::game::modular_layout_graph {
template<class Memory>bool complete_order(Memory& memory,uintptr_t image,uintptr_t graph,
    const npc_quest_targets::Bounds& operations,const duplicate_history::Graph& checked,std::string& e){
    namespace q=npc_quest_targets;q::Bounds dependencies,ordered;
    if(!q::typed(memory,image,graph,0x16e32d0,e)||!q::bounds(memory,graph,100000,dependencies,e)||
       !q::typed(memory,image,graph+0x30,0x1748aa0,e)||!q::bounds(memory,graph+0x30,100000,ordered,e))return false;
    // AddUndo locks the inverse. Original Do fills both caches through Sort later.
    if(!dependencies.count&&!ordered.count)
        return q::unchanged(memory,graph+0x18,100000,operations,e)&&q::unchanged(memory,graph,100000,dependencies,e)&&
            q::unchanged(memory,graph+0x30,100000,ordered,e);
    if(!dependencies.count||ordered.count!=operations.count)
        return q::fail(e,"Modular original sorted graph count differs nodes="+std::to_string(dependencies.count)+
            " sorted="+std::to_string(ordered.count)+" operations="+std::to_string(operations.count));
    std::vector<uintptr_t> order;
    return q::unchanged(memory,graph+0x18,100000,operations,e)&&duplicate_history::execution_order(memory,image,graph,checked,order,e)&&
        q::unchanged(memory,graph,100000,dependencies,e)&&q::unchanged(memory,graph+0x30,100000,ordered,e);
}

template<class Memory,class Record>bool read(Memory& memory,uintptr_t image,uintptr_t graph,uint64_t uid,const duplicate_building_history::Saved& expected,Record&& record,std::string& e,bool sorted=true){
    namespace q=npc_quest_targets;q::Bounds bounds;uintptr_t node=0,method=0;uint64_t actual=0;std::array<uint8_t,0x6c> pose{};std::string layout,plan;
    if(!q::typed(memory,image,graph+0x18,0x16e2dc0,e)||!q::bounds(memory,graph+0x18,100000,bounds,e)||bounds.count!=1||!q::at(memory,bounds.data,0,node,e)||!q::typed(memory,image,node,0x173b490,e)||!q::at(memory,image+0x173b490,0x10,method,e)||method!=image+0x55dd80||!q::at(memory,node,0xb8,actual,e)||actual!=uid||!q::at(memory,node,0xc0,pose,e)||pose!=expected.transform)return q::fail(e,e.empty()?"Modular original graph/UID/transform differs":e);
    if(!record(node+0x130,false,layout,e)||!record(node+0x248,true,plan,e)||layout!=expected.layout_sha||plan!=expected.plan_sha)return q::fail(e,e.empty()?"Modular original complete stored Layout/Plan differs":e);
    if(!q::unchanged(memory,graph+0x18,100000,bounds,e))return false;if(!sorted)return true;duplicate_history::Graph checked;checked.operations={node};checked.top_operations={node};return complete_order(memory,image,graph,bounds,checked,e);
}
}

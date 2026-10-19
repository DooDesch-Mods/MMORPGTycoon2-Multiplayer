#pragma once
#include "duplicate_history_reader.h"
#include "scenery_make_history.h"

namespace mt2mp::game::group_object_graph {
template<class Memory>bool move(Memory& m,uintptr_t image,uintptr_t graph,uint64_t uid,
                              const std::array<uint8_t,0x6c>& before,duplicate_history::Graph& out,std::string& e){
    namespace q=npc_quest_targets;out={};q::Bounds nodes;uintptr_t node=0,method=0;uint64_t actual=0;std::array<uint8_t,0x6c> pose{};
    if(!q::typed(m,image,graph+0x18,0x16e2dc0,e)||!q::bounds(m,graph+0x18,100000,nodes,e)||nodes.count!=1||
       !q::at(m,nodes.data,0,node,e)||!q::typed(m,image,node,0x1786ff0,e)||!q::at(m,image+0x1786ff0,0x10,method,e)||method!=image+0x55ed80||
       !q::at(m,node,0xb8,actual,e)||actual!=uid||!q::at(m,node,0xc0,pose,e)||pose!=before)
        return q::fail(e,e.empty()?"Group original Move inverse full UID/transform/node differs":e);
    if(!scenery_make_history::uid_set(m,node+8,0,e)||!scenery_make_history::uid_set(m,node+0x30,uid,e)||
       !scenery_make_history::uid_set(m,node+0x58,0,e)||!q::unchanged(m,graph+0x18,100000,nodes,e))return false;
    out.operations={node};out.top_operations={node};std::vector<uintptr_t> sorted;
    return duplicate_history::execution_order(m,image,graph,out,sorted,e);
}
template<class Memory,class Find,class Text>
bool destroy_restore(Memory& m,uintptr_t image,uintptr_t graph,bool destroying,const std::string& target,
                     const std::map<uint64_t,duplicate_history::Known>& known,const std::vector<action_session::DuplicateCash>& cash,
                     Find&& find,Text&& text,duplicate_history::Graph& out,std::string& e){
    namespace q=npc_quest_targets;
    if(!duplicate_history::read(m,image,graph,destroying,known,find,text,out,e)||out.targets!=std::vector<std::string>{target}||
       !duplicate_history::cash_equal(out.cash,cash))return q::fail(e,e.empty()?"Group original whole Make/Destroy graph or signed refund differs":e);
    for(auto node:out.operations){uintptr_t ap=0;uint64_t uid=0;if(!q::at(m,node,0,ap,e))return false;
        if(ap==image+0x1786fb0){if(!scenery_make_history::uid_set(m,node+8,0,e)||!scenery_make_history::uid_set(m,node+0x30,0,e)||
            !scenery_make_history::uid_set(m,node+0x58,0,e))return false;continue;}
        if(!q::at(m,node,0xb8,uid,e)||!known.contains(uid))return q::fail(e,e.empty()?"Group original owned graph references a foreign full UID":e);
        if(ap==image+0x16de860){if(!scenery_make_history::destroy(m,image,node,uid,e))return false;continue;}
        if(ap!=image+0x1706590&&ap!=image+0x16fb9d0)return q::fail(e,"Group original owned graph contains an unbound operation");
        if(!scenery_make_history::uid_set(m,node+8,uid,e)||!scenery_make_history::uid_set(m,node+0x30,0,e)||
           !scenery_make_history::uid_set(m,node+0x58,0,e))return false;
    }
    std::vector<uintptr_t> sorted;return duplicate_history::execution_order(m,image,graph,out,sorted,e);
}
}

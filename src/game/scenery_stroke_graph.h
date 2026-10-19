#pragma once
#include "scenery_make_history.h"
#include "duplicate_history_reader.h"

namespace mt2mp::game::scenery_stroke_graph {
using Graph=duplicate_history::Graph;
using Cash=action_session::DuplicateCash;
template<class Memory>bool read(Memory& memory,uintptr_t image,uintptr_t graph,
                               const std::map<uint64_t,scenery_make_history::Saved>& removed,
                               const std::set<uint64_t>& born,const std::vector<Cash>& cash,Graph& out,std::string& e){
    namespace q=npc_quest_targets;out={};q::Bounds span;
    if(!q::typed(memory,image,graph+0x18,0x16e2dc0,e)||!q::bounds(memory,graph+0x18,100000,span,e)||!span.count)
        return q::fail(e,e.empty()?"Scenery original complete graph is empty":e);
    std::set<uintptr_t> nodes;std::set<uint64_t> made,destroyed;
    for(int32_t index=0;index<span.count;++index){uintptr_t node=0,ap=0;uint64_t uid=0;
        if(!q::at(memory,span.data,size_t(index)*8,node,e)||!q::at(memory,node,0,ap,e))return false;
        if(!nodes.insert(node).second)return q::fail(e,"Scenery original graph repeats an operation");out.operations.push_back(node);out.top_operations.push_back(node);
        if(ap==image+0x1706590){
            if(!q::at(memory,node,0xb8,uid,e))return false;const auto expected=removed.find(uid);
            if(expected==removed.end()||!made.insert(uid).second)return q::fail(e,"Scenery original Make has an unknown/repeated removed full UID");
            if(!scenery_make_history::make(memory,image,node,expected->second,e))return false;continue;
        }
        if(ap==image+0x16de860){
            if(!q::at(memory,node,0xb8,uid,e))return false;
            if(!born.contains(uid)||!destroyed.insert(uid).second)return q::fail(e,"Scenery original Destroy has an unknown/repeated birth full UID");
            if(!scenery_make_history::destroy(memory,image,node,uid,e))return false;continue;
        }
        if(ap==image+0x1786fb0){Cash value;uintptr_t method=0;
            if(!q::at(memory,image+0x1786fb0,0x10,method,e)||method!=image+0x53ccf0||!q::at(memory,node,0xb8,value.amount,e)||!q::at(memory,node,0xbc,value.position,e))
                return q::fail(e,e.empty()?"Scenery original cash method differs":e);
            for(auto part:value.position)if(!std::isfinite(std::bit_cast<float>(part)))return q::fail(e,"Scenery original cash position is nonfinite");
            if(!scenery_make_history::uid_set(memory,node+8,0,e)||!scenery_make_history::uid_set(memory,node+0x30,0,e)||!scenery_make_history::uid_set(memory,node+0x58,0,e))return false;
            out.cash.push_back(value);continue;
        }
        return q::fail(e,"Scenery original graph has an unbound operation addresspoint_rva="+std::to_string(ap>=image?ap-image:0));
    }
    if(made.size()!=removed.size()||destroyed!=born||!duplicate_history::cash_equal(out.cash,cash))
        return q::fail(e,"Scenery original graph lacks complete removal/birth/cash coverage");
    std::vector<uintptr_t> sorted;
    return q::unchanged(memory,graph+0x18,100000,span,e)&&duplicate_history::execution_order(memory,image,graph,out,sorted,e);
}
}

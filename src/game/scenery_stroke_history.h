#pragma once
#include "scenery_stroke_graph.h"

namespace mt2mp::game::scenery_stroke_history {
using Saved=scenery_make_history::Saved;
inline bool same_inputs(const Saved& left,const Saved& right){
    return left.uid==right.uid&&left.root==right.root&&left.region==right.region&&left.type==right.type&&left.height==right.height&&
        left.automatic==right.automatic&&!std::memcmp(left.transform.data(),right.transform.data(),40);
}
// Native Make and Destroy copy the full Transform, including its matrix cache.
// Retain that complete payload while comparing the authored quaternion, position
// and scale against the original closed transaction's exact UID domain.
template<class Memory>bool graph(Memory& memory,uintptr_t image,uintptr_t owner,
                                const std::map<uint64_t,Saved>& expected_make,const std::set<uint64_t>& expected_destroy,
                                const std::vector<action_session::DuplicateCash>& cash,bool original,
                                std::map<uint64_t,Saved>& stored,duplicate_history::Graph& out,std::string& e){
    namespace q=npc_quest_targets;stored=expected_make;q::Bounds span;
    if(!q::bounds(memory,owner+0x18,100000,span,e))return false;
    std::set<uint64_t> seen;
    for(int32_t index=0;index<span.count;++index){uintptr_t node=0,ap=0;
        if(!q::at(memory,span.data,size_t(index)*8,node,e)||!q::at(memory,node,0,ap,e))return false;
        if(ap!=image+0x1706590)continue;uint64_t uid=0;if(!q::at(memory,node,0xb8,uid,e))return false;
        const auto expected=expected_make.find(uid);
        if(expected==expected_make.end()||!seen.insert(uid).second)return q::fail(e,"Scenery history original Make full UID is foreign or repeated");
        auto& payload=stored.at(uid);
        if(!q::at(memory,node,0xd0,payload.transform,e))return false;
        if((original&&payload.transform!=expected->second.transform)||!same_inputs(payload,expected->second))
            return q::fail(e,"Scenery history original Make authored pose differs");
    }
    return q::unchanged(memory,owner+0x18,100000,span,e)&&scenery_stroke_graph::read(memory,image,owner,stored,expected_destroy,cash,out,e);
}
inline std::vector<action_session::DuplicateCash> opposite(const std::vector<action_session::DuplicateCash>& cash){
    auto out=cash;for(auto& row:out)row.amount=-row.amount;return out;
}
}

#pragma once
#include "scenery_stroke_graph.h"

namespace mt2mp::game::ordered_empty_clearance {
struct Receipt {
    std::vector<action_session::DuplicateCash> cash;
    std::vector<uintptr_t> operations;
    bool operator==(const Receipt&)const=default;
};
// Empty clearance is proven from the complete original inverse, not a guessed
// empty placement area. Any secondary operation needs its own affected receipt.
template<class Memory>bool read(Memory& memory,uintptr_t image,uintptr_t graph,uint64_t primary,
                               Receipt& out,std::string& error){
    namespace q=npc_quest_targets;out={};q::Bounds span;
    if(!primary||!q::typed(memory,image,graph+0x18,0x16e2dc0,error)||
       !q::bounds(memory,graph+0x18,100000,span,error)||!span.count)
        return q::fail(error,error.empty()?"Ordered empty clearance requires original primary/graph":error);
    std::vector<action_session::DuplicateCash> cash;
    for(int32_t i=0;i<span.count;++i){uintptr_t node=0,ap=0;
        if(!q::at(memory,span.data,size_t(i)*8,node,error)||!q::at(memory,node,0,ap,error))return false;
        if(ap==image+0x16de860){uint64_t uid=0;if(!q::at(memory,node,0xb8,uid,error))return false;
            if(uid!=primary)return q::fail(error,"Ordered clearance contains another Destroy target; affected-object receipt is not installed; original effect must not replay");
        }else if(ap==image+0x1786fb0){action_session::DuplicateCash row;
            if(!q::at(memory,node,0xb8,row.amount,error)||!q::at(memory,node,0xbc,row.position,error))return false;
            cash.push_back(row);
        }else return q::fail(error,"Ordered clearance contains secondary original operation RVA="+
            std::to_string(ap>=image?ap-image:0)+"; affected-object receipt is not installed; original effect must not replay");
    }
    duplicate_history::Graph proof;
    if(!q::unchanged(memory,graph+0x18,100000,span,error)||
       !scenery_stroke_graph::read(memory,image,graph,{},std::set<uint64_t>{primary},cash,proof,error))return false;
    out.cash=std::move(proof.cash);out.operations=std::move(proof.operations);error.clear();return true;
}
}

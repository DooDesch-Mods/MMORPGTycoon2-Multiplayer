#pragma once
#include "npc_action.h"
#include "../npc_target_key.h"

namespace mt2mp::action_session {
struct NpcHistoryOrigin {
    uint8_t action=0;
    std::string key;
    int32_t region=-1;
    NpcActor before,after;
    bool operator==(const NpcHistoryOrigin&)const=default;
};
struct NpcHistoryProof {
    NpcHistoryOrigin origin;
    int32_t cash=0;
    std::string quests_before,quests_after,references_before,references_after;
    uint32_t count_before=0,count_after=0;
    int32_t completed_before=0,completed_after=0;
    bool operator==(const NpcHistoryProof&)const=default;
};
inline bool npc_history_digest(const std::string& value){
    return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos;
}
inline bool valid_npc_history(const NpcHistoryProof& value){
    const auto& origin=value.origin;
    return (origin.action==15||origin.action==16||origin.action==17)&&
        game::npc_target_key::valid(origin.key)&&origin.region>=0&&
        (origin.action==15?(origin.before==NpcActor{}&&origin.after!=NpcActor{}):
         origin.action==16?(origin.before!=NpcActor{}&&origin.after==NpcActor{}):
         (origin.before!=NpcActor{}&&origin.after!=NpcActor{}))&&
        npc_history_digest(value.quests_before)&&npc_history_digest(value.quests_after)&&
        npc_history_digest(value.references_before)&&npc_history_digest(value.references_after)&&
        value.count_before<=4096&&value.count_after<=4096&&value.completed_before>=0&&value.completed_after>=0&&
        uint32_t(value.completed_before)<=value.count_before&&uint32_t(value.completed_after)<=value.count_after;
}
// Actor values and regenerated Quest fields are checked by the codec and the
// original graph reader. This predicate fixes membership and history direction.
inline bool npc_history_membership(bool undo,const NpcHistoryProof& proof,const NpcState& before,const NpcState& after){
    if(!valid_npc_history(proof)||before==NpcState{}||after==NpcState{}||before.count>100000||after.count>100000||
       (undo?(before.undo_count==0||after.undo_count+1!=before.undo_count):
             after.undo_count!=(before.undo_count<20?before.undo_count+1:before.undo_count)))return false;
    const auto& origin=proof.origin;
    const bool moving=origin.action==17;
    const bool restoring=!moving&&((origin.action==16)==undo);
    const bool removing=!moving&&!restoring;
    if((before.created==NpcActor{})!=restoring)return false;
    if(removing)return after.created==NpcActor{}&&before.count==after.count+1;
    if(restoring)return after.created!=NpcActor{}&&after.count==before.count+1;
    return before.count==after.count&&after.created!=NpcActor{}&&
        proof.quests_before==proof.quests_after&&proof.references_before==proof.references_after&&
        proof.count_before==proof.count_after&&proof.completed_before==proof.completed_after;
}
}

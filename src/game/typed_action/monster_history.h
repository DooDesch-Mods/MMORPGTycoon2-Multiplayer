#pragma once
#include "monster_action.h"
#include "../dungeon_gizmo_identity.h"
#include "../monster_target_key.h"
#include "../monster_move_key.h"

namespace mt2mp::action_session {
struct MonsterHistoryOrigin {
    uint8_t action=0;
    std::string key,scene;
    int32_t region=-1;
    MonsterActor before,after;
    bool operator==(const MonsterHistoryOrigin&)const=default;
};
struct MonsterHistoryProof {
    MonsterHistoryOrigin origin;
    std::string quests_before,quests_after,references_before,references_after;
    uint32_t count_before=0,count_after=0;
    int32_t completed_before=0,completed_after=0;
    bool operator==(const MonsterHistoryProof&)const=default;
};
inline bool monster_history_digest(const std::string& value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline bool valid_monster_history(const MonsterHistoryProof& value){
    const auto& origin=value.origin;
    return (origin.action==29||origin.action==30)&&
        (origin.action==29?game::monster_move_key::valid(origin.key):game::monster_target_key::valid(origin.key))&&origin.region>=0&&
        (origin.scene=="world"||game::dungeon_gizmo_identity::scene(origin.scene))&&
        valid_monster_actor(origin.before)&&origin.before!=MonsterActor{}&&valid_monster_actor(origin.after)&&
        ((origin.action==29&&origin.after!=MonsterActor{})||(origin.action==30&&origin.after==MonsterActor{}))&&
        monster_history_digest(value.quests_before)&&monster_history_digest(value.quests_after)&&
        monster_history_digest(value.references_before)&&monster_history_digest(value.references_after)&&
        value.count_before<=4096&&value.count_after<=4096&&value.completed_before>=0&&value.completed_after>=0&&
        uint32_t(value.completed_before)<=value.count_before&&uint32_t(value.completed_after)<=value.count_after;
}
inline bool monster_history_transition(bool undo,const MonsterHistoryProof& proof,const MonsterState& before,const MonsterState& after){
    if(!valid_monster_history(proof)||!valid_monster_state(before)||!valid_monster_state(after)||
       before.roster==ZoneState{}||after.roster==ZoneState{})return false;
    const auto& a=before.roster;const auto& b=after.roster;const auto& origin=proof.origin;
    if(a.zones!=b.zones||a.region_random!=b.region_random||a.release_edits!=b.release_edits||
       (undo?(a.undo_count==0||b.undo_count+1!=a.undo_count):(b.undo_count!=std::min(a.undo_count+1,20u))))return false;
    if(origin.action==29){
        return a.monsters==b.monsters&&a.next_tag==b.next_tag&&a.created_monsters==b.created_monsters&&
            monster_shared_actor(before.actor,undo?origin.after:origin.before)&&
            monster_shared_actor(after.actor,undo?origin.before:origin.after)&&proof.quests_before==proof.quests_after&&
            proof.references_before==proof.references_after&&proof.count_before==proof.count_after&&proof.completed_before==proof.completed_after;
    }
    if(int64_t(b.created_monsters)-a.created_monsters!=1)return false;
    if(undo)return before.actor==MonsterActor{}&&monster_shared_actor(after.actor,origin.before)&&
        b.monsters==a.monsters+1&&int64_t(b.next_tag)-a.next_tag==1;
    return monster_shared_actor(before.actor,origin.before)&&after.actor==MonsterActor{}&&a.monsters==b.monsters+1&&a.next_tag==b.next_tag;
}
}

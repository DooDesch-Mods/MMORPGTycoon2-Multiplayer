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
    return (origin.action==28||origin.action==29||origin.action==30)&&
        (origin.action==29?game::monster_move_key::valid(origin.key):game::monster_target_key::valid(origin.key))&&origin.region>=0&&
        (origin.scene=="world"||game::dungeon_gizmo_identity::scene(origin.scene))&&
        valid_monster_actor(origin.before)&&valid_monster_actor(origin.after)&&
        ((origin.action==28&&origin.before==MonsterActor{}&&origin.after!=MonsterActor{})||
         (origin.action==29&&origin.before!=MonsterActor{}&&origin.after!=MonsterActor{})||
         (origin.action==30&&origin.before!=MonsterActor{}&&origin.after==MonsterActor{}))&&
        monster_history_digest(value.quests_before)&&monster_history_digest(value.quests_after)&&
        monster_history_digest(value.references_before)&&monster_history_digest(value.references_after)&&
        value.count_before<=4096&&value.count_after<=4096&&value.completed_before>=0&&value.completed_after>=0&&
        uint32_t(value.completed_before)<=value.count_before&&uint32_t(value.completed_after)<=value.count_after;
}
inline const char* monster_history_uid_serial_error(bool restoring,int32_t before,int32_t after,const MonsterActor& actor){
    if(before<0||after<0)return "UID spawn serial is negative";
    if(int64_t(after)-before!=(restoring?1:0))return "UID spawn serial delta differs";
    if(restoring&&actor.tag!=before)return "restored actor UID spawn serial differs";
    return nullptr;
}
inline const char* monster_history_transition_error(bool undo,const MonsterHistoryProof& proof,const MonsterState& before,const MonsterState& after){
    if(!valid_monster_history(proof))return "history proof invalid";
    if(!valid_monster_state(before))return "before actor/roster invalid";
    if(!valid_monster_state(after))return "after actor/roster invalid";
    if(before.roster==ZoneState{}||after.roster==ZoneState{})return "roster absent";
    const auto& a=before.roster;const auto& b=after.roster;const auto& origin=proof.origin;
    if(a.zones!=b.zones)return "Zone count changed";
    if(a.region_random!=b.region_random)return "Region RNG changed";
    if(a.release_edits!=b.release_edits)return "release edit counters changed";
    if(undo?(a.undo_count==0||b.undo_count+1!=a.undo_count):(b.undo_count!=std::min(a.undo_count+1,20u)))return "native Undo count differs";
    if(origin.action==29){
        if(a.monsters!=b.monsters)return "Move Monster count changed";
        if(a.next_tag!=b.next_tag)return "Move spawn serial changed";
        if(a.created_monsters!=b.created_monsters)return "Move creation counter changed";
        if(!monster_shared_actor(before.actor,undo?origin.after:origin.before))return "Move source actor differs";
        if(!monster_shared_actor(after.actor,undo?origin.before:origin.after))return "Move destination actor differs";
        if(proof.quests_before!=proof.quests_after)return "Move Quest digest changed";
        if(proof.references_before!=proof.references_after)return "Move Subscriber references changed";
        if(proof.count_before!=proof.count_after)return "Move Quest count changed";
        if(proof.completed_before!=proof.completed_after)return "Move completed Quest count changed";
        return nullptr;
    }
    if(int64_t(b.created_monsters)-a.created_monsters!=1)return "creation counter delta differs from one";
    const auto& alive=origin.action==28?origin.after:origin.before;
    const bool restoring=origin.action==28?!undo:undo;
    if(restoring){
        if(before.actor!=MonsterActor{})return "restore source actor is present";
        if(!monster_shared_actor(after.actor,alive))return "restored actor differs from original";
        if(b.monsters!=a.monsters+1)return "restored Monster count delta differs from one";
        // UID restoration advances a separate native counter from ordinary placement.
        if(b.next_tag!=a.next_tag)return "restore changed ordinary spawn serial";
        return nullptr;
    }
    if(!monster_shared_actor(before.actor,alive))return "removed source actor differs from original";
    if(after.actor!=MonsterActor{})return "removed actor remains present";
    if(a.monsters!=b.monsters+1)return "removed Monster count delta differs from one";
    if(a.next_tag!=b.next_tag)return "remove spawn serial changed";
    return nullptr;
}
inline bool monster_history_transition(bool undo,const MonsterHistoryProof& proof,const MonsterState& before,const MonsterState& after){
    return monster_history_transition_error(undo,proof,before,after)==nullptr;
}
}

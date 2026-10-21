#pragma once
#include "zone_action.h"
namespace mt2mp::action_session {
struct MonsterIntent {
    int32_t region=-1,character_slot=-1,character_type=-1,level=0,rank=0;
    std::string tool;
    uint8_t loose=0;
    std::array<float,3> position{},scale{1,1,1};
    std::array<float,4> rotation{0,0,0,1};
    bool operator==(const MonsterIntent&)const=default;
};
struct MonsterRemoveIntent {
    int32_t region=-1;
    bool operator==(const MonsterRemoveIntent&)const=default;
};
struct MonsterMoveIntent {
    int32_t region=-1;
    std::array<float,3> position{},scale{1,1,1};
    std::array<float,4> rotation{0,0,0,1};
    bool operator==(const MonsterMoveIntent&)const=default;
};
struct MonsterActor {
    int32_t tag=-1,status=0,rank=0,character_slot=-1,character_type=-1,level=0;
    std::string name;
    std::array<float,3> position{},scale{};
    std::array<float,4> rotation{};
    bool operator==(const MonsterActor&)const=default;
};
struct MonsterState {
    ZoneState roster;
    MonsterActor actor;
    bool operator==(const MonsterState&)const=default;
};
inline bool monster_pose(const std::array<float,3>& p,const std::array<float,4>& r,const std::array<float,3>& s){
    for(float v:p)if(!std::isfinite(v)||std::abs(v)>1000000.f)return false;
    float norm=0;for(float v:r){if(!std::isfinite(v))return false;norm+=v*v;}
    if(!std::isfinite(norm)||std::abs(norm-1.f)>0.001f)return false;
    for(float v:s)if(!std::isfinite(v)||v<=0||v>1000.f)return false;
    return true;
}
inline bool valid_monster_move(const MonsterMoveIntent& v){
    return v.region>=0&&monster_pose(v.position,v.rotation,v.scale);
}
inline bool monster_move_transition(const MonsterMoveIntent& intent,const MonsterState& a,const MonsterState& b);
inline bool valid_monster_intent(const MonsterIntent& v){
    return v.region>=0&&v.tool=="item_elitemonster"&&v.character_slot==1&&v.character_type>=0&&
        v.level>=1&&v.rank>0&&v.loose==0&&monster_pose(v.position,v.rotation,v.scale);
}
inline bool valid_monster_actor(const MonsterActor& v){
    return v==MonsterActor{}||(v.status==3&&v.rank>=0&&v.rank<=2&&v.character_slot==1&&v.character_type>=0&&
        v.level>=0&&!v.name.empty()&&v.name.size()<=256&&v.name.find('\0')==std::string::npos&&
        monster_pose(v.position,v.rotation,v.scale));
}
inline bool valid_monster_state(const MonsterState& v){return valid_zone_state(v.roster)&&valid_monster_actor(v.actor);}
inline bool monster_shared_actor(const MonsterActor& a,const MonsterActor& b){
    // The original Spawn serial is neither persisted nor an object identity.
    auto local=a,remote=b;local.tag=remote.tag=0;return local==remote;
}
inline bool monster_shared_state(const MonsterState& a,const MonsterState& b){
    return zone_shared_state(a.roster,b.roster)&&monster_shared_actor(a.actor,b.actor);
}
inline bool monster_move_transition(const MonsterMoveIntent& intent,const MonsterState& a,const MonsterState& b){
    if(!valid_monster_move(intent)||!valid_monster_state(a)||!valid_monster_state(b)||
       a.roster==ZoneState{}||b.roster==ZoneState{}||a.actor==MonsterActor{}||b.actor==MonsterActor{})return false;
    const auto& x=a.roster;const auto& y=b.roster;
    if(!x.monsters||x.monsters!=y.monsters||x.zones!=y.zones||x.next_tag!=y.next_tag||
       x.created_monsters!=y.created_monsters||x.undo_count>20||y.undo_count!=std::min(x.undo_count+1,20u)||
       x.region_random!=y.region_random||x.release_edits!=y.release_edits)return false;
    auto expected=a.actor;expected.position=intent.position;expected.rotation=intent.rotation;expected.scale=intent.scale;
    return b.actor==expected;
}
inline bool monster_remove_transition(const MonsterState& a,const MonsterState& b){
    if(!valid_monster_state(a)||!valid_monster_state(b)||a.actor==MonsterActor{}||b.actor!=MonsterActor{})return false;
    const auto& x=a.roster;const auto& y=b.roster;
    return x.monsters&&x.monsters==y.monsters+1&&x.zones==y.zones&&x.next_tag==y.next_tag&&
        int64_t(y.created_monsters)-x.created_monsters==1&&x.undo_count<=20&&y.undo_count==std::min(x.undo_count+1,20u)&&
        x.region_random==y.region_random&&x.content_sha256!=y.content_sha256;
}
inline const char* monster_transition_error(const MonsterIntent& intent,const MonsterState& a,const MonsterState& b){
    if(!valid_monster_intent(intent))return "intent invalid";
    if(!valid_monster_state(a))return "before state invalid";
    if(!valid_monster_state(b))return "after state invalid";
    if(a.roster==ZoneState{}||b.roster==ZoneState{})return "roster absent";
    if(a.actor!=MonsterActor{})return "before actor present";
    if(b.actor==MonsterActor{})return "after actor absent";
    const auto& x=a.roster;const auto& y=b.roster;
    if(x.monsters>=4096||y.monsters!=x.monsters+1)return "monster count differs";
    if(y.zones!=x.zones)return "Zone count differs";
    if(x.undo_count>=100000||y.undo_count!=x.undo_count+1)return "Undo count differs";
    if(int64_t(y.next_tag)-x.next_tag!=1)return "next tag differs";
    if(int64_t(y.created_monsters)-x.created_monsters!=1)return "created count differs";
    if(x.region_random!=y.region_random)return "Region RNG differs";
    if(x.content_sha256==y.content_sha256)return "content unchanged";
    for(size_t i=0;i<8;++i)if(int64_t(y.release_edits[i])-x.release_edits[i]!=(i==6?1:0))return "Release edits differ";
    const auto& actor=b.actor;
    if(actor.tag!=x.next_tag)return "actor tag differs";
    if(actor.rank!=intent.rank)return "actor rank differs";
    if(actor.level!=intent.level)return "actor level differs";
    if(actor.character_slot!=intent.character_slot||actor.character_type!=intent.character_type)return "actor character differs";
    if(actor.position!=intent.position)return "actor position differs";
    if(actor.rotation!=intent.rotation)return "actor rotation differs";
    // SpawnNewMonsterHere copies position and rotation from the preview Transform.
    // Its new Prop keeps unit scale; the Elite preview uses a separate rank scale.
    if(actor.scale!=std::array<float,3>{1,1,1})return "actor native unit scale differs";
    return nullptr;
}
inline bool monster_transition(const MonsterIntent& intent,const MonsterState& a,const MonsterState& b){
    return monster_transition_error(intent,a,b)==nullptr;
}
}

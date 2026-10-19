#pragma once
#include "terrain_action.h"
#include <limits>
namespace mt2mp::action_session {
struct ZoneStamp {
    std::array<float,3> position{};
    TerrainShape shape;
    bool operator==(const ZoneStamp&)const=default;
};
struct ZoneIntent {
    int32_t region=-1,character_type=-1,level=0;
    std::vector<ZoneStamp> stamps;
    bool operator==(const ZoneIntent&)const=default;
};
struct ZoneState {
    uint32_t zones=0,monsters=0,undo_count=0;
    int32_t next_tag=0,created_monsters=0;
    std::array<uint64_t,4> global_random{},region_random{};
    std::array<int32_t,8> release_edits{};
    std::string content_sha256;
    bool operator==(const ZoneState&)const=default;
};
inline bool valid_zone_intent(const ZoneIntent& v){
    if(v.region<0||v.character_type<0||v.level<1||v.stamps.empty()||v.stamps.size()>64)return false;
    size_t points=0;
    for(const auto& s:v.stamps){
        for(float f:s.position)if(!std::isfinite(f)||std::abs(f)>1000000.f)return false;
        if(s.shape.empty())return false;
        for(const auto& path:s.shape){
            if(path.size()<3||path.size()>1024-points)return false;points+=path.size();
            for(const auto& p:path)if(p.x<-(INT64_MAX>>2)||p.x>(INT64_MAX>>2)||p.z<-(INT64_MAX>>2)||p.z>(INT64_MAX>>2))return false;
        }
    }
    return true;
}
inline bool valid_zone_state(const ZoneState& v){
    if(v==ZoneState{})return true;
    if(v.zones>4096||v.monsters>4096||v.undo_count>100000||v.next_tag<0||v.created_monsters<0||v.content_sha256.size()!=64)return false;
    for(char c:v.content_sha256)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
// Region RNG is observed locally but is not consumed by this original action.
inline bool zone_shared_content(const ZoneState& a,const ZoneState& b){
    return a.zones==b.zones&&a.monsters==b.monsters&&a.undo_count==b.undo_count&&
        a.created_monsters==b.created_monsters&&
        a.release_edits==b.release_edits&&a.content_sha256==b.content_sha256;
}
inline bool zone_shared_state(const ZoneState& a,const ZoneState& b){
    return zone_shared_content(a,b)&&a.global_random==b.global_random;
}
inline bool zone_transition(const ZoneState& a,const ZoneState& b){
    if(!valid_zone_state(a)||!valid_zone_state(b)||a==ZoneState{}||b==ZoneState{}||
       a.zones>=4096||b.zones!=a.zones+1||b.monsters<a.monsters||a.undo_count>=100000||
       b.undo_count!=a.undo_count+1||a.region_random!=b.region_random||a.content_sha256==b.content_sha256)return false;
    const auto spawned=b.monsters-a.monsters;
    if(int64_t(b.next_tag)-a.next_tag!=spawned||int64_t(b.created_monsters)-a.created_monsters!=spawned)return false;
    for(size_t i=0;i<8;++i){if(i==5){if(b.release_edits[i]<=a.release_edits[i])return false;}else if(a.release_edits[i]!=b.release_edits[i])return false;}
    return true;
}
}

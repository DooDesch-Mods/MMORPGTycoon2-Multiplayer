#pragma once
#include "zone_action.h"
#include "../snapshot_zone_key.h"
#include "../world_zone_key.h"
#include <string_view>

namespace mt2mp::action_session {
enum class ZoneEditKind:uint8_t {Stroke=1,Apply=2,Reset=3,Respawn=4,Delete=5};
struct ZoneEditIntent {
    ZoneEditKind kind=ZoneEditKind::Stroke;
    int32_t region=-1;
    std::string selected;
    int32_t zone_type=-1,character_type=-1,level=0,density=-1;
    bool add=true;
    std::array<uint32_t,3> press{};
    std::vector<ZoneStamp> stamps;
    bool operator==(const ZoneEditIntent&)const=default;
};
struct ZoneEditState {
    std::string values_sha,quests_sha,references_sha;
    std::array<uint32_t,4> counts{};
    uint32_t monsters=0;
    std::array<uint64_t,4> global_random{},region_random{};
    int32_t next_tag=0,created_monsters=0;
    std::array<int32_t,8> release_edits{};
    bool operator==(const ZoneEditState&)const=default;
};
struct ZoneEditEffect {
    ZoneEditState before,after;
    std::string inverse_sha;
    uint32_t inverse_operations=0;
    bool operator==(const ZoneEditEffect&)const=default;
};
inline bool zone_edit_sha(std::string_view value){
    return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string_view::npos;
}
inline bool zone_edit_selected(std::string_view value){
    return game::snapshot_zone_key::valid(value)||game::world_zone_key::valid(value);
}
inline bool valid_zone_edit_intent(const ZoneEditIntent& value){
    if(value.region<0||uint8_t(value.kind)<1||uint8_t(value.kind)>5||
       (!value.selected.empty()&&!zone_edit_selected(value.selected)))return false;
    if(value.kind==ZoneEditKind::Stroke){
        if(value.zone_type<0||value.zone_type>3||value.character_type<0||value.level<1||value.density!=-1||
           (!value.add&&!value.selected.empty())||value.stamps.empty()||value.stamps.size()>64)return false;
        for(uint32_t bits:value.press){const float coordinate=std::bit_cast<float>(bits);
            if(!std::isfinite(coordinate)||std::abs(coordinate)>1000000.f)return false;}
        size_t points=0;
        for(const auto& stamp:value.stamps){
            for(float coordinate:stamp.position)if(!std::isfinite(coordinate)||std::abs(coordinate)>1000000.f)return false;
            if(stamp.shape.empty())return false;
            for(const auto& path:stamp.shape){
                if(path.size()<3||path.size()>1024-points)return false;
                points+=path.size();
                for(const auto& point:path)
                    if(point.x<-(INT64_MAX>>2)||point.x>(INT64_MAX>>2)||point.z<-(INT64_MAX>>2)||point.z>(INT64_MAX>>2))return false;
            }
        }
        return true;
    }
    if(value.selected.empty()||!value.add||value.press!=std::array<uint32_t,3>{}||!value.stamps.empty())return false;
    if(value.kind==ZoneEditKind::Apply)
        return value.zone_type==1&&value.character_type>=0&&value.level>=1&&value.density>=0&&value.density<=3;
    return value.zone_type==-1&&value.character_type==-1&&value.level==0&&value.density==-1;
}
inline bool valid_zone_edit_state(const ZoneEditState& value){
    return zone_edit_sha(value.values_sha)&&zone_edit_sha(value.quests_sha)&&zone_edit_sha(value.references_sha)&&
        value.next_tag>=0&&value.created_monsters>=0;
}
inline bool valid_zone_edit_effect(const ZoneEditEffect& value){
    if(!valid_zone_edit_state(value.before)||!valid_zone_edit_state(value.after))return false;
    return value.inverse_operations?zone_edit_sha(value.inverse_sha):value.inverse_sha.empty();
}
inline bool valid_zone_edit_effect(const ZoneEditIntent& intent,const ZoneEditEffect& value){
    return valid_zone_edit_intent(intent)&&valid_zone_edit_effect(value)&&
        (intent.kind==ZoneEditKind::Respawn||value.inverse_operations>0);
}
}

#pragma once
#include "typed_action/district_stroke_action.h"
#include "district_lifecycle.h"
#include <bit>
#include <cmath>
#include <set>

namespace mt2mp::game::district_stroke_state {
namespace a=action_session;
using Values=std::map<std::string,a::DistrictState>;
inline bool sha(const std::string& value){
    if(value.size()!=64)return false;
    for(auto c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
inline bool shape(const a::TerrainShape& value,size_t& remaining){
    if(value.size()>remaining)return false;
    remaining-=value.size();
    for(const auto& path:value){
        if(path.size()>remaining)return false;remaining-=path.size();
        for(const auto& point:path)if(point.x<-(INT64_MAX>>2)||point.x>(INT64_MAX>>2)||point.z<-(INT64_MAX>>2)||point.z>(INT64_MAX>>2))return false;
    }
    return true;
}
inline bool intent(const a::DistrictStrokeIntent& value){
    if(value.region<0||value.geo<0||value.geo>2||(!value.selected.empty()&&!district_lifecycle::valid_key(value.selected)))return false;
    for(const auto* values:{&value.press,&value.finish})for(auto bits:*values)if(!std::isfinite(std::bit_cast<float>(bits)))return false;
    for(const auto* values:{&value.color,&value.water_color})for(auto bits:*values)if(!std::isfinite(std::bit_cast<float>(bits)))return false;
    if(value.kind==a::DistrictStrokeKind::Delete)return !value.selected.empty()&&value.stamps.empty()&&value.clip.empty()&&
        value.press==std::array<uint32_t,3>{}&&value.finish==std::array<uint32_t,3>{}&&value.color==std::array<uint32_t,4>{}&&
        value.water_color==std::array<uint32_t,4>{}&&!value.change_color&&!value.change_water_color&&value.geo==0;
    if(value.kind!=a::DistrictStrokeKind::Paint&&value.kind!=a::DistrictStrokeKind::Erase)return false;
    if(value.stamps.empty()||value.stamps.size()>100000)return false;
    size_t remaining=1000000;
    if(!shape(value.clip,remaining))return false;
    for(const auto& stamp:value.stamps)if(stamp.route>2||stamp.shape.empty()||!shape(stamp.shape,remaining)||
        ((value.kind==a::DistrictStrokeKind::Erase)!=(stamp.route==1))||(stamp.route==2&&value.selected.empty()))return false;
    return value.kind!=a::DistrictStrokeKind::Erase||value.selected.empty();
}
inline bool state(const a::DistrictStrokeState& value){return value.count<=district_saved_identity::maximum_districts&&sha(value.values_sha);}
inline bool effect(const a::DistrictStrokeEffect& value){
    return state(value.before)&&state(value.after)&&sha(value.inverse_sha)&&value.inverse_operations>0&&value.inverse_operations<=100000;
}
inline bool changed_region(const Values& before,const Values& after,int32_t region,std::string& error){
    for(const auto& [key,value]:before)if(value.region!=region){
        const auto found=after.find(key);
        if(found==after.end()||found->second!=value){error="District original changed an unrelated Region owner/value";return false;}
    }
    for(const auto& [key,value]:after)if(value.region!=region&&!before.contains(key)){
        error="District original created an owner in an unrelated Region";return false;
    }
    return true;
}
}

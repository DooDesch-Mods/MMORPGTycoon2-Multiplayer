#pragma once
#include "snapshot_building_key.h"

namespace mt2mp::game::snapshot_district_key {
inline std::string root_suffix(int32_t region,int32_t child){
    return "inline:region{"+std::to_string(region)+"}/districtManager/district["+std::to_string(child)+"]";
}
inline bool valid(std::string_view value){
    constexpr std::string_view domain=":world:district:";
    if(value.size()<=64+domain.size()||value.substr(0,64).find_first_not_of("0123456789abcdef")!=std::string_view::npos||
       value.substr(64,domain.size())!=domain)return false;
    const auto suffix=value.substr(64+domain.size());
    if(!suffix.starts_with("inline:"))return snapshot_building_key::number(suffix,INT32_MAX,true);
    constexpr std::string_view prefix="inline:region{",middle="}/districtManager/district[";
    if(!suffix.starts_with(prefix)||!suffix.ends_with(']'))return false;
    const auto split=suffix.find(middle,prefix.size());if(split==std::string_view::npos)return false;
    return snapshot_building_key::number(suffix.substr(prefix.size(),split-prefix.size()),INT32_MAX,false)&&
        snapshot_building_key::number(suffix.substr(split+middle.size(),suffix.size()-split-middle.size()-1),INT32_MAX,false);
}
}

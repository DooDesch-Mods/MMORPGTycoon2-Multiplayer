#pragma once
#include "snapshot_building_key.h"
#include <string>
#include <string_view>

namespace mt2mp::game::building_npc_key {
inline bool action(std::string_view value){
    if(value.size()!=43||!value.starts_with("action:"))return false;
    const auto id=value.substr(7);
    for(size_t i=0;i<id.size();++i){
        if(i==8||i==13||i==18||i==23){if(id[i]!='-')return false;}
        else if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return false;
    }return true;
}
inline bool parent(const std::string& value){return snapshot_building_key::valid(value)||action(value);}
inline std::string make(const std::string& building){return "building-npc:"+building;}
inline bool valid(std::string_view value){constexpr std::string_view prefix="building-npc:";return value.starts_with(prefix)&&parent(std::string(value.substr(prefix.size())));}
}

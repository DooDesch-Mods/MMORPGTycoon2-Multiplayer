#pragma once
#include "snapshot_building_key.h"
namespace mt2mp::game::snapshot_party_key {
inline std::string root_suffix(int32_t region,int32_t child){return "inline:region{"+std::to_string(region)+"}/partyManager/party["+std::to_string(child)+"]";}
inline std::string scene_suffix(int32_t region,int32_t exterior,int32_t instance,int32_t child){return "inline:dungeon:region{"+std::to_string(region)+"}/building["+std::to_string(exterior)+"]"+(instance<0?"/template/":"/instance["+std::to_string(instance)+"]/")+"party["+std::to_string(child)+"]";}
inline bool valid(std::string_view value){
    constexpr std::string_view domain=":world:party:";
    if(value.size()<=64+domain.size()||value.substr(0,64).find_first_not_of("0123456789abcdef")!=std::string_view::npos||value.substr(64,domain.size())!=domain)return false;
    auto suffix=value.substr(64+domain.size());
    if(!suffix.starts_with("inline:"))return snapshot_building_key::number(suffix,INT32_MAX,true);
    const auto number=[&](std::string_view end){const auto split=suffix.find(end);if(split==std::string_view::npos||!snapshot_building_key::number(suffix.substr(0,split),INT32_MAX,false))return false;suffix.remove_prefix(split+end.size());return true;};
    if(suffix.starts_with("inline:region{")){suffix.remove_prefix(14);if(!number("}/partyManager/party["))return false;}
    else if(suffix.starts_with("inline:dungeon:region{")){suffix.remove_prefix(22);if(!number("}/building[")||!number("]"))return false;
        if(suffix.starts_with("/template/party["))suffix.remove_prefix(16);
        else if(suffix.starts_with("/instance[")){suffix.remove_prefix(10);if(!number("]/party["))return false;}
        else return false;
    }else return false;
    return suffix.ends_with(']')&&snapshot_building_key::number(suffix.substr(0,suffix.size()-1),INT32_MAX,false);
}
}

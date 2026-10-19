#pragma once
#include "snapshot_building_key.h"
#include <string_view>

namespace mt2mp::game::group_object_key {
inline bool valid(std::string_view value){
    if(!value.starts_with("group:"))return false;value.remove_prefix(6);
    if(snapshot_building_key::valid(value))return true;
    if(value.size()!=43||!value.starts_with("action:"))return false;value.remove_prefix(7);
    for(size_t index=0;index<value.size();++index){
        if(index==8||index==13||index==18||index==23){if(value[index]!='-')return false;}
        else if(!((value[index]>='0'&&value[index]<='9')||(value[index]>='a'&&value[index]<='f')))return false;
    }
    return true;
}
}

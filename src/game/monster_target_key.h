#pragma once
#include "snapshot_monster_key.h"

namespace mt2mp::game::monster_target_key {
inline bool uuid(std::string_view value){
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){const auto c=value[i];
        if(i==8||i==13||i==18||i==23){if(c!='-')return false;}
        else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}
    return true;
}
inline bool valid(std::string_view key){return snapshot_monster_key::valid(key)||(key.starts_with("action:")&&uuid(key.substr(7)));}
inline bool input(std::string_view target){return uuid(target)||snapshot_monster_key::valid(target);}
inline std::string canonical(const std::string& target){return snapshot_monster_key::valid(target)?target:"action:"+target;}
inline std::string argument(const std::string& key){return key.starts_with("action:")?key.substr(7):key;}
inline bool request(const std::string& key,const std::string& action){
    return action.empty()?snapshot_monster_key::valid(key):(uuid(action)&&key=="action:"+action);
}
}

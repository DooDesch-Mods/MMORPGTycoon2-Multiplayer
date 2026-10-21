#pragma once
#include "monster_target_key.h"
#include <charconv>
#include <vector>
namespace mt2mp::game::world_zone_key {
inline bool number(std::string_view text,int32_t minimum,int32_t maximum){
    int32_t value=0;const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    return result.ec==std::errc{}&&result.ptr==text.data()+text.size()&&value>=minimum&&value<=maximum&&std::to_string(value)==text;
}
inline bool valid(std::string_view key){
    if(key.starts_with("action:"))return monster_target_key::uuid(key.substr(7));
    if(key.size()<84||key.size()>240)return false;
    for(size_t i=0;i<64;++i)if(!((key[i]>='0'&&key[i]<='9')||(key[i]>='a'&&key[i]<='f')))return false;
    if(key.substr(64,17)!=":world:zone-type:")return false;
    key.remove_prefix(81);
    std::vector<std::string_view> parts;
    for(;;){const auto colon=key.find(':');parts.push_back(key.substr(0,colon));if(colon==std::string_view::npos)break;key.remove_prefix(colon+1);}
    if(parts.empty()||(parts[0]!="0"&&parts[0]!="2"&&parts[0]!="3"))return false;
    if(parts.size()==3&&parts[1]=="record")return number(parts[2],1,INT32_MAX);
    if(parts.size()==5&&parts[1]=="root"&&parts[3]=="child")return number(parts[2],0,INT32_MAX)&&number(parts[4],0,4095);
    if(parts.size()==7&&parts[1]=="scene"&&parts[5]=="child")return number(parts[2],0,INT32_MAX)&&number(parts[3],0,INT32_MAX)&&number(parts[4],-1,INT32_MAX)&&number(parts[6],0,4095);
    return false;
}
}

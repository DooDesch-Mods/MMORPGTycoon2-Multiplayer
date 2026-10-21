#pragma once
#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::costume_weapon_read {
template<class Read,class Text>
bool command(Read&& read,Text&& text,uintptr_t record,std::string& category,bool& enabled,std::string& error){
    category.clear();enabled=false;uintptr_t tokens=0;int32_t count=0,capacity=0,kind=0,integer=0,first_kind=0;
    if(!read(record,0x38,tokens,error)||!read(record,0x40,count,error)||!read(record,0x44,capacity,error))return false;
    if(!tokens||count!=2||capacity<count||tokens>UINTPTR_MAX-0x60){error="Costume weapon original command token bounds differ";return false;}
    if(!read(tokens,0,first_kind,error)||!read(tokens,0x30,kind,error)||!read(tokens,0x58,integer,error)||!text(tokens+8,category,error))return false;
    if((first_kind!=1&&first_kind!=2)||kind!=4||(integer!=0&&integer!=1)||category.empty()||category.size()>4096){error="Costume weapon original checkbox token type/value differs";return false;}
    uintptr_t again=0;int32_t size=0,cap=0;
    if(!read(record,0x38,again,error)||!read(record,0x40,size,error)||!read(record,0x44,cap,error))return false;
    if(again!=tokens||size!=count||cap!=capacity){error="Costume weapon original command changed during read";return false;}
    enabled=integer!=0;return true;
}
// CharacterDefinition::Add/RemoveWeaponCategory use the mmoSet at +c0.
// Its original ordered string array is +d8, count +e0 and capacity +e4.
template<class Read,class Text>
bool categories(Read&& read,Text&& text,uintptr_t definition,std::vector<std::string>& out,std::string& error){
    out.clear();uintptr_t data=0;int32_t count=0,capacity=0;
    if(!read(definition,0xd8,data,error)||!read(definition,0xe0,count,error)||!read(definition,0xe4,capacity,error))return false;
    if(count<0||count>4096||capacity<count||(count&&!data)||data>UINTPTR_MAX-size_t(count)*32){error="Costume weapon category array bounds differ";return false;}
    std::set<std::string> unique;
    for(int32_t i=0;i<count;++i){std::string value;
        if(!text(data+size_t(i)*32,value,error))return false;
        if(value.empty()||value.size()>4096||!unique.insert(value).second){error="Costume weapon category is empty, too long or duplicated";return false;}
        out.push_back(std::move(value));}
    uintptr_t again=0;int32_t size=0,cap=0;
    if(!read(definition,0xd8,again,error)||!read(definition,0xe0,size,error)||!read(definition,0xe4,cap,error))return false;
    if(again!=data||size!=count||cap!=capacity){error="Costume weapon category array changed during read";return false;}
    return true;
}
}

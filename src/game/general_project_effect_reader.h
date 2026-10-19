#pragma once
#include "general_project_state.h"
namespace mt2mp::game::general_project {
template<class Read>bool array(Read&,uintptr_t,size_t,std::vector<uintptr_t>&,std::string&);
struct EffectOwners {
    uintptr_t features=0,values=0;
    std::array<uintptr_t,2> arrays{};
    std::vector<uintptr_t> modifiers,value_modifiers;
    bool operator==(const EffectOwners&)const=default;
};
template<class Read,class Text>
bool effects(Read& read,Text& text,uintptr_t image,const std::map<std::string,uintptr_t>& tech_keys,
             State& state,EffectOwners& owners,std::string& e){
    owners={};uintptr_t ap=0;
    if(!read(image,0x13df3c0,owners.features,e)||!read(owners.features,0,ap,e)||ap!=image+0x16ffdd0||
       !read(owners.features,0x27,state.features,e)||!read(image,0x13df300,owners.values,e)||
       !read(owners.values,0,ap,e)||ap!=image+0x1746a90){if(e.empty())e="General project GameFeatures/GameValues owner differs";return false;}
    std::array<std::vector<std::vector<uintptr_t>>,2> membership;
    for(size_t type=0;type<2;++type){
        const size_t offset=type?0x20:8;int32_t count=0,capacity=0;
        if(!read(owners.values,offset,owners.arrays[type],e)||!read(owners.values,offset+8,count,e)||!read(owners.values,offset+12,capacity,e))return false;
        if(count<0||count>4096||capacity<count||capacity>65536||(!owners.arrays[type]&&capacity)||owners.arrays[type]>UINTPTR_MAX-size_t(capacity)*64){e="General project GameValues array extent invalid";return false;}
        state.values[type].clear();membership[type].resize(count);
        for(int32_t i=0;i<count;++i){
            const auto record=owners.arrays[type]+size_t(i)*64;State::Value row;std::vector<uintptr_t> mods;
            if(!text(record,row.name,e)||!read(record,0x20,row.base,e)||!read(record,0x24,row.current,e)||!read(record,0x28,ap,e)||ap!=image+0x1747a50||!array(read,record,0x30,mods,e)){if(e.empty())e="General project GameValue current modifier-array class differs";return false;}
            for(auto mod:mods){std::array<uint32_t,2> payload{};if(!read(mod,0,payload,e)||payload[0]>2){if(e.empty())e="General project active GameValue modifier mode invalid";return false;}row.mods.push_back(payload);owners.value_modifiers.push_back(mod);}
            membership[type][i]=std::move(mods);state.values[type].push_back(std::move(row));
        }
    }
    std::set<uintptr_t> owned_mods;
    for(auto& tech:state.techs){
        const auto found=tech_keys.find(tech.key);std::vector<uintptr_t> mods;
        if(found==tech_keys.end()||!array(read,found->second,0x158,mods,e))return false;
        tech.mods.clear();
        for(auto mod:mods){State::Mod value;uint8_t integer=0,active=0;
            if(!owned_mods.insert(mod).second||!read(mod,0,ap,e)||ap!=image+0x1700150||!read(mod,0x40,value.value_id,e)||
               !read(mod,0x44,value.payload,e)||!read(mod,0x4c,integer,e)||integer>1||!read(mod,0x4d,active,e)||active>1||value.payload[0]>2){if(e.empty())e="General project owned Tech modifier class/fields differ";return false;}
            value.integer=integer;value.active=active;const size_t type=integer?0:1;
            if(value.value_id<0||size_t(value.value_id)>=membership[type].size()){e="General project Tech modifier actual GameValue index invalid";return false;}
            const auto& rows=membership[type][value.value_id];const auto occurrences=std::count(rows.begin(),rows.end(),mod+0x44);
            if(occurrences!=(active?1:0)){e="General project Tech modifier active flag/current GameValue membership differs";return false;}
            owners.modifiers.push_back(mod);tech.mods.push_back(value);
        }
    }
    e.clear();return true;
}
}

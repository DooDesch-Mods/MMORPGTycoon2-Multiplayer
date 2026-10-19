#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace mt2mp::game::prop_text_dungeon {
using Owners=std::array<uintptr_t,3>;
template<class Read,class Text>bool read(Read& at,Text& text,uintptr_t image,uintptr_t root,uintptr_t building,
    Owners& owners,std::string& name,std::string& region_name,std::string& e){
    owners={};name.clear();region_name.clear();
    if(!at(building,0xa38,owners[0],e))return false;
    if(!owners[0])return true;
    uintptr_t ap=0,reverse=0;int32_t shard_type=-1;
    if(!at(building,0,ap,e)||ap!=image+0x16d93c0||!at(building,0x170,reverse,e)||reverse!=root||
       !at(owners[0],0,ap,e)||ap!=image+0x16c0560||!at(owners[0],0x6f8,reverse,e)||reverse!=building||
       !at(owners[0],0x48,owners[1],e)||!at(owners[1],0,ap,e)||ap!=image+0x174ad20||
       !at(owners[1],0x48,shard_type,e)||shard_type!=1||!at(owners[1],0x18,reverse,e)||reverse!=owners[0]||
       !at(owners[0],0x50,owners[2],e)||!at(owners[2],0,ap,e)||ap!=image+0x1787930||
       !at(owners[2],0x70,reverse,e)||reverse!=owners[1]||
       !text(owners[0]+0x28,name,e)||!text(owners[2]+0x810,region_name,e)){
        if(e.empty())e="Prop text Dungeon exterior/template Shard/Region reciprocal owner differs";return false;}
    return true;
}
}

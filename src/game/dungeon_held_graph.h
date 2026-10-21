#pragma once
#include "dungeon_scene_roster.h"
#include <algorithm>

namespace mt2mp::game::dungeon_held_graph {
template<class Read>
bool member(Read& at,uintptr_t image,uintptr_t graph,uintptr_t node,std::string& error){
    uintptr_t ap=0;int32_t count=0;
    if(!graph||!node||graph>UINTPTR_MAX-0x30||!at(graph,0x18,ap,error)||ap!=image+0x16e2dc0||
       !at(graph,0x28,count,error)||count<1||count>100000){
        if(error.empty())error="Held Dungeon graph original operation array differs";return false;
    }
    dungeon_scene_roster::Array operations;
    if(!dungeon_scene_roster::array(at,graph+0x18,operations,error))return false;
    if(std::count(operations.members.begin(),operations.members.end(),node)!=1){
        error="Held Dungeon Make node is not an exact current graph member";return false;
    }
    return dungeon_scene_roster::unchanged(at,graph+0x18,operations,error);
}
}

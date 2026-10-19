#pragma once
#include "boss_identity.h"
#include "monster_zone_read.h"
namespace mt2mp::game::interventions {
// Scope is from boss_current's complete current Root/Dungeon scene census.
// The original roster reader keeps all member, weak-link, shape and RNG checks.
template<class Memory>bool monster_roster(Memory& memory,uintptr_t image,uintptr_t map,const boss_identity::Scope& scene,monster_zone_read::State& out,std::string& error){
    npc_quest_targets::World world;world.image=image;world.map=map;world.root=scene.shard;world.region=scene.region;
    const auto owners=[&](auto& source,const auto& current,uintptr_t& buildings,uintptr_t& zones,std::string& cause){
        namespace q=npc_quest_targets;uintptr_t root=0,region=0,owner=0;
        if(!boss_identity::valid(scene)||current.root!=scene.shard||current.region!=scene.region)return q::fail(cause,"Intervention Monster scene requires a completed canonical owner");
        if(!q::at(source,scene.region,0x70,root,cause)||root!=scene.shard||!q::at(source,scene.region,0x950,buildings,cause)||!q::at(source,scene.region,0x968,zones,cause)||
           !q::typed(source,image,buildings,0x17183c0,cause)||!q::typed(source,image,zones,0x16fe170,cause)||!q::at(source,buildings,0x450,root,cause)||!q::at(source,buildings,0x458,region,cause)||!q::at(source,zones,0xf0,owner,cause))return false;
        return (root==scene.shard&&region==scene.region&&owner==scene.region)||q::fail(cause,"Intervention Monster explicit scene manager reciprocity differs");
    };
    return monster_zone_read::read_owned(memory,world,owners,out,error);
}
}

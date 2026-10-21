#pragma once
#include "state.h"
#include "../dungeon_scene_identity_runtime.h"
#include "../npc_quest_targets.h"
#include "../monster_saved_identity.h"
#include "../direct_monster_identity.h"
namespace mt2mp::game::scene_clone {
struct Member {
    Family family=Family::Gizmo;
    uint32_t child=0;
    uintptr_t object=0;uint64_t uid=0;
    std::string key;
    std::string origin;
    uint32_t ordinal=0;
    bool operator==(const Member&)const=default;
};
struct Pool {
    dungeon_scene_roster::Scene scene;
    std::array<uintptr_t,4> managers{};
    uintptr_t building_manager=0;
    mt2mp::build_actions::private_native::place::SceneryWitness scenery;
    npc_quest_targets::Array zones;
    monster_saved_identity::Pool monsters;
    std::vector<Member> members;
};
struct Raw {
    dungeon_scene_identity_runtime::World world;
    std::vector<Pool> pools;
};
inline direct_monster_identity::Scope direct_scope(const Raw& raw,const Pool& pool){
    return {raw.world.uuid,raw.world.world_epoch,raw.world.generation,uintptr_t(raw.world.map)+0x1c0,
        pool.scene.owner.region,pool.managers[2],pool.scene.owner.key};
}
bool read_raw(Raw&,std::string&);
bool read_held_template(const Raw&,const std::string&,uintptr_t,Raw&,std::string&);
inline bool held_member(const Raw* held,const std::string& scene,uintptr_t object,uint64_t uid){
    if(!held||!object||!uid)return false;
    for(const auto& pool:held->pools)if((pool.scene.owner.type==1||pool.scene.owner.type==2)&&pool.scene.owner.key==scene)
        for(const auto& member:pool.members)if(member.object==object&&member.uid==uid)return true;
    return false;
}
bool find_uid(uint64_t,uintptr_t&,std::string&);
bool same_world(const dungeon_scene_identity_runtime::World&,const dungeon_scene_identity_runtime::World&);
bool same_owner(const Pool&,const Pool&);
// Departed members require original deletion or the exact graph-owned template.
// No surviving scene may lose or replace a previously confirmed full UID here.
struct Changes {std::vector<Assignment> births,retired,restored;};
bool plan(const Raw& before,const Raw& after,const std::string& operation,const Mapping& previous,const Mapping* authority,Mapping&,Changes&,std::string&,const std::string& template_exterior={},const Raw* restoration=nullptr);
bool retirements_ready(const Raw&,const Mapping&,const std::function<bool(uint64_t,uintptr_t&,std::string&)>&,std::string&,const Raw* held=nullptr);
}

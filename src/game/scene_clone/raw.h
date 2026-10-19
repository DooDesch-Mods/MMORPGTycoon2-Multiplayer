#pragma once
#include "state.h"
#include "../dungeon_scene_identity_runtime.h"
#include "../npc_quest_targets.h"
#include "../monster_saved_identity.h"
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
    npc_quest_targets::Array zones;
    monster_saved_identity::Pool monsters;
    std::vector<Member> members;
};
struct Raw {
    dungeon_scene_identity_runtime::World world;
    std::vector<Pool> pools;
};
bool read_raw(Raw&,std::string&);
bool find_uid(uint64_t,uintptr_t&,std::string&);
bool same_world(const dungeon_scene_identity_runtime::World&,const dungeon_scene_identity_runtime::World&);
bool same_owner(const Pool&,const Pool&);
// Every departed scene member must have completed original native deletion.
// No surviving scene may lose or replace a previously confirmed full UID here.
struct Changes {std::vector<Assignment> births,retired;};
bool plan(const Raw& before,const Raw& after,const std::string& operation,const Mapping& previous,const Mapping* authority,Mapping&,Changes&,std::string&);
bool retirements_ready(const Raw&,const Mapping&,const std::function<bool(uint64_t,uintptr_t&,std::string&)>&,std::string&);
}

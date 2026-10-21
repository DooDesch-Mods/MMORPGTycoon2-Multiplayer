#pragma once
#include "world_zone_runtime.h"
#include "npc_quest_targets.h"
#include "dungeon_zone_roster.h"

namespace mt2mp::game::world_zone_runtime {
struct Edit {
    action_session::Binding binding;
    std::string snapshot,operation;
    npc_quest_targets::World world;
    npc_quest_targets::SavedPool monsters;
    std::vector<Current> before;
    bool prepared=false,published=false;
};
struct Preview {
    dungeon_zone_roster::Census raw;
    npc_quest_targets::SavedPool monsters;
    std::vector<Current> rows;
    uintptr_t born=0;uint64_t born_uid=0;
    std::map<uint64_t,Current> restored;
};
bool prepare_edit(const action_session::Binding&,const std::string& snapshot,int32_t region,
    const std::string& operation,Edit&,std::string&);
bool preview_edit(const Edit&,uintptr_t born,uint64_t born_uid,const std::map<uint64_t,Current>& restored,
    Preview&,std::string&);
bool publish_edit(Edit&,const Preview&,std::string&);
}

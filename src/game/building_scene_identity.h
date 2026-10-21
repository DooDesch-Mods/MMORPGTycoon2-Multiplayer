#pragma once
#include "scene_clone/raw.h"
#include <memory>

namespace mt2mp::game::remove_diagnostics::scene_buildings {
struct Row {
    std::string key,scene;
    uintptr_t object=0,shard=0,region=0,manager=0;
    uint64_t uid=0;bool scenery=false;
};
struct Restore {Row before,after;};
struct State;
struct Preview {std::shared_ptr<State> state;};
bool resolve(const dungeon_scene_identity_runtime::World&,uintptr_t,uint64_t,std::string&,std::string&);
bool preview(const dungeon_scene_identity_runtime::World&,const std::vector<Row>&,
    const std::vector<Row>&,const std::vector<Restore>&,Preview&,std::string&,const scene_clone::Raw* held=nullptr);
bool check(const Preview&,std::string&);
bool publish(Preview&,std::string&);
}

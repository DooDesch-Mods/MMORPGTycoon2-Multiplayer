#pragma once
#include "scenery_scene_key.h"
#include "object_identity/remove_runtime/copy/scenery_witness.h"
namespace mt2mp::game::root_scenery_saved {
struct Row {std::string record,scene;uintptr_t object=0,manager=0,shard=0,region=0;uint64_t uid=0;};
inline bool append(const std::string& sha,int32_t region_id,
    const mt2mp::build_actions::private_native::place::SceneryWitness& witness,std::vector<Row>& out,std::string& error){
    if(!dungeon_gizmo_identity::digest(sha)||region_id<0||!witness.context.region||!witness.context.shard||!witness.context.manager||!witness.context.exact_build||!witness.context.main_thread||!witness.context.owned_world_valid||(!witness.context.epoch&&!witness.context.snapshot_observation)){error="Root Scenery saved census scope invalid";return false;}
    std::array<int32_t,47> children{};std::set<uintptr_t> pointers;std::set<uint64_t> uids;std::vector<Row> rows;
    int32_t previous=-1;
    for(const auto& member:witness.members){
        if(member.category<0||member.category>=47||member.category<previous||!member.object||!member.uid||
           !pointers.insert(member.object).second||!uids.insert(member.uid).second){error="Root Scenery saved categories or native members invalid/duplicated";return false;}
        previous=member.category;if(children[size_t(member.category)]==INT32_MAX){error="Root Scenery saved child ordinal overflow";return false;}const auto index=children[size_t(member.category)]++;
        const auto record=scenery_scene_key::root_record(sha,region_id,member.category,index);
        if(!scenery_scene_key::root_saved(record)){error="Root Scenery original category/child path invalid";return false;}
        rows.push_back({record,"world",member.object,witness.context.manager,witness.context.shard,witness.context.region,member.uid});
    }
    if(children!=witness.counts){error="Root Scenery complete47category saved child counts differ";return false;}
    out.insert(out.end(),rows.begin(),rows.end());error.clear();return true;
}
}

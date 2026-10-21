namespace dungeon_input {
namespace {
bool publish_birth(Memory& memory,const a::Binding& binding,const a::Request& request,const Sample& before,
                   const Port& port,std::string& error){
    const auto& intent=request.dungeon_input;
    if(intent.tool==a::DungeonInputTool::Paint)return true;
    const auto* pool=selected(before,intent);
    if(!pool||!port.object||!port.uid)return fail(error,"Dungeon input birth lacks its exact original returned object/full UID");
    if(intent.tool==a::DungeonInputTool::Door||intent.tool==a::DungeonInputTool::Key||intent.tool==a::DungeonInputTool::Treasure){
        dungeon_gizmo_identity::Member born{intent.scene,port.object,pool->managers[3],pool->scene.owner.shard,pool->scene.owner.region,port.uid};
        dungeon_scene_identity_runtime::ClonePreview preview;dungeon_scene_identity_runtime::Census actual;
        if(!dungeon_scene_identity_runtime::preview_clones(before.source.scenes.world,
            {{dungeon_gizmo_identity::birth(request.action_uuid,0),born}},{},preview,actual,error))return false;
        if(actual.pools.size()!=before.source.scenes.pools.size())return fail(error,"Dungeon input original Gizmo birth changed scene membership");
        size_t births=0;
        for(size_t i=0;i<actual.pools.size();++i){const auto& old=before.source.scenes.pools[i];const auto& next=actual.pools[i];
            if(old.scene.owner.key!=next.scene.owner.key||old.scene.owner.shard!=next.scene.owner.shard||old.scene.owner.region!=next.scene.owner.region||
               old.scene.owner.dungeon!=next.scene.owner.dungeon||old.managers!=next.managers||old.scenery!=next.scenery||old.zones.members!=next.zones.members)
                return fail(error,"Dungeon input original Gizmo birth changed a protected scene owner/Scenery/Zone pool");
            for(size_t type=0;type<4;++type)if(old.zones.arrays[type].data!=next.zones.arrays[type].data||
                old.zones.arrays[type].count!=next.zones.arrays[type].count||old.zones.arrays[type].capacity!=next.zones.arrays[type].capacity||
                old.zones.arrays[type].members!=next.zones.arrays[type].members)
                return fail(error,"Dungeon input original Gizmo birth changed a protected all-type Zone owner array");
            if(old.scene.owner.key!=intent.scene){if(old.local!=next.local||old.gizmos!=next.gizmos)return fail(error,"Dungeon input original Gizmo birth changed another scene");continue;}
            ++births;if(next.local.members.size()!=old.local.members.size()+1||next.gizmos.size()!=old.gizmos.size()+1||
                next.local.members.back().object!=port.object||next.local.members.back().uid!=port.uid||
                !std::equal(old.local.members.begin(),old.local.members.end(),next.local.members.begin())||
                !std::equal(old.gizmos.begin(),old.gizmos.end(),next.gizmos.begin()))return fail(error,"Dungeon input original Gizmo append changed existing members/order or returned another object");
            const auto& value=next.gizmos.back();
            if(value.type!=intent.type||value.variant!=intent.variant||!value.parent.empty()||value.position!=intent.position||
               value.rotation!=intent.rotation||value.scale!=intent.scale)return fail(error,"Dungeon input original Gizmo selected resource/pose differs");
        }
        if(births!=1)return fail(error,"Dungeon input original Gizmo birth did not occur in exactly one selected scene");
        return dungeon_scene_identity_runtime::publish_clones(preview,error);
    }
    std::vector<monster_zone_read::State> actual;
    if(!dungeon_cells::scene_monsters(memory,before.source.scenes,actual,error)||actual.size()!=before.monster_pools.size())return false;
    unsigned births=0;
    for(size_t i=0;i<actual.size();++i){const auto& old=before.monster_pools[i];const auto& next=actual[i];auto expected=old;
        expected.global_random=next.global_random;expected.next_monster_tag=next.next_monster_tag;
        if(before.source.scenes.pools[i].scene.owner.key!=intent.scene){if(expected!=next)return fail(error,"Dungeon input original Monster birth changed another scene/Zone/counter");continue;}
        ++births;
        if(next.monsters.size()!=old.monsters.size()+1||int64_t(next.created_monsters)-old.created_monsters!=1||
           int64_t(next.next_monster_tag)-old.next_monster_tag!=1)return fail(error,"Dungeon input original Monster birth did not append exactly one counted actor");
        const auto& born=next.monsters.back();
        if(born.pointer!=port.object||born.uid!=port.uid||born.zone||born.status!=3||born.tag!=old.next_monster_tag||
           born.rank!=intent.rank||born.slot!=1||born.type!=intent.character_type||born.level!=intent.level||
           born.position!=intent.position||born.rotation!=intent.rotation||born.scale!=std::array<float,3>{1,1,1})
            return fail(error,"Dungeon input original Monster return, selected Character/rank/level or pose differs");
        expected.created_monsters=next.created_monsters;expected.monsters.push_back(born);
        if(expected!=next)return fail(error,"Dungeon input original Monster birth changed protected existing actors/Zone/Region RNG");
    }
    if(births!=1)return fail(error,"Dungeon input original Monster birth did not occur in exactly one selected scene");
    const identity::Scope scope{request.snapshot_uuid,binding.native_epoch,binding.native_generation,
        uintptr_t(before.source.scenes.world.map)+0x1c0,pool->scene.owner.region,pool->managers[2],intent.scene};
    auto aliases=identity::registry;
    if(!aliases.stage(request.action_uuid,scope,port.object,port.uid,error)||!aliases.confirm(request.action_uuid,error))return false;
    std::vector<std::pair<std::string,identity::Entry>> observed;
    if(!dungeon_cells::monster_members(memory,before.source.scenes,observed,false,error,&aliases))return false;
    identity::registry=std::move(aliases);return true;
}
}
}

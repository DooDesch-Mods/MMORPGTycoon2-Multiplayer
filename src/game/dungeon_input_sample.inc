namespace dungeon_input {
namespace {
struct Sample {
    dungeon_cells::Sample source;
    a::DungeonInputState value;
    std::vector<monster_zone_read::State> monster_pools;
    std::vector<a::DungeonGizmoScene> gizmos;
    std::map<std::string,std::pair<std::string,dungeon_gizmo_control::Scenery>> scenery;
    struct Monster {std::string key,scene;a::MonsterActor value;};std::vector<Monster> monsters;
};
dungeon_gizmo_control::Scenery scenery_value(const native::BuildingState& value){
    dungeon_gizmo_control::Scenery out;out.type=value.type;out.variant=value.variant;
    for(size_t i=0;i<4;++i)out.pose[i]=std::bit_cast<uint32_t>(value.rotation[i]);
    out.pose[4]=std::bit_cast<uint32_t>(value.prop.x);out.pose[5]=std::bit_cast<uint32_t>(value.prop.y);out.pose[6]=std::bit_cast<uint32_t>(value.prop.z);
    for(size_t i=0;i<3;++i)out.pose[i+7]=std::bit_cast<uint32_t>(value.scale[i]);return out;
}
bool scenery_observed(Memory& memory,uintptr_t object,uint64_t uid,dungeon_gizmo_control::Scenery& out,std::string& error){
    native::BuildingState value;uint8_t automatic=2;
    if(!scenery_kiss::read_action_prop(uid,value,error))return false;
    if(value.prop.uid!=uid)return fail(error,"Scene Scenery native record full UID differs requested="+std::to_string(uid)+" actual="+std::to_string(value.prop.uid));
    out=scenery_value(value);
    if(!field(memory,object,0x270,out.height,error)||!field(memory,object,0x275,automatic,error)||automatic>1)
        return fail(error,error.empty()?"Scene Scenery height/automatic value differs":error);
    out.automatic=automatic!=0;return dungeon_gizmo_control::valid(out)||fail(error,"Scene Scenery complete native resource/pose/height invalid");
}
bool scenery_record(Memory& memory,const dungeon_scene_identity_runtime::Census& census,uintptr_t object,uint64_t uid,
                    dungeon_gizmo_control::SceneryRow& out,std::string& error){
    out={};if(!remove_diagnostics::scene_scenery::resolve(census.world,object,uid,out.key,error))return false;
    return scenery_observed(memory,object,uid,out.value,error);
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::DungeonInputIntent& intent,
            Sample& out,std::string& error,const qr::Witness* previous=nullptr,const std::string& source_action={}){
    out={};a::DungeonPaletteIntent target;target.key=intent.key;
    if(!dungeon_cells::sample(memory,binding,snapshot,target,out.source,error,previous)||
       !dungeon_cells::scene_monsters(memory,out.source.scenes,out.monster_pools,error))return false;
    out.value.protected_state=out.source.palette.value.protected_state;
    // Complete Region/Dungeon records remain in source for local lifetime and
    // graph diagnostics. Peer authored proof uses the original Canvas graph,
    // layout, full Gizmos and canonical membership, not private Actor motion.
    out.value.quests_sha256=out.source.value.stroke.quests_sha;
    if(!dungeon_palette::digest(out.source.palette.value,out.value.layout_sha256,error))return false;
    size_t gizmo=0;
    for(size_t i=0;i<out.source.scenes.pools.size();++i){
        const auto& pool=out.source.scenes.pools[i];a::DungeonGizmoScene scene;scene.key=pool.scene.owner.key;
        if(pool.gizmos.size()!=pool.local.members.size())return fail(error,"Dungeon input original complete Gizmo record/member count differs");
        for(const auto& row:pool.gizmos){
            if(gizmo>=out.source.scenes.keys.size())return fail(error,"Dungeon input complete Gizmo canonical roster truncated");
            scene.rows.push_back({out.source.scenes.keys[gizmo++],row});
        }
        out.gizmos.push_back(std::move(scene));
        if(intent.controls&&dungeon_gizmo_control::mixed(*intent.controls))for(const auto& member:pool.scenery.members){
            dungeon_gizmo_control::SceneryRow value;if(!scenery_record(memory,out.source.scenes,member.object,member.uid,value,error)||
               !out.scenery.emplace(value.key,std::pair{pool.scene.owner.key,value.value}).second)return fail(error,error.empty()?"Scene Scenery canonical roster repeats":error);}

        for(const auto& row:out.monster_pools[i].monsters){
            const auto alias=std::find_if(out.source.monsters.begin(),out.source.monsters.end(),[&](const auto& item){
                return item.second.scope.manager==pool.managers[2]&&item.second.pointer==row.pointer&&item.second.uid==row.uid;});
            if(alias==out.source.monsters.end())return fail(error,"Dungeon input current Monster lacks its confirmed scene/full-UID owner");
            Sample::Monster value;value.key=monster_target_key::canonical(alias->first);value.scene=pool.scene.owner.key;
            value.value={0,row.status,row.rank,row.slot,row.type,row.level,{},row.position,row.scale,row.rotation};
            if(!npc_census::read_name(memory,row.pointer,value.value.name,error))return false;
            out.monsters.push_back(std::move(value));
        }
    }
    out.value.dungeon_sha256=out.source.canvas_sha;
    out.value.scenes_sha256=out.source.value.stroke.scenes_sha;
    if(intent.tool==a::DungeonInputTool::Gizmos){
        out.value.edits.emplace();
        if(dungeon_gizmo_control::mixed(*intent.controls)){
            out.value.scenery_edits.emplace();out.value.scenery_release.emplace();uintptr_t manager=0,release=0,ap=0;
            if(!field(memory,state.image,0x13df4f0,manager,error)||!field(memory,manager,0,ap,error)||ap!=state.image+0x1714ed0||
               !field(memory,manager,0x38,release,error)||!field(memory,release,0,ap,error)||ap!=state.image+0x16c12b0||
               !field(memory,release,0x120,*out.value.scenery_release,error))return fail(error,error.empty()?"Scene Scenery current Release owner/counter unavailable":error);
        }
        for(size_t i=0;i<intent.controls->rows.size();++i){const auto& control=intent.controls->rows[i];
            const auto key=intent.controls->command==dungeon_gizmo_control::Command::Duplicate&&!source_action.empty()?
                dungeon_gizmo_control::birth(control,source_action,uint32_t(i)):control.key;
            if(control.scenery){auto found=out.scenery.find(key);
                if(found==out.scenery.end()&&intent.controls->command==dungeon_gizmo_control::Command::Duplicate&&key!=control.key)found=out.scenery.find(control.key);
                if(found!=out.scenery.end()){if(found->second.first!=intent.scene)return fail(error,"Scene Scenery control sampled another scene");
                    out.value.scenery_edits->push_back({found->first,found->second.second});}continue;}
            const auto size_before=out.value.edits->size();
            for(const auto& scene:out.gizmos)for(const auto& row:scene.rows)if(row.key==key){
                if(scene.key!=intent.scene)return fail(error,"Gizmo control sampled canonical owner left selected scene");
                out.value.edits->push_back(row);}
            if(size_before==out.value.edits->size()&&intent.controls->command==dungeon_gizmo_control::Command::Duplicate&&key!=control.key)
                for(const auto& scene:out.gizmos)for(const auto& row:scene.rows)if(row.key==control.key){
                    if(scene.key!=intent.scene)return fail(error,"Gizmo Duplicate source left selected scene");out.value.edits->push_back(row);}
        }
    }else if(!source_action.empty()&&intent.tool!=a::DungeonInputTool::Paint){
        if(intent.tool==a::DungeonInputTool::Monster){
            const auto key="action:"+source_action;
            for(const auto& row:out.monsters)if(row.key==key){
                if(out.value.birth)return fail(error,"Dungeon input source Monster canonical owner repeats");
                out.value.birth=a::DungeonInputBirth{row.key,row.scene,intent.tool,{},row.value};
            }
        }else{
            const auto key=dungeon_gizmo_identity::birth(source_action,0);
            for(const auto& scene:out.gizmos)for(const auto& row:scene.rows)if(row.key==key){
                if(out.value.birth)return fail(error,"Dungeon input source Gizmo canonical owner repeats");
                out.value.birth=a::DungeonInputBirth{row.key,scene.key,intent.tool,row.value,{}};
            }
        }
    }
    if(gizmo!=out.source.scenes.keys.size()||!a::valid_dungeon_input_state(out.value))
        return fail(error,"Dungeon input complete original scene state invalid");
    dungeon_cells::Sample final;
    if(!dungeon_cells::sample(memory,binding,snapshot,target,final,error,previous))return false;
    if(!dungeon_palette::same_local(out.source.palette,final.palette)||out.source.value!=final.value||
       out.source.scenes.keys!=final.scenes.keys||out.source.monsters!=final.monsters)
        return fail(error,"Dungeon input complete original scene state changed during capture");
    return true;
}
bool digest(const Sample& value,std::string& out,std::string& error){
    // Capture is an intent for this exact current layout. Native motion keeps
    // advancing on Host and is published by the existing authoritative Frame.
    // Complete original records are sampled again at the ordered barrier.
    return quest_bridge::sha256(json{{"layout",value.value.layout_sha256}}.dump(),out,error);
}
bool control_monsters(const Sample& before,const Sample& after,std::string& error){
    if(before.monster_pools.size()!=after.monster_pools.size())return fail(error,"Gizmo control changed the complete Monster scene census");
    for(size_t i=0;i<before.monster_pools.size();++i){auto expected=before.monster_pools[i];expected.global_random=after.monster_pools[i].global_random;
        if(expected!=after.monster_pools[i])return fail(error,"Gizmo control changed a protected Monster/Zone/counter/Region value");}
    return true;
}
const dungeon_scene_identity_runtime::Pool* selected(const Sample& sample,const a::DungeonInputIntent& intent){
    const auto found=std::find_if(sample.source.scenes.pools.begin(),sample.source.scenes.pools.end(),[&](const auto& pool){return pool.scene.owner.key==intent.scene;});
    return found==sample.source.scenes.pools.end()?nullptr:&*found;
}
}
}

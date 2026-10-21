bool dungeon_building_target(host::ProcessMemory& memory,uintptr_t image,uintptr_t root,uintptr_t object,
    const native::BuildingState& value,const std::string& scene,std::string& error){
    if(!action_session::valid_dungeon_building_scene(scene)||!object||object>UINTPTR_MAX-0xa40){
        error="Dungeon Building current object/scene invalid";return false;
    }
    uintptr_t definition=0,ap=0,data=0,shard=0,region=0;uint64_t length=0;uint32_t flags=0;
    if(!memory.copy(object+0x390,&definition,8,error)||!definition||definition>UINTPTR_MAX-0x170||
       !memory.copy(definition,&ap,8,error)||ap!=image+0x172ae50||
       !memory.copy(definition+0x18,&data,8,error)||!data||
       !memory.copy(definition+0x20,&length,8,error)||!length||length>256||
       !memory.copy(definition+0x16c,&flags,4,error)||
       !memory.copy(object+0x170,&shard,8,error)||!memory.copy(object+0x178,&region,8,error)){
        if(error.empty())error="Dungeon Building current definition/owner invalid";return false;
    }
    std::string type(size_t(length),'\0');
    if(!memory.copy(data,type.data(),type.size(),error))return false;
    if(type!=value.type||type.find('\0')!=std::string::npos||type=="Scenery"||(flags&(1u<<13))||
       ((flags&(1u<<16))!=0)!=(type=="DungeonExterior")||
       ((flags&(1u<<17))!=0)!=(type=="DungeonPortal")){
        error="Dungeon Building original type and feature flags differ";return false;
    }
    if(scene=="world"){
        uintptr_t dungeon=0;
        if(type!="DungeonExterior"||shard!=root||value.dungeon_membership||value.dungeon_instance_membership||
           !memory.copy(object+0xa38,&dungeon,8,error)||!dungeon){
            if(error.empty())error="Dungeon Exterior has no original root/template owner";return false;
        }
        return true;
    }
    if(type=="DungeonExterior"||!value.dungeon_membership||value.dungeon_instance_membership){
        error="Dungeon Building target is not an original template member";return false;
    }
    dungeon_scene_identity_runtime::World world;std::vector<dungeon_scene_roster::Scene> scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(world,scenes,error))return false;
    if(uintptr_t(world.map)+0x1c0!=root){error="Dungeon Building scene census current root differs";return false;}
    const auto found=std::find_if(scenes.begin(),scenes.end(),[&](const auto& row){
        return row.owner.key==scene&&row.owner.type==1&&row.owner.shard==shard&&row.owner.region==region;});
    if(found==scenes.end()){error="Dungeon Building full current scene owner differs";return false;}
    return true;
}

bool captured_build_scene(Runtime& runtime,const placement_kiss::Request& request,std::string& scene,std::string& error){
    scene.clear();host::opened::CurrentWorld world;void* root=nullptr;
    if(!read_world(world,root,error))return false;
    host::ProcessMemory memory;uintptr_t manager=0,ap=0,active=0;
    if(!memory.copy(runtime.image+0x13df440,&manager,sizeof manager,error)||!manager||
       !memory.copy(manager,&ap,sizeof ap,error)||ap!=runtime.image+0x1702b00||
       !memory.copy(manager+0x60,&active,sizeof active,error)||!active){
        if(error.empty())error="Building capture lacks current original view Shard";return false;
    }
    if(active==uintptr_t(root)){
        if(request.type=="DungeonPortal"){error="DungeonPortal capture requires its actual template scene";return false;}
        scene="world";return true;
    }
    if(request.type=="DungeonExterior"){error="DungeonExterior capture requires the root world";return false;}
    dungeon_scene_identity_runtime::World current_world;std::vector<dungeon_scene_roster::Scene> scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(current_world,scenes,error))return false;
    if(current_world.world!=world.world||current_world.map!=world.map||current_world.uuid!=world.uuid||
       current_world.world_epoch!=world.world_epoch||current_world.generation!=world.generation){
        error="Building capture current Dungeon world changed";return false;
    }
    for(const auto& current:scenes)if(current.owner.shard==active){
        if(current.owner.type!=1||!scene.empty()){
            error="Building capture is not one exact current template scene";return false;
        }
        scene=current.owner.key;
    }
    uintptr_t final_active=0;
    if(!memory.copy(manager+0x60,&final_active,sizeof final_active,error))return false;
    if(scene.empty()||final_active!=active){error="Building capture template view changed or lacks a current owner";return false;}
    return true;
}
bool submit_captured_build(Runtime& runtime,const placement_kiss::Request& request,const std::string& id,
                           const action::BuildingState& value,std::string& error){
    std::string scene;if(!captured_build_scene(runtime,request,scene,error))return false;
    if(scene=="world"&&request.type!="DungeonExterior")return typed_action_runtime::submit_place(id,value,error);
    return typed_action_runtime::submit_dungeon_building(id,action::Action::PlaceBuilding,"action:"+id,scene,value,{},error);
}

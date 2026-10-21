bool read_build_target(const action::Request& request,host::opened::CurrentWorld& world,void*& shard,std::string& error){
    if(!read_world(world,shard,error))return false;
    if(!request.dungeon_building)return request.dungeon_scene.empty()||[] (std::string& e){e="Ordinary Building carries a Dungeon scene";return false;}(error);
    if(!action::valid_dungeon_building_scene(request.dungeon_scene)||
       (request.building.type=="DungeonExterior")!=(request.dungeon_scene=="world")){
        error="Dungeon Building definition and original scene family differ";return false;
    }
    if(request.dungeon_scene=="world")return true;
    dungeon_scene_identity_runtime::World observed;std::vector<dungeon_scene_roster::Scene> scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(observed,scenes,error))return false;
    if(observed.game!=world.game||observed.mode!=world.mode||observed.state!=world.state||
       observed.world!=world.world||observed.map!=world.map||observed.generation!=world.generation||
       observed.world_epoch!=world.world_epoch||observed.uuid!=world.uuid||observed.sha256!=world.sha256){
        error="Dungeon Building target roster changed complete World owner";return false;
    }
    const auto found=std::find_if(scenes.begin(),scenes.end(),[&](const auto& row){return row.owner.key==request.dungeon_scene;});
    if(found==scenes.end()||found->owner.type!=1||!found->owner.dungeon||!found->owner.shard||
       std::count_if(scenes.begin(),scenes.end(),[&](const auto& row){return row.owner.exterior_key==found->owner.exterior_key;})!=1){
        error="Dungeon Building requires its exact editable template without live Instances";return false;
    }
    host::ProcessMemory memory;int32_t instances=-1;
    if(!memory.copy(found->owner.dungeon+0x4c8,&instances,sizeof instances,error))return false;
    if(instances){error="Dungeon Building original Instance count is not zero";return false;}
    shard=reinterpret_cast<void*>(found->owner.shard);return true;
}
struct DungeonBuildView {
    Runtime& runtime;
    scenery_stroke_calls::Context local;
    host::opened::CurrentWorld world;
    void* target=nullptr;
    bool active=false,finished=false;
    explicit DungeonBuildView(Runtime& owner):runtime(owner){}
    bool begin(const action::Request& request,std::string& error){
        if(!request.dungeon_building)return true;
        if(!read_build_target(request,world,target,error))return false;
        host::ProcessMemory memory;uintptr_t ap=0;
        if(!memory.copy(runtime.image+0x13df440,&local.manager,8,error)||!local.manager||
           !memory.copy(local.manager,&ap,8,error)||ap!=runtime.image+0x1702b00||
           !memory.copy(runtime.image+0x13df9d0,&local.cursor,8,error)||!local.cursor||
           !memory.copy(local.cursor,&ap,8,error)||ap!=runtime.image+0x1786dd0){
            if(error.empty())error="Dungeon Building original Shard/Cursor context owner differs";return false;
        }
        std::array<uint32_t,3> position{};const std::array<float,3> values{request.building.x,request.building.y,request.building.z};
        std::memcpy(position.data(),values.data(),sizeof position);
        const auto code=scenery_stroke_calls::enter(local,uintptr_t(target),position);active=local.installed;
        if(code){error="Dungeon Building original context entry SEH="+std::to_string(code);return false;}
        uintptr_t selected=0;std::array<uint32_t,3> actual{};
        if(!memory.copy(local.manager+0x60,&selected,8,error)||selected!=uintptr_t(target)||
           !memory.copy(local.cursor+0x1e4,actual.data(),sizeof actual,error)||actual!=position){
            if(error.empty())error="Dungeon Building context entry readback differs";return false;
        }
        return true;
    }
    bool restore(std::string& error){
        if(!active){finished=true;return true;}
        if(finished){error="Dungeon Building context restore was already attempted";return false;}
        finished=true;host::ProcessMemory memory;uintptr_t manager=0,cursor=0,selected=0;
        if(!memory.copy(runtime.image+0x13df440,&manager,8,error)||manager!=local.manager||
           !memory.copy(runtime.image+0x13df9d0,&cursor,8,error)||cursor!=local.cursor||
           !memory.copy(local.manager+0x60,&selected,8,error)||selected!=uintptr_t(target)){
            if(error.empty())error="Dungeon Building context owner changed before restoration";return false;
        }
        const auto code=scenery_stroke_calls::restore(local);
        if(code){error="Dungeon Building original context restoration SEH="+std::to_string(code);return false;}
        std::array<uint32_t,3> position{};host::opened::CurrentWorld after;
        if(!memory.copy(local.manager+0x60,&selected,8,error)||selected!=local.root||
           !memory.copy(local.cursor+0x1e4,position.data(),sizeof position,error)||position!=local.position||
           !native_open_runtime::read_current_world(after,error)||!host::same_world(world,after)){
            if(error.empty())error="Dungeon Building restored local view/current World differs";return false;
        }
        return true;
    }
    ~DungeonBuildView(){
        if(active&&!finished){std::string error;if(!restore(error)){
            runtime.cause=error;LOG_ERROR("[UIBuild] Dungeon context restoration failed cause={}",error);
        }}
    }
};

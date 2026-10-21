namespace scene_buildings {
struct State {
    std::map<std::string,Bound> before,after;
    dungeon_scene_identity_runtime::World world;
    std::string snapshot;
    uint64_t generation=0,first_entry=0,next_entry=0;
    bool prepared=false;
};
namespace {
bool scene_current(const dungeon_scene_identity_runtime::World& expected,std::string& error){
    host::opened::CurrentWorld world;
    if(unknown_locked){error="Scene Building registry is locked after an unresolved native effect";return false;}
    if(!current(world,error))return false;
    if(world.uuid!=expected.uuid||world.sha256!=expected.sha256||world.world_epoch!=expected.world_epoch||
       world.generation!=expected.generation||world.world!=expected.world||world.map!=expected.map||
       world.game!=expected.game||world.mode!=expected.mode||world.state!=expected.state){
        error="Scene Building registry current complete world differs";return false;}
    return true;
}
bool row_read(const dungeon_scene_identity_runtime::World& world,const Row& row,native::BuildingState& value,std::string& error){
    if(!(row.scenery?scenery_scene_key::valid(row.key):dungeon_scene_key::building_key(row.key))||!(row.scenery?scenery_scene_key::scene(row.scene):dungeon_gizmo_identity::scene(row.scene))||!row.object||!row.uid||
       !row.shard||!row.region||!row.manager){error="Scene Building current canonical member invalid";return false;}
    if(row.scenery){scene_scenery::Row actual;dungeon_scene_identity_runtime::World observed;
        if(!scene_scenery::owner(row.object,row.uid,actual,observed,error)||!scene_clone::same_world(world,observed))return false;
        if(actual.scene!=row.scene||actual.manager!=row.manager||actual.shard!=row.shard||actual.region!=row.region){error="Scene Scenery current canonical owner differs";return false;}
        return scenery_kiss::read_action_prop(row.uid,value,error)&&scene_current(world,error);}
    uintptr_t image=0;uint32_t thread=0;std::string hash;
    if(!native::read_resolver_receipt(image,thread,hash,error)||thread!=GetCurrentThreadId()||
       hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){
        if(error.empty())error="Scene Building exact Mainthread resolver differs";return false;}
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,image,hash,api,error))return false;
    host::Reader reader(memory,origin,image,thread,api);prop_lease::Prop prop;
    if(!reader.prop(reinterpret_cast<void*>(row.object),row.uid,prop,error)||prop.class_addresspoint!=image+0x16d93c0||
       !scenery_kiss::read_action_prop(row.uid,value,error)||value.prop.uid!=row.uid||
       (!value.dungeon_membership&&!value.dungeon_instance_membership)){
        if(error.empty())error="Scene Building original class/full UID/typed current owner differs";return false;}
    uintptr_t shard=0,region=0,manager=0;
    if(!memory.copy(row.object+0x170,&shard,sizeof shard,error)||!memory.copy(row.object+0x178,&region,sizeof region,error)||
       !memory.copy(region+0x950,&manager,sizeof manager,error))return false;
    if(shard!=row.shard||region!=row.region||manager!=row.manager){error="Scene Building original reciprocal owner changed";return false;}
    return scene_current(world,error);
}
bool exact_bound(const Bound& bound,const State& phase,const Row& row,bool retired){
    return bound.generation==phase.generation&&bound.snapshot==phase.snapshot&&bound.key==row.key&&
        bound.world==prop_lease::World{phase.world.world_epoch,phase.world.generation,phase.world.world,phase.world.map}&&
        bound.uid==row.uid&&bound.object==row.object&&bound.revoked==retired&&!bound.unknown&&
        (!row.scenery||(bound.scene_owner==row.scene&&bound.scene_manager==row.manager&&bound.scene_shard==row.shard&&bound.scene_region==row.region))&&
        bound.class_addresspoint==uintptr_t(GetModuleHandleW(nullptr))+(row.scenery?0x16c1390:0x16d93c0);
}
}
bool resolve(const dungeon_scene_identity_runtime::World& world,uintptr_t object,uint64_t uid,std::string& key,std::string& error){
    key.clear();if(!object||!uid||!scene_current(world,error))return false;
    return resolve_current_building_key(net::current_snapshot_id(),object,uid,key,error);
}
bool preview(const dungeon_scene_identity_runtime::World& world,const std::vector<Row>& born,
    const std::vector<Row>& retired,const std::vector<Restore>& restored,Preview& out,std::string& error,const scene_clone::Raw* held){
    out={};if(!scene_current(world,error))return false;
    auto phase=std::make_shared<State>();phase->before=registry;phase->after=registry;phase->world=world;
    phase->snapshot=net::current_snapshot_id();phase->generation=registry_generation;phase->first_entry=next_entry;phase->next_entry=next_entry;
    const auto id=[&](const std::string& key){return std::to_string(phase->generation)+":"+binding_id(phase->snapshot,key);};
    for(const auto& row:retired){
        auto found=phase->after.find(id(row.key));uintptr_t actual=0;
        if(found==phase->after.end()||!exact_bound(found->second,*phase,row,false)||
           !scene_clone::find_uid(row.uid,actual,error)||(scene_clone::held_member(held,row.scene,row.object,row.uid)?actual!=row.object:actual!=0)){
            if(error.empty())error="Scene Building retirement lacks exact original full UID absence";return false;}
        found->second.revoked=true;
    }
    const auto admit=[&](const Row& row,const Row* old)->bool{
        native::BuildingState value;if(!row_read(world,row,value,error))return false;
        const auto key=id(row.key);auto found=phase->after.find(key);
        if(old){
            if(row.key!=old->key||row.scene!=old->scene||row.scenery!=old->scenery||found==phase->after.end()||!exact_bound(found->second,*phase,*old,true)){
                error="Scene Building restoration differs from exact retired original member";return false;}
        }else if(found!=phase->after.end()||!row.key.starts_with("action:")){
            error="Scene Building birth key is reused or lacks original operation provenance";return false;}
        for(const auto& [other,bound]:phase->after)if(other!=key&&bound.generation==phase->generation&&
            bound.snapshot==phase->snapshot&&(bound.uid==row.uid||(!bound.revoked&&bound.object==row.object))){
            error="Scene Building native full UID/pointer aliases another confirmed member";return false;}
        if(phase->next_entry==UINT64_MAX){error="Scene Building registry entry exhausted";return false;}
        Bound fresh;fresh.snapshot=phase->snapshot;fresh.key=row.key;fresh.sha=world.sha256;fresh.uid=row.uid;
        fresh.entry=++phase->next_entry;fresh.generation=phase->generation;
        fresh.world={world.world_epoch,world.generation,world.world,world.map};
        fresh.metadata=building_metadata(value);fresh.object=row.object;fresh.scene_owner=row.scenery?row.scene:std::string{};
        if(row.scenery){fresh.scene_manager=row.manager;fresh.scene_shard=row.shard;fresh.scene_region=row.region;}fresh.class_addresspoint=uintptr_t(GetModuleHandleW(nullptr))+(row.scenery?0x16c1390:0x16d93c0);
        phase->after[key]=std::move(fresh);return true;
    };
    for(const auto& row:born)if(!admit(row,nullptr))return false;
    for(const auto& row:restored)if(!admit(row.after,&row.before))return false;
    phase->prepared=true;out.state=std::move(phase);error.clear();return true;
}
bool check(const Preview& preview,std::string& error){
    if(!preview.state||!preview.state->prepared||!scene_current(preview.state->world,error)||
       preview.state->snapshot!=net::current_snapshot_id()||preview.state->generation!=registry_generation||
       preview.state->first_entry!=next_entry||preview.state->before!=registry){
        if(error.empty())error="Scene Building registry preview changed or consumed";return false;}
    return true;
}
bool publish(Preview& preview,std::string& error){
    if(!check(preview,error))return false;
    registry=std::move(preview.state->after);next_entry=preview.state->next_entry;preview.state->prepared=false;error.clear();return true;
}
}

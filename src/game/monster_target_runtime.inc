namespace {
bool generated_member(const a::Binding& binding,const std::string& snapshot,const std::string& key,
                      interventions::Actor& out,std::string& error){
    out={};a::Context context;interventions::Census census;
    if(!interventions::zone_monster_key(key)||!a::selected_session().current(context,error)||context.snapshot_uuid!=snapshot||
       !interventions::census(context,binding,census,error)){
        if(error.empty())error="Monster Move generated member requires the current complete canonical actor census";return false;}
    unsigned found=0;for(const auto& row:census.actors)if(row.target.family==interventions::Family::Monster&&row.target.key==key){out=row;++found;}
    return found==1&&out.object&&out.uid||fail(error,"Monster Move generated canonical member is absent or ambiguous");
}
bool direct_registry(Memory& memory,const a::Binding& binding,const std::string& snapshot,Pin& owner,
                     std::vector<std::pair<std::string,identity::Entry>>& rows,std::string& error){
    owner={};if(!state.ready||state.thread!=GetCurrentThreadId())return fail(error,"Monster target requires installed bound Mainthread");
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,owner.api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,owner.api);
    if(!reader.current(owner.world,true,error)||!action_runtime::bound(owner.world,binding,snapshot,error))return false;
    owner.binding=binding;owner.snapshot=snapshot;
    return identity::registry.enumerate(owner.world.origin.uuid,owner.world.origin.world_epoch,
        owner.world.origin.generation,owner.world.root,rows,error);
}
bool direct_target_pin(Memory& memory,const a::Binding& binding,const std::string& snapshot,const std::string& argument,
                       int32_t expected_region,Pin& owner,std::string& error,bool retired=false){
    std::vector<std::pair<std::string,identity::Entry>> rows;if(!direct_registry(memory,binding,snapshot,owner,rows,error))return false;
    const identity::Scope world{owner.world.origin.uuid,owner.world.origin.world_epoch,owner.world.origin.generation,owner.world.root};
    identity::Entry entry;if(!monster_target_scope::target(world,rows,argument,retired,entry,error))return false;
    int32_t saved=-1;if(!field(memory,entry.scope.region,0x834,saved,error))return false;
    if(saved<0||(expected_region>=0&&saved!=expected_region))return fail(error,"Monster target current saved Region differs from request");
    if(identity::root_owned(entry.scope)){
        if(!pin(memory,binding,snapshot,saved,owner,error))return false;
        uintptr_t manager=0;if(!field(memory,owner.region.region,0x988,manager,error))return false;
        return entry.scope.region==owner.region.region&&entry.scope.manager==manager||fail(error,"Monster target Root Region/manager differs from confirmed owner");
    }
    dungeon_scene_identity_runtime::Census scenes;
    if(!dungeon_scene_identity_runtime::read_current(scenes,error))return false;
    if(scenes.world.uuid!=owner.world.origin.uuid||scenes.world.sha256!=owner.world.origin.sha256||
       scenes.world.world_epoch!=owner.world.origin.world_epoch||scenes.world.generation!=owner.world.origin.generation||
       scenes.world.map!=owner.world.origin.map||scenes.world.state!=owner.world.origin.state)
        return fail(error,"Monster target Dungeon roster belongs to another current NativeOpen world");
    const auto found=std::find_if(scenes.pools.begin(),scenes.pools.end(),[&](const auto& pool){return pool.scene.owner.key==entry.scope.scene;});
    if(found==scenes.pools.end()||found->scene.owner.region!=entry.scope.region||found->managers[2]!=entry.scope.manager||
       found->scene.owner.shard==owner.world.root)return fail(error,"Monster target exact Dungeon scene/Region/manager is absent or foreign");
    owner.scene=entry.scope.scene;owner.region.image=state.image;owner.region.map=uintptr_t(owner.world.origin.map);
    owner.region.root=found->scene.owner.shard;owner.region.region=entry.scope.region;owner.region.saved_region=saved;
    owner.region.origin={owner.world.origin.uuid,owner.world.origin.sha256,owner.world.origin.world_epoch,owner.world.origin.generation};
    error.clear();return true;
}
bool direct_receiver(Memory& memory,const a::Binding& binding,const std::string& snapshot,uintptr_t object,uint64_t uid,
                     std::string& canonical,std::string& error){
    canonical.clear();Pin owner;std::vector<std::pair<std::string,identity::Entry>> rows;
    if(!direct_registry(memory,binding,snapshot,owner,rows,error))return false;
    const identity::Scope world{owner.world.origin.uuid,owner.world.origin.world_epoch,owner.world.origin.generation,owner.world.root};
    std::string argument;if(!monster_target_scope::receiver(world,rows,object,uid,argument,error))return false;
    canonical=monster_target_key::canonical(argument);return true;
}
bool direct_sample(Memory& memory,const Pin& owner,d::Sample& out,std::string& error){
    if(owner.scene=="world")return d::sample(memory,owner.region,out,error);
    d::ReadMemory bounded{memory};uintptr_t manager=0,release=0;
    const auto owners=[&](auto& source,const auto& current,uintptr_t& buildings,uintptr_t& zones,std::string& cause){
        uintptr_t building_root=0,building_region=0,zone_region=0,region_shard=0;
        if(!q::at(source,current.region,0x950,buildings,cause)||!q::at(source,current.region,0x968,zones,cause)||
           !q::typed(source,current.image,buildings,0x17183c0,cause)||!q::typed(source,current.image,zones,0x16fe170,cause)||
           !q::at(source,buildings,0x450,building_root,cause)||!q::at(source,buildings,0x458,building_region,cause)||
           !q::at(source,zones,0xf0,zone_region,cause)||!q::at(source,current.region,0x70,region_shard,cause))return false;
        return building_root==current.root&&region_shard==current.root&&building_region==current.region&&zone_region==current.region||
            fail(cause,"Monster target Dungeon manager reciprocal Shard/Region differs");
    };
    return monster_zone_read::read_owned(bounded,owner.region,owners,out.state,error)&&native::read_simulation_state(out.money,error)&&
        undo_probe::read_undo(memory.process,state.image,out.undo,error)&&
        q::at(bounded,state.image,0x13df4f0,manager,error)&&q::typed(bounded,state.image,manager,0x1714ed0,error)&&
        q::at(bounded,manager,0x38,release,error)&&q::typed(bounded,state.image,release,0x16c12b0,error)&&
        q::at(bounded,release,0x120,out.release_edits,error);
}
bool generated_pin(Memory& memory,const a::Binding& binding,const std::string& snapshot,const std::string& key,
                   int32_t expected_region,Pin& owner,std::string& error){
    std::vector<std::pair<std::string,identity::Entry>> direct;
    if(!direct_registry(memory,binding,snapshot,owner,direct,error))return false;
    a::Context context;interventions::Census census;
    if(!a::selected_session().current(context,error)||context.snapshot_uuid!=snapshot||!interventions::census(context,binding,census,error))return false;
    const auto found=std::find_if(census.actors.begin(),census.actors.end(),[&](const auto& row){return row.target.family==interventions::Family::Monster&&row.target.key==key;});
    if(found==census.actors.end()||!interventions::zone_monster_key(key))return fail(error,"Monster Move generated target absent from current complete actor census");
    uintptr_t region=0,zone=0;int32_t saved=-1;
    if(!field(memory,found->object,0x178,region,error)||!field(memory,found->object,0x8d0,zone,error)||
       !field(memory,region,0x834,saved,error))return false;
    const auto pool=std::find_if(census.bosses.pools.begin(),census.bosses.pools.end(),[&](const auto& row){return row.scope.region==region;});
    const auto separator=key.rfind(":monster:");const auto zone_key=key.substr(0,separator);
    const auto bound_zone=std::find_if(census.zones.begin(),census.zones.end(),[&](const auto& row){return row.object==zone&&row.key==zone_key;});
    if(pool==census.bosses.pools.end()||bound_zone==census.zones.end()||saved<0||(expected_region>=0&&saved!=expected_region)||
       census.bosses.world.uuid!=owner.world.origin.uuid||census.bosses.world.world_epoch!=owner.world.origin.world_epoch||
       census.bosses.world.generation!=owner.world.origin.generation||census.bosses.world.map!=owner.world.origin.map)
        return fail(error,"Monster Move generated member scene/Region/reciprocal Zone differs");
    if(pool->scope.scene=="world")return pin(memory,binding,snapshot,saved,owner,error);
    owner.scene=pool->scope.scene;owner.region.image=state.image;owner.region.map=uintptr_t(owner.world.origin.map);
    owner.region.root=pool->scope.shard;owner.region.region=region;owner.region.saved_region=saved;
    owner.region.origin={owner.world.origin.uuid,owner.world.origin.sha256,owner.world.origin.world_epoch,owner.world.origin.generation};
    return true;
}
bool move_destination(Memory& memory,const Pin& owner,uintptr_t object,const std::array<float,3>& point,std::string& error){
    const auto at=[&](uintptr_t address,size_t offset,auto& value,std::string& cause){return field(memory,address,offset,value,cause);};
    const auto locate=[&](uintptr_t shard,const std::array<float,3>& position,uintptr_t& region,std::string& cause){
        const auto seh=shard_region(state.image,shard,position,region);
        return !seh||fail(cause,"Monster Move original GetRegionAt SEH="+std::to_string(seh));
    };
    return monster_move_destination::check(object,owner.region.root,owner.region.region,point,at,locate,error);
}
bool move_owner_pin(Memory& memory,const a::Binding& binding,const std::string& snapshot,const std::string& key,
                    int32_t region,Pin& owner,std::string& error){
    return interventions::zone_monster_key(key)?generated_pin(memory,binding,snapshot,key,region,owner,error):
        direct_target_pin(memory,binding,snapshot,key,region,owner,error);
}
bool move_receiver(Memory& memory,const a::Binding& binding,const std::string& snapshot,uintptr_t object,uint64_t uid,
                   std::string& key,std::string& error){
    key.clear();Pin owner;std::vector<std::pair<std::string,identity::Entry>> direct;
    if(!direct_registry(memory,binding,snapshot,owner,direct,error))return false;
    a::Context context;interventions::Census census;
    if(!a::selected_session().current(context,error)||context.snapshot_uuid!=snapshot||!interventions::census(context,binding,census,error))return false;
    unsigned found=0;
    for(const auto& row:census.actors)if(row.object==object||row.uid==uid){
        if(row.object!=object||row.uid!=uid||row.target.family!=interventions::Family::Monster||!monster_move_key::valid(row.target.key))
            return fail(error,"Monster Move receiver/full UID aliases a different canonical actor");
        key=row.target.key;++found;
    }
    return found==1||fail(error,"Monster Move receiver lacks a unique complete current canonical binding");
}
}

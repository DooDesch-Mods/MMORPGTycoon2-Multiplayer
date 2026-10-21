namespace {
bool root_scenery_rows(const World& world,uintptr_t image,uint32_t thread,const host::LookupApi& api,
    bool snapshot,std::vector<ScenerySaved>& out,std::string& error){
    host::ProcessMemory memory;
    auto at=[&](uintptr_t pointer,size_t offset,auto& value,std::string& cause){
        if(!pointer||pointer>UINTPTR_MAX-offset)return fail(cause,"Root Scenery census field overflow");
        return memory.copy(pointer+offset,&value,sizeof value,cause);
    };
    region_saved_identity::Index before,after;const auto map=uintptr_t(world.map),root=map+0x1c0;
    if(!region_saved_identity::capture(image,map,root,before,at,error))return false;
    std::vector<ScenerySaved> rows;std::set<uintptr_t> objects;std::set<uint64_t> uids;
    for(const auto& [id,region]:before.saved){
        Pool pool;pool.scene.owner.key="world";pool.scene.owner.shard=root;pool.scene.owner.region=region;
        if(!at(region,0x960,pool.managers[0],error))return false;
        SceneSceneryHost scenery_host(world,pool,api,thread,snapshot);
        mt2mp::build_actions::private_native::place::SceneryWitness witness;
        if(!mt2mp::build_actions::private_native::place::read_scenery(memory,scenery_host,image,witness,error))return false;
        for(const auto& member:witness.members)if(!objects.insert(member.object).second||!uids.insert(member.uid).second)
            return fail(error,"Root Scenery full native membership overlaps another saved Region");
        if(!root_scenery_saved::append(world.sha256,id,witness,rows,error))return false;
    }
    if(!region_saved_identity::capture(image,map,root,after,at,error))return false;
    if(before!=after)return fail(error,"Root Scenery saved Region owners/order changed during snapshot census");
    out.insert(out.end(),rows.begin(),rows.end());return true;
}
}
bool root_scenery_owner(uintptr_t object,uint64_t uid,ScenerySaved& out,World& world,bool& matched,std::string& error){
    out={};world={};matched=false;uintptr_t image=0;uint32_t thread=0;std::string digest;
    if(!native::read_resolver_receipt(image,thread,digest,error))return false;
    if(digest!=executable||image!=uintptr_t(GetModuleHandleW(nullptr))||thread!=GetCurrentThreadId())return fail(error,"Root Scenery current resolver/Mainthread differs");
    building_mixed_identity::opened_file::CurrentWorld opened;
    if(!host::RootSceneryReadBatch::read_current(opened,error))return false;
    host::ProcessMemory memory;uintptr_t shard=0;
    if(!object||!uid)return fail(error,"Root Scenery requires current native pointer/full UID");
    if(!memory.copy(object+0x170,&shard,sizeof shard,error))return false;
    if(shard!=uintptr_t(opened.map)+0x1c0)return true;
    matched=true;host::LookupApi api;if(!host::resolve_lookup(memory,image,digest,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory,origin,image,thread,api);host::WorldState before,after;prop_lease::Prop prop;
    uintptr_t saved_region=0;int32_t saved_region_id=-1;
    if(!memory.copy(object+0x178,&saved_region,sizeof saved_region,error)||!memory.copy(saved_region+0x834,&saved_region_id,sizeof saved_region_id,error))return false;
    if(!reader.current(before,true,error)||!host::same_world(before.origin,opened)||
       !reader.saved_root_scenery(reinterpret_cast<void*>(object),uid,saved_region_id,saved_region,prop,error))return false;
    if(prop.class_addresspoint!=image+0x16c1390||!prop.typed_prop||!prop.complete_ownership||prop.uid!=uid||
       prop.registry_object!=reinterpret_cast<void*>(object))return fail(error,"Root Scenery actual class/registry/member ownership differs");
    uintptr_t region=0,manager=0;
    if(!memory.copy(object+0x178,&region,sizeof region,error)||!memory.copy(region+0x960,&manager,sizeof manager,error)||
       !reader.current(after,true,error))return false;
    if(!host::same_world(before.origin,after.origin)||before.root!=after.root||shard!=after.root)return fail(error,"Root Scenery current complete world changed");
    world=copy_world(opened);out={"","world",object,manager,shard,region,uid};error.clear();return true;
}

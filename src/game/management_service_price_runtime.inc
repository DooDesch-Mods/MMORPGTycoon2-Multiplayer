namespace price_graph=management_service_graph;
uint32_t price_graph_lock_call(uintptr_t image,uintptr_t manager,bool unlock)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(image+(unlock?0xa0b140:0xa0b120))(reinterpret_cast<void*>(manager));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct PriceGraphLock {
    Runtime& runtime;
    bool call(uintptr_t manager,bool unlock,std::string& error){const auto seh=price_graph_lock_call(runtime.image,manager,unlock);
        if(seh){error=std::string("Service price native ")+(unlock?"unlock":"lock")+" SEH="+std::to_string(seh);return false;}return true;}
    bool lock(uintptr_t manager,std::string& error){return call(manager,false,error);}
    bool unlock(uintptr_t manager,std::string& error){return call(manager,true,error);}
};
bool price_gizmo_keys(const a::Context& context,const a::Binding& binding,const building_mixed_identity::opened_file::CurrentWorld& world,price_graph::OwnerKeys& out,std::string& error){
    out.clear();dungeon_scene_identity_runtime::Census census;if(!dungeon_scene_identity_runtime::read_current(census,error))return false;
    if(census.world.uuid!=context.snapshot_uuid||census.world.world_epoch!=binding.native_epoch||census.world.generation!=binding.native_generation||census.world.game!=world.game||census.world.mode!=world.mode||census.world.state!=world.state||census.world.world!=world.world||census.world.map!=world.map)return fail(error,"Service price Gizmo census world differs");
    for(const auto& pool:census.pools)for(const auto& member:pool.local.members){std::string key;
        if(!dungeon_scene_identity_runtime::resolve(member.object,member.uid,key,error))return false;
        if(key.empty()||!out.emplace(member.object,key).second)return fail(error,"Service price Gizmo canonical owner duplicated");}
    return true;
}
bool read_price_graph(Runtime& r,const a::Context& context,const a::Binding& binding,Local& local,a::ManagementServices& services,std::string& error){
    try{
    using price_graph::OwnerKeys;building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,error)||world.uuid!=context.snapshot_uuid||world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation)return fail(error,error.empty()?"Service price NativeOpen world differs":error);
    const uintptr_t map=uintptr_t(world.map);if(!map||map>UINTPTR_MAX-0x2b8)return fail(error,"Service price Map extent invalid");const uintptr_t root=map+0x1c0;
    uintptr_t shard=0,manager=0,random=0;int32_t shard_type=-1,initial_extra=0;std::array<uint64_t,4> initial_random{};
    if(!ap(r,map,0x1742860,error)||!ap(r,root,0x174ad20,error)||!at(r,root,0x48,shard_type,error)||shard_type!=0||
       !at(r,local.service_building,0x170,shard,error)||shard!=root||!at(r,map,0x2b0,manager,error)||!manager||
       !at(r,r.image,0x164b250,random,error)||!random||!at(r,random,0,initial_random,error)||!at(r,random,0x20,initial_extra,error))return fail(error,error.empty()?"Service price root owner/manager/RNG invalid":error);
    auto read=[&](uintptr_t p,size_t off,auto& value,std::string& cause){return at(r,p,off,value,cause);};
    region_saved_identity::Index regions;if(!region_saved_identity::capture(r.image,map,root,regions,read,error))return false;
    OwnerKeys region_keys;for(const auto& [id,pointer]:regions.saved)if(!region_keys.emplace(pointer,"world/region:"+std::to_string(id)).second)return fail(error,"Service price Region pointer duplicated");
    std::vector<remove_diagnostics::LocalTarget> targets;if(!remove_diagnostics::identity_targets(targets,error))return false;
    OwnerKeys building_keys;std::map<std::string,price_graph::BuildingSource> buildings;
    for(const auto& target:targets){uintptr_t klass=0,owner=0,region=0;if(!at(r,target.object,0,klass,error))return false;if(klass!=r.image+0x16d93c0)continue;
        if(!at(r,target.object,0x170,owner,error)||!at(r,target.object,0x178,region,error))return false;
        if(owner!=root)continue;if(!region_keys.contains(region))return fail(error,"Service price source Building root/Region owner differs");
        if(!building_keys.emplace(target.object,target.record_key).second||!buildings.emplace(target.record_key,price_graph::BuildingSource{target.object,owner,region,target.uid,target.record_key}).second)return fail(error,"Service price source Building identity duplicated");}
    OwnerKeys paths,gizmos;if(!ui_path_runtime::current_root_path_keys(context.snapshot_uuid,paths,error)||!price_gizmo_keys(context,binding,world,gizmos,error))return false;
    PriceGraphLock calls{r};std::vector<std::string> source_keys;
    if(!price_graph::with_read_lock(calls,manager,r.service_price_lock_unknown,[&](std::string& why){return price_graph::discover_sources(read,r.image,manager,building_keys,source_keys,why);},error))return false;
    source_keys.erase(std::remove(source_keys.begin(),source_keys.end(),services.building_key),source_keys.end());source_keys.insert(source_keys.begin(),services.building_key);
    std::vector<price_graph::BuildingSource> sources;for(const auto& key:source_keys){const auto found=buildings.find(key);if(found==buildings.end())return fail(error,"Service price source Building disappeared");sources.push_back(found->second);}
    const auto get_text=[&](uintptr_t p,std::string& value,std::string& why){return text(r,p,value,4096,why);};
    const auto destination=[&](uintptr_t p,std::string& key,std::string& why){uint64_t uid=0;if(!at(r,p,0x18,uid,why))return false;return remove_diagnostics::resolve_current_building_key(context.snapshot_uuid,p,uid,key,why);};
    const auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& why){const auto seh=uid_lookup(r.image,uid,pointer);return seh?fail(why,"Service price source FindUID SEH="+std::to_string(seh)):true;};
    std::map<std::string,price_graph::SourceBuilding> sampled;std::vector<price_graph::ServiceSourcePin> pins;
    if(!price_graph::source_services(read,get_text,destination,find,r.image,root,sources,sampled,pins,error))return false;
    price_graph::Graph graph;if(!price_graph::with_read_lock(calls,manager,r.service_price_lock_unknown,[&](std::string& why){return price_graph::read_graph(read,r.image,manager,"world",services.building_key,region_keys,building_keys,paths,gizmos,sampled,graph,why);},error))return false;
    if(graph.source_buildings.size()!=source_keys.size())return fail(error,"Service price source census changed between locked passes");
    for(size_t i=0;i<source_keys.size();++i)if(graph.source_buildings[i].key!=source_keys[i])return fail(error,"Service price source order changed between locked passes");
    std::map<std::string,price_graph::SourceBuilding> after_sample;std::vector<price_graph::ServiceSourcePin> after_pins;
    if(!price_graph::source_services(read,get_text,destination,find,r.image,root,sources,after_sample,after_pins,error))return false;
    if(after_pins!=pins||!region_saved_identity::verify(r.image,map,root,regions,read,error))return fail(error,error.empty()?"Service price source or Region owner changed":error);
    OwnerKeys paths_after,gizmos_after;if(!ui_path_runtime::current_root_path_keys(context.snapshot_uuid,paths_after,error)||!price_gizmo_keys(context,binding,world,gizmos_after,error))return false;
    if(paths_after!=paths||gizmos_after!=gizmos)return fail(error,"Service price Path/Gizmo census changed");
    uintptr_t manager_after=0,shard_after=0;building_mixed_identity::opened_file::CurrentWorld after_world;
    if(!at(r,map,0x2b0,manager_after,error)||!at(r,local.service_building,0x170,shard_after,error)||!native_open_runtime::read_current_world(after_world,error))return false;
    if(manager_after!=manager||shard_after!=root||after_world.game!=world.game||after_world.mode!=world.mode||after_world.state!=world.state||after_world.world!=world.world||after_world.map!=world.map||after_world.world_epoch!=world.world_epoch||after_world.generation!=world.generation||after_world.uuid!=world.uuid)return fail(error,"Service price current World or manager changed");
    auto hash=[](const std::string& text){return mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));};a::ManagementPriceWitness witness;
    if(!price_graph::price_witness(read,hash,r.image,graph,witness,error))return false;
    if(witness.random!=initial_random||witness.random_extra!=initial_extra)return fail(error,"Service price RNG changed during graph census");
    local.price_manager=manager;local.price_graph=std::move(graph);services.price=std::move(witness);return true;
    }catch(const std::exception& ex){error=std::string("Service price graph read exception: ")+ex.what();return false;}
    catch(...){return fail(error,"Service price graph read nonstandard exception");}
}

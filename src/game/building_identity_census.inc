namespace {
bool building_identity_index(const host::WorldState& world,uintptr_t image,std::map<uint64_t,const Bound*>& indexed,
                            std::set<uint64_t>& omitted,std::string& error){
    indexed.clear();omitted.clear();
    if(unknown_locked){error="remove diagnostics are Unknown; no new binding admission";return false;}
    const prop_lease::World expected{world.origin.world_epoch,world.origin.generation,world.origin.world,world.origin.map};
    if(admitted_world.epoch&&admitted_world!=expected){error="Building owner census admitted World changed";return false;}
    const auto snapshot=net::current_snapshot_id();std::set<std::string> keys;std::set<uintptr_t> objects;
    for(const auto& [id,binding]:registry){
        if(binding.generation!=registry_generation||binding.snapshot!=snapshot||binding.revoked||binding.scene_owner=="retired")continue;
        if(binding.unknown){error="Service census includes Unknown native binding";return false;}
        if(binding.world!=expected){error="Building owner census current registry World differs";return false;}
        if(!binding.uid||!binding.entry||binding.key.empty()||id!=registry_id(snapshot,binding.key)||
           !keys.insert(binding.key).second||!indexed.emplace(binding.uid,&binding).second||
           (binding.object&&!objects.insert(binding.object).second)){
            error="Building owner census complete registry identity is ambiguous or invalid";return false;
        }
        // Only previously admitted original saved Root members can avoid an
        // unused full property read. Unclassified and newly born rows stay fresh.
        if(binding.object&&binding.scene_owner=="world"&&binding.scene_manager&&binding.scene_region&&
           binding.scene_shard==world.root&&binding.scene_record.starts_with(binding.sha+":world:scenery:inline:")&&
           binding.class_addresspoint==image+0x16c1390)omitted.insert(binding.uid);
    }
    return true;
}
bool building_identity_resolve(const Bound& binding,const host::WorldState& world,const host::LookupApi& api,
                               LocalTarget& target,std::string& error){
    void* object=nullptr;uint32_t seh=0;
    if(!lookup(api,binding.uid,object,seh)){error="Service census full UID lookup SEH="+std::to_string(seh);return false;}
    if(!object){error="Service census full UID no longer resolves";return false;}
    return resolve_local_target_impl(world.selection,uintptr_t(object),target,error,&world.origin,true,&binding);
}
}
bool identity_building_targets(std::vector<LocalTarget>& out,std::string& error,std::map<uintptr_t,std::string>* destinations){
    out.clear();if(destinations)destinations->clear();host::RootSceneryReadBatch batch;
    auto& free=native_free_runtime::Runtime::instance();native_free_runtime::Receipt heap;
    if(!free.read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState world;
    if(!reader.current(world,true,error))return false;
    std::map<uint64_t,const Bound*> indexed;std::set<uint64_t> omitted;
    if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;
    json sources;if(!collect_bindings(sources,error,BindingDetails::BuildingTargets,&omitted))return false;
    const auto prior_omitted=omitted;
    if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;
    if(!std::includes(omitted.begin(),omitted.end(),prior_omitted.begin(),prior_omitted.end())){
        error="Building owner census saved exclusion changed during collection";return false;
    }
    json selected={{"world_epoch",sources.at("world_epoch")},{"world_generation",sources.at("world_generation")},{"bindings",json::array()}};
    for(const auto& row:sources.at("bindings")){
        if(row.at("unknown").get<bool>()){error="Service census includes Unknown native binding";return false;}
        if(!row.at("revoked").get<bool>()&&!omitted.contains(row.at("local_uid").get<uint64_t>()))selected["bindings"].push_back(row);
    }
    const auto resolve=[&](const json& row,LocalTarget& target,std::string& cause){
        const auto found=indexed.find(row.at("local_uid").get<uint64_t>());
        if(found==indexed.end()||found->second->snapshot!=row.at("snapshot_id")||found->second->key!=row.at("record_key")){
            cause="Service census supplied binding differs from complete UID index";return false;
        }
        return building_identity_resolve(*found->second,world,api,target,cause);
    };
    std::vector<LocalTarget> sampled,pending;std::map<uintptr_t,std::string> candidates;
    if(!service_building_census::read(selected,resolve,sampled,error))return false;
    for(const auto& [uid,binding]:indexed)if(omitted.contains(uid))candidates.emplace(binding->object,binding->key);
    for(auto& target:sampled){
        if(!candidates.emplace(target.object,target.record_key).second){error="Building owner census current receiver is ambiguous";return false;}
        if(target.normal_building)pending.push_back(std::move(target));
    }
    if(!batch.finish(error))return false;
    out=std::move(pending);if(destinations)*destinations=std::move(candidates);return true;
}
bool resolve_identity_destination(uintptr_t object,LocalTarget& out,std::string& error){
    out={};host::RootSceneryReadBatch batch;
    auto& free=native_free_runtime::Runtime::instance();native_free_runtime::Receipt heap;
    if(!free.read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState world;
    if(!reader.current(world,true,error))return false;
    std::map<uint64_t,const Bound*> indexed;std::set<uint64_t> omitted;
    if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;
    if(!object||object>UINTPTR_MAX-0x20){error="Service referenced destination pointer extent invalid";return false;}
    uint64_t uid=0;
    if(!memory.copy(object+0x18,&uid,sizeof uid,error))return false;
    const auto matched=indexed.find(uid);
    // Saved Building bindings retain canonical full UIDs before a local pointer
    // hint exists. The original lookup and complete target proof remain fresh.
    if(!uid||matched==indexed.end()){error="Service referenced destination lacks complete current canonical identity";return false;}
    LocalTarget pending;
    if(!building_identity_resolve(*matched->second,world,api,pending,error))return false;
    if(pending.object!=object){error="Service referenced destination current full UID receiver changed";return false;}
    if(!batch.finish(error))return false;
    out=std::move(pending);return true;
}

namespace {
bool party_alias_current(const prop_lease::World& expected,const std::string& snapshot,const std::string& key,uint64_t uid,uintptr_t object,uintptr_t stamp,std::string& error){
    if(unknown_locked||snapshot!=net::current_snapshot_id()||admitted_world!=expected||!uid||!object||!stamp){
        error="Party prop alias requires its current admitted World and unlocked snapshot";return false;
    }
    const auto found=registry.find(registry_id(snapshot,key));
    if(found==registry.end()){error="Party prop alias has no current registry entry";return false;}
    const auto& saved=found->second;
    if(saved.revoked||saved.unknown||saved.scene_owner=="retired"||saved.generation!=registry_generation||saved.world!=expected||
       saved.snapshot!=snapshot||saved.key!=key||saved.uid!=uid||!saved.entry||
       (saved.object&&saved.object!=object)||(saved.class_addresspoint&&saved.class_addresspoint!=stamp)){
        error="Party prop alias current canonical lifetime differs";return false;
    }
    return true;
}
}
bool party_identity_current(const std::string& snapshot,const std::string& key,uint64_t uid,uintptr_t object,uintptr_t stamp,std::string& error){
    host::opened::CurrentWorld world;if(!current(world,error))return false;
    return party_alias_current({world.world_epoch,world.generation,world.world,world.map},snapshot,key,uid,object,stamp,error);
}
bool party_identity_bindings(json& out,std::string& error){
    out=json::array();host::RootSceneryReadBatch batch;
    auto& free=native_free_runtime::Runtime::instance();native_free_runtime::Receipt heap;
    if(!free.read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState world;
    if(!reader.current(world,true,error))return false;
    const prop_lease::World expected{world.origin.world_epoch,world.origin.generation,world.origin.world,world.origin.map};
    std::map<uint64_t,const Bound*> indexed;std::set<uint64_t> omitted;
    if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;
    // Party records use canonical links, not the pose of unrelated saved props.
    // Newly admitted and unclassified rows retain the complete metadata read.
    json sources;if(!collect_bindings(sources,error,BindingDetails::Identity,&omitted))return false;
    const auto prior_omitted=omitted;
    if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;
    if(!std::includes(omitted.begin(),omitted.end(),prior_omitted.begin(),prior_omitted.end())){
        error="Party prop alias saved exclusion changed during collection";return false;
    }
    json pending=json::array();std::set<uint64_t> seen;std::set<uintptr_t> pointers;std::set<std::string> keys;
    for(const auto& row:sources.at("bindings")){
        if(row.at("revoked").get<bool>())continue;
        const auto uid=row.at("local_uid").get<uint64_t>();const auto found=indexed.find(uid);
        if(row.at("unknown").get<bool>()||found==indexed.end()||found->second->snapshot!=row.at("snapshot_id")||
           found->second->key!=row.at("record_key")||!seen.insert(uid).second){
            error="Party prop alias differs from complete current UID index";return false;
        }
        const auto& saved=*found->second;void* actual=nullptr;uint32_t seh=0;
        if(!lookup(api,uid,actual,seh)){error="Party prop alias full UID lookup SEH="+std::to_string(seh);return false;}
        const auto object=reinterpret_cast<uintptr_t>(actual);uintptr_t stamp=0;uint64_t observed_uid=0;
        if(!object||object>UINTPTR_MAX-0x20){error="Party prop alias full UID returned no readable object";return false;}
        if(!memory.copy(object,&stamp,sizeof stamp,error)||!memory.copy(object+0x18,&observed_uid,sizeof observed_uid,error))return false;
        if(observed_uid!=uid||!pointers.insert(object).second||!keys.insert(saved.key).second){
            error="Party prop alias full UID receiver is changed or ambiguous";return false;
        }
        if(!party_alias_current(expected,saved.snapshot,saved.key,uid,object,stamp,error))return false;
        pending.push_back({{"snapshot_id",saved.snapshot},{"record_key",saved.key},{"local_uid",uid},{"object",object},{"stamp",stamp}});
    }
    if(seen.size()!=indexed.size()){error="Party prop alias collection omitted an active registry entry";return false;}
    for(const auto& row:pending)if(!party_alias_current(expected,row.at("snapshot_id").get<std::string>(),row.at("record_key").get<std::string>(),
        row.at("local_uid").get<uint64_t>(),row.at("object").get<uintptr_t>(),row.at("stamp").get<uintptr_t>(),error))return false;
    host::WorldState after;
    if(!reader.current(after,true,error)||!host::same_world(world.origin,after.origin)||world.root!=after.root||world.selection!=after.selection){
        if(error.empty())error="Party prop alias World changed before publication";return false;
    }
    if(!batch.finish(error))return false;
    out=std::move(pending);return true;
}

bool resolve_identity_destination(const std::string& snapshot,const std::string& key,LocalTarget& out,std::string& error){
    out={};host::RootSceneryReadBatch batch;
    if(snapshot!=net::current_snapshot_id()||key.empty()){error="Referenced prop key requires the current canonical snapshot";return false;}
    auto& free=native_free_runtime::Runtime::instance();native_free_runtime::Receipt heap;
    if(!free.read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState world;
    if(!reader.current(world,true,error))return false;
    std::map<uint64_t,const Bound*> indexed;std::set<uint64_t> omitted;
    if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;
    const auto found=registry.find(registry_id(snapshot,key));
    if(found==registry.end()){error="Referenced prop key has no current canonical identity";return false;}
    const auto unique=indexed.find(found->second.uid);
    if(unique==indexed.end()||unique->second!=&found->second){error="Referenced prop key has no unique active UID binding";return false;}
    auto& saved=found->second;native::BuildingState settled;
    if(!scenery_kiss::read_action_prop(saved.uid,settled,error))return false;
    if(settled.prop.uid!=saved.uid||saved.metadata.at("type")!=settled.type||saved.metadata.at("variant")!=settled.variant||
       saved.metadata.at("x")!=settled.prop.x||saved.metadata.at("z")!=settled.prop.z||!std::isfinite(settled.prop.y)||
       (saved.metadata.contains("rotation")&&saved.metadata.at("rotation")!=settled.rotation)||
       (saved.metadata.contains("scale")&&saved.metadata.at("scale")!=settled.scale)){
        error="shared building identity changed during native height readback";return false;
    }
    if(saved.metadata.at("y")!=settled.prop.y){
        LOG_INFO("[Action] Native terrain height settled key={} before={} after={}",saved.key,saved.metadata.at("y").dump(),settled.prop.y);
        saved.metadata["y"]=settled.prop.y;
    }
    LocalTarget pending;
    if(!building_identity_resolve(found->second,world,api,pending,error))return false;
    if(pending.snapshot_id!=snapshot||pending.record_key!=key||pending.uid!=found->second.uid||
       pending.world_epoch!=world.origin.world_epoch||pending.world_generation!=world.origin.generation){
        error="Referenced prop key resolved a different canonical lifetime";return false;
    }
    if(!batch.finish(error))return false;
    out=std::move(pending);return true;
}

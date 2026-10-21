namespace scene_scenery {
namespace {
struct SavedAdmission {dungeon_scene_identity_runtime::World world;std::vector<dungeon_scene_identity_runtime::ScenerySaved> rows;scenery_scene_snapshot_alias::Manifest aliases;bool consumed=false;};
std::optional<SavedAdmission> saved_admission;
}
bool registered(const std::string& snapshot,const std::string& key,uint64_t uid){
    const auto found=registry.find(registry_id(snapshot,key));
    return found!=registry.end()&&found->second.generation==registry_generation&&found->second.snapshot==snapshot&&
        found->second.uid==uid&&!found->second.unknown&&!found->second.scene_owner.empty()&&found->second.scene_owner!="retired"&&found->second.scene_owner!="world"&&
        found->second.class_addresspoint==uintptr_t(GetModuleHandleW(nullptr))+0x16c1390;
}
bool prepare_saved(const dungeon_scene_identity_runtime::Census& census,const scenery_scene_snapshot_alias::Manifest& aliases,std::string& error){
    if(!census.snapshot_census||!aliases.present||!scenery_scene_snapshot_alias::valid(aliases,census.world.sha256,error)){
        if(error.empty())error="Scene Scenery alias admission requires its exact saved census/companion";return false;}
    auto complete=aliases;
    // Earlier companions had no root Scenery owner. Their exact native file is
    // still authoritative for first root admission; new revision7 must be complete.
    if(!complete.root_present){
        for(const auto& row:census.scenery_saved)if(scenery_scene_key::root_saved(row.record))complete.rows.push_back({row.record,row.record});
        complete.root_present=true;std::sort(complete.rows.begin(),complete.rows.end(),[](const auto& a,const auto& b){return a.record<b.record;});
    }
    if(!scenery_scene_snapshot_alias::valid(complete,census.world.sha256,error))return false;
    std::set<std::string> records;std::set<uintptr_t> pointers;std::set<uint64_t> uids;
    for(const auto& row:census.scenery_saved)if(!scenery_scene_key::saved(row.record)||row.record.substr(0,64)!=census.world.sha256||
       !scenery_scene_key::scene(row.scene)||!row.object||!row.uid||!row.manager||!row.shard||!row.region||
       !records.insert(row.record).second||!pointers.insert(row.object).second||!uids.insert(row.uid).second){error="Scene Scenery complete saved census has an invalid or duplicate record/lifetime";return false;}
    if(records.size()!=complete.rows.size()){error="Scene Scenery companion omits or adds an original saved member";return false;}
    for(const auto& row:complete.rows)if(!records.contains(row.record)){error="Scene Scenery companion contains a foreign original record";return false;}
    saved_admission=SavedAdmission{census.world,census.scenery_saved,std::move(complete),false};error.clear();return true;
}
bool capture_saved(const dungeon_scene_identity_runtime::Census& census,scenery_scene_snapshot_alias::Manifest& out,std::string& error){
    out={};if(!census.snapshot_census){error="Scene Scenery alias capture requires its exact original snapshot census";return false;}
    scenery_scene_snapshot_alias::Manifest next;next.present=true;next.root_present=true;const auto snapshot=net::current_snapshot_id();
    const prop_lease::World world{census.world.world_epoch,census.world.generation,census.world.world,census.world.map};
    const auto ap=uintptr_t(GetModuleHandleW(nullptr))+0x16c1390;
    bool root_initialized=false,scene_initialized=false;size_t alive=0,matched=0;
    if(saved_admission&&saved_admission->consumed&&saved_admission->world.uuid==snapshot&&
       prop_lease::World{saved_admission->world.world_epoch,saved_admission->world.generation,saved_admission->world.world,saved_admission->world.map}==world)
        root_initialized=saved_admission->aliases.root_present;
    for(const auto& [id,entry]:registry){(void)id;
        if(entry.generation!=registry_generation||entry.world!=world||entry.snapshot!=snapshot||entry.class_addresspoint!=ap)continue;
        root_initialized=root_initialized||entry.scene_owner=="world";
        scene_initialized=scene_initialized||(!entry.scene_owner.empty()&&entry.scene_owner!="world"&&entry.scene_owner!="retired");
        if(entry.unknown){error="Scene Scenery snapshot contains an unresolved current alias";return false;}
        if(entry.revoked)next.retired.insert(entry.key);else ++alive;
    }
    // Retired companion-only rows have no live UID or class receiver.
    for(const auto& [id,entry]:registry){(void)id;
        if(entry.generation==registry_generation&&entry.world==world&&entry.snapshot==snapshot&&entry.scene_owner=="retired")next.retired.insert(entry.key);
    }
    std::set<std::string> keys;
    for(const auto& row:census.scenery_saved){std::string key=row.record;const Bound* found=nullptr;
        for(const auto& [id,entry]:registry){(void)id;
            if(entry.generation!=registry_generation||entry.world!=world||entry.snapshot!=snapshot||entry.revoked||entry.unknown||entry.uid!=row.uid||entry.class_addresspoint!=ap)continue;
            if(found){error="Scene Scenery snapshot full UID aliases multiple records";return false;}found=&entry;}
        if(found){
            if(found->object!=row.object||(!found->scene_owner.empty()&&(found->scene_owner!=row.scene||found->scene_manager!=row.manager||found->scene_shard!=row.shard||found->scene_region!=row.region))||
               (found->scene_owner.empty()&&row.scene!="world")){error="Scene Scenery snapshot member differs from its exact current alias/owner";return false;}
            ++matched;key=found->key;
        }else if(row.scene=="world"?root_initialized:scene_initialized){error="Scene Scenery snapshot member lacks its exact current alias/owner";return false;}
        if(!keys.insert(key).second){error="Scene Scenery snapshot repeats a canonical key";return false;}next.rows.push_back({row.record,key});
    }
    if(matched!=alive){error="Scene Scenery snapshot omits a current confirmed member";return false;}
    std::sort(next.rows.begin(),next.rows.end(),[](const auto& a,const auto& b){return a.record<b.record;});
    if(!scenery_scene_snapshot_alias::valid(next,census.world.sha256,error))return false;out=std::move(next);error.clear();return true;
}
bool append_saved(json& list,std::string& error){
    if(!saved_admission||saved_admission->consumed)return true;auto& saved=*saved_admission;
    host::opened::CurrentWorld opened;if(!current(opened,error))return false;
    const prop_lease::World world{opened.world_epoch,opened.generation,opened.world,opened.map};
    // The native observer can seal the next file before session authority adopts it.
    if(saved.world.uuid!=net::current_snapshot_id()||world!=prop_lease::World{saved.world.world_epoch,saved.world.generation,saved.world.world,saved.world.map}||
       opened.game!=saved.world.game||opened.mode!=saved.world.mode||opened.state!=saved.world.state)return true;
    size_t roots=0;for(const auto& [id,entry]:registry){(void)id;
        if(entry.generation!=registry_generation||entry.world!=world||entry.snapshot!=saved.world.uuid||!entry.scene_owner.empty()||entry.revoked||entry.unknown)continue;
        if(entry.sha!=saved.world.sha256){error="Scene Scenery pending companion differs from the admitted native snapshot file SHA";return false;}++roots;
    }
    if(!roots&&std::any_of(saved.rows.begin(),saved.rows.end(),[](const auto& row){return row.scene!="world";})){error="Scene Scenery pending companion has no admitted original snapshot parent";return false;}
    host::RootSceneryReadBatch batch;
    auto after=registry;auto counter=next_entry;
    // A new shared snapshot keeps canonical keys but replaces local alias scope.
    // Old same-world Scenery entries must not compete with the newly admitted UID.
    for(auto& [id,entry]:after){(void)id;
        if(entry.generation==registry_generation&&entry.world==world&&entry.snapshot!=saved.world.uuid&&
           entry.class_addresspoint==uintptr_t(GetModuleHandleW(nullptr))+0x16c1390)entry.revoked=true;
    }
    for(const auto& row:saved.rows){const auto alias=std::find_if(saved.aliases.rows.begin(),saved.aliases.rows.end(),[&](const auto& entry){return entry.record==row.record;});
        if(alias==saved.aliases.rows.end()){error="Scene Scenery pending saved alias disappeared";return false;}
        Row actual;dungeon_scene_identity_runtime::World observed;
        if(!owner(row.object,row.uid,actual,observed,error)||!(row.scene=="world"?observed.game==saved.world.game&&observed.mode==saved.world.mode&&observed.state==saved.world.state&&observed.world==saved.world.world&&observed.map==saved.world.map&&observed.world_epoch==saved.world.world_epoch&&observed.generation==saved.world.generation:scene_clone::same_world(observed,saved.world))||actual.scene!=row.scene||actual.manager!=row.manager||actual.shard!=row.shard||actual.region!=row.region){
            if(error.empty())error="Scene Scenery original saved member changed before shared alias admission";return false;}
        native::BuildingState value;if(!scenery_kiss::read_action_prop(row.uid,value,error))return false;
        const auto id=registry_id(saved.world.uuid,alias->key);if(after.contains(id)||counter==UINT64_MAX){error="Scene Scenery saved alias is reused or entry counter exhausted";return false;}
        Bound fresh;fresh.snapshot=saved.world.uuid;fresh.key=alias->key;fresh.sha=saved.world.sha256;fresh.uid=row.uid;fresh.entry=++counter;fresh.generation=registry_generation;
        fresh.world=world;fresh.metadata=building_metadata(value);fresh.object=row.object;fresh.class_addresspoint=uintptr_t(GetModuleHandleW(nullptr))+0x16c1390;fresh.scene_owner=row.scene;if(row.scene=="world")fresh.scene_record=row.record;fresh.scene_manager=row.manager;fresh.scene_shard=row.shard;fresh.scene_region=row.region;
        after.emplace(id,std::move(fresh));
    }
    for(const auto& key:saved.aliases.retired){const auto id=registry_id(saved.world.uuid,key);if(after.contains(id)||counter==UINT64_MAX){error="Scene Scenery retired saved alias is reused or entry counter exhausted";return false;}
        Bound retired;retired.snapshot=saved.world.uuid;retired.key=key;retired.sha=saved.world.sha256;retired.entry=++counter;retired.generation=registry_generation;retired.world=world;retired.revoked=true;retired.scene_owner="retired";after.emplace(id,std::move(retired));}
    if(!batch.finish(error))return false;
    for(const auto& row:saved.rows){const auto alias=std::find_if(saved.aliases.rows.begin(),saved.aliases.rows.end(),[&](const auto& entry){return entry.record==row.record;});const auto& entry=after.at(registry_id(saved.world.uuid,alias->key));
        list.push_back({{"snapshot_id",entry.snapshot},{"record_key",entry.key},{"local_uid",entry.uid},{"entry_id",entry.entry},{"registry_generation",entry.generation},{"revoked",false},{"unknown",false},
            {"type",entry.metadata.at("type")},{"variant",entry.metadata.at("variant")},{"x",entry.metadata.at("x")},{"y",entry.metadata.at("y")},{"z",entry.metadata.at("z")},
            {"rotation",entry.metadata.at("rotation")},{"scale",entry.metadata.at("scale")}});
        if(!entry.scene_record.empty())list.back()["native_record_key"]=entry.scene_record;}
    registry=std::move(after);next_entry=counter;saved.consumed=true;error.clear();return true;
}
bool owner(uintptr_t object,uint64_t uid,Row& out,dungeon_scene_identity_runtime::World& world,std::string& error){
    out={};world={};if(!object||!uid){error="Scene Scenery owner requires a complete native pointer/full UID";return false;}
    dungeon_scene_identity_runtime::ScenerySaved root;bool root_member=false;
    if(!dungeon_scene_identity_runtime::root_scenery_owner(object,uid,root,world,root_member,error))return false;
    if(root_member){out={"","world",object,root.shard,root.region,root.manager,uid,true};error.clear();return true;}
    scene_clone::Raw first,second;
    if(!scene_clone::read_raw(first,error)||!scene_clone::read_raw(second,error))return false;
    if(!scene_clone::same_world(first.world,second.world)||first.pools.size()!=second.pools.size()){
        error="Scene Scenery complete current world/Scene count changed";return false;}
    size_t found=0;Row actual;
    for(size_t n=0;n<first.pools.size();++n){const auto& before=first.pools[n];const auto& after=second.pools[n];
        if(!scene_clone::same_owner(before,after)||before.scenery!=after.scenery){error="Scene Scenery complete current manager/values changed";return false;}
        for(const auto& member:before.scenery.members)if(member.object==object&&member.uid==uid){
            ++found;actual={"",before.scene.owner.key,object,before.scene.owner.shard,before.scene.owner.region,before.managers[0],uid,true};}
    }
    if(found!=1){error="Scene Scenery exact full UID is absent or ambiguous in current managers";return false;}
    out=std::move(actual);world=first.world;error.clear();return true;
}
bool resolve(const dungeon_scene_identity_runtime::World& expected,uintptr_t object,uint64_t uid,std::string& key,std::string& error){
    key.clear();Row row;dungeon_scene_identity_runtime::World world;
    if(!owner(object,uid,row,world,error))return false;
    const prop_lease::World lifetime{world.world_epoch,world.generation,world.world,world.map};
    if(!scene_clone::same_world(expected,world)||unknown_locked||admitted_world!=lifetime){error="Scene Scenery canonical alias world is foreign or unadmitted";return false;}
    const Bound* matched=nullptr;
    for(const auto& [id,entry]:registry){(void)id;
        if(entry.generation!=registry_generation||entry.snapshot!=net::current_snapshot_id()||entry.world!=lifetime||entry.uid!=uid||entry.revoked||entry.unknown)continue;
        if(matched){error="Scene Scenery full UID has multiple canonical aliases";return false;}matched=&entry;
    }
    if(!matched||!scenery_scene_key::valid(matched->key)||matched->scene_owner!=row.scene||matched->scene_manager!=row.manager||matched->scene_shard!=row.shard||matched->scene_region!=row.region||matched->object!=object||matched->class_addresspoint!=uintptr_t(GetModuleHandleW(nullptr))+0x16c1390){
        error="Scene Scenery exact current unrevoked canonical alias is unavailable";return false;}
    key=matched->key;error.clear();return true;
}
}

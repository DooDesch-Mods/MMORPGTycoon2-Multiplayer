// A shared capture names canonical pools; NativeOpen retains the exact local loaded file.
template<class Left,class Right>bool same_world_zone_origin(const Left& a,const Right& b){
    return same_world(a,b)&&a.uuid==b.uuid&&a.sha256==b.sha256;
}
bool resolve_world_zone(const action_session::Binding& binding,const std::string& snapshot,uintptr_t object,uint64_t uid,
                        world_zone_runtime::Current& out,std::string& error){
    out={};std::lock_guard lock(gate);opened::CurrentWorld world;
    if(!healthy(error)||GetCurrentThreadId()!=main_thread||host_active||client_active||snapshot!=uuid||net::current_snapshot_id()!=uuid||
       !uid||!object||!origin.generation||!origin.world_epoch||binding.native_epoch!=origin.world_epoch||binding.native_generation!=origin.generation||
       !native_open_runtime::read_current_world(world,error)||!same_world_zone_origin(origin,world))
        return q::fail(error,error.empty()?"World Zone target requires its exact admitted snapshot/world binding":error);
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    auto find=[&](uint64_t value,uintptr_t& pointer,std::string& cause){const auto exception=exact_uid(value,pointer);
        return !exception||q::fail(cause,"World Zone target FindUID failed exception="+std::to_string(exception));};
    uintptr_t root=0,region=0,manager=0,registered=0;uint64_t actual_uid=0;int32_t type=-1;
    if(!q::typed(memory,image,object,0x1745ce0,error)||!at(object,0x18,actual_uid,error)||!at(object,0x170,root,error)||
       !at(object,0x178,region,error)||!at(object,0x24c,type,error)||!at(region,0x968,manager,error)||!find(uid,registered,error))return false;
    if(actual_uid!=uid||registered!=object||type<0||type>3)return q::fail(error,"World Zone target exact full UID/class/type differs");
    const q::SavedPool* monsters=nullptr;std::string scene="root";const auto map=reinterpret_cast<uintptr_t>(world.map);
    if(root==map+0x1c0){int32_t id=-1;uintptr_t resolved=0;
        if(!at(region,0x834,id,error)||!region_saved_identity::resolve(image,map,root,id,region,resolved,at,error))return false;
        const auto known=pools.find(id);if(known==pools.end())return q::fail(error,"World Zone target root owner has no confirmed snapshot pool");monsters=&known->second;
    }else{
        dungeon_scene_identity_runtime::World scene_world;std::vector<dungeon_scene_roster::Scene> scenes;
        if(!dungeon_scene_identity_runtime::read_scene_roster(scene_world,scenes,error)||!same_world_zone_origin(scene_world,world))
            return q::fail(error,error.empty()?"World Zone target Dungeon world differs":error);
        const auto owner=std::find_if(scenes.begin(),scenes.end(),[&](const auto& value){return value.owner.shard==root&&value.owner.region==region;});
        const auto known=scene_pools.find(manager);
        if(owner==scenes.end()||known==scene_pools.end()||known->second.scene.owner.key!=owner->owner.key||
           known->second.scene.owner.dungeon!=owner->owner.dungeon||known->second.scene.dungeon_uid!=owner->dungeon_uid||
           known->second.scene.owner.region!=region||known->second.scene.owner.shard!=root)
            return q::fail(error,"World Zone target Dungeon owner is not a current confirmed scene");
        monsters=&known->second.saved;scene=owner->owner.key;
    }
    dungeon_zone_roster::Census actual;if(!dungeon_zone_roster::read(at,find,image,manager,root,region,actual,error))return false;
    if(std::none_of(actual.members.begin(),actual.members.end(),[&](const auto& member){return member.object==object&&member.uid==uid&&member.type==type;}))
        return q::fail(error,"World Zone target absent from its complete current owner array");
    std::string key;
    if(type==1){const auto known=std::find_if(monsters->array.zones.begin(),monsters->array.zones.end(),[&](const auto& row){return row.pointer==object&&row.uid==uid;});
        if(known==monsters->array.zones.end())return q::fail(error,"World Zone target Monster lacks confirmed canonical alias");key=q::zone_key(*monsters,*known);
    }else{const auto pool=world_zone_pools.find(manager);
        if(pool==world_zone_pools.end()||pool->second.root!=root||pool->second.region!=region||pool->second.scene!=scene)
            return q::fail(error,"World Zone target non-Monster owner lacks confirmed identity");
        const auto known=pool->second.rows.find(uid);
        if(known==pool->second.rows.end()||known->second.object!=object||known->second.type!=type)return q::fail(error,"World Zone target non-Monster full UID/type lacks confirmed alias");key=known->second.key;
    }
    opened::CurrentWorld after;
    if(key.empty()||!native_open_runtime::read_current_world(after,error)||!same_world_zone_origin(world,after))
        return q::fail(error,error.empty()?"World Zone target world changed before readback":error);
    out={key,scene,object,root,region,manager,uid,type};error.clear();return true;
}
bool read_world_zones(const action_session::Binding& binding,const std::string& snapshot,
                      std::vector<world_zone_runtime::Current>& out,std::string& error){
    out.clear();dungeon_scene_identity_runtime::World scene_world;std::vector<dungeon_scene_roster::Scene> scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(scene_world,scenes,error))return false;
    std::lock_guard lock(gate);opened::CurrentWorld world;
    if(!healthy(error)||GetCurrentThreadId()!=main_thread||host_active||client_active||snapshot!=uuid||net::current_snapshot_id()!=uuid||
       !origin.generation||!origin.world_epoch||binding.native_epoch!=origin.world_epoch||binding.native_generation!=origin.generation||
       !native_open_runtime::read_current_world(world,error)||!same_world_zone_origin(origin,world)||
       !same_world_zone_origin(scene_world,world))
        return q::fail(error,error.empty()?"World Zone current roster requires its exact admitted snapshot/world binding":error);
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    auto find=[&](uint64_t uid,uintptr_t& value,std::string& cause){
#ifdef _WIN32
        mt2mp::game::live_host_state::apply_timing::PageReadScope original_lookup(mt2mp::game::live_host_state::apply_timing::PageBoundary::OriginalCall);
#endif
        const auto exception=exact_uid(uid,value);
        return !exception||q::fail(cause,"World Zone current FindUID failed exception="+std::to_string(exception));};
    std::vector<world_zone_runtime::Current> next;std::set<std::string> keys;std::set<uintptr_t> managers,pointers;std::set<uint64_t> uids;
    const auto add=[&](uintptr_t manager,uintptr_t root,uintptr_t region,const std::string& scene,const q::SavedPool& monsters)->bool{
#ifdef _WIN32
        mt2mp::game::live_host_state::apply_timing::PageReadScope zone_pages(!mt2mp::game::live_host_state::apply_timing::PageReadScope::reader_active());
#endif
        if(!managers.insert(manager).second)return q::fail(error,"World Zone current manager duplicated");
        dungeon_zone_roster::Census actual;if(!dungeon_zone_roster::read(at,find,image,manager,root,region,actual,error))return false;
        const auto saved=world_zone_pools.find(manager);size_t extra_count=0,monster_count=0;
        for(const auto& row:actual.members){std::string key;
            if(row.type==1){++monster_count;const auto known=std::find_if(monsters.array.zones.begin(),monsters.array.zones.end(),[&](const auto& member){return member.pointer==row.object&&member.uid==row.uid;});
                if(known==monsters.array.zones.end())return q::fail(error,"World Zone current Monster lacks confirmed Quest alias");key=q::zone_key(monsters,*known);
            }else{++extra_count;if(saved==world_zone_pools.end())return q::fail(error,"World Zone non-Monster scene lacks confirmed serialization/lifecycle binding");
                const auto known=saved->second.rows.find(row.uid);
                if(saved->second.root!=root||saved->second.region!=region||saved->second.scene!=scene||known==saved->second.rows.end()||
                   known->second.object!=row.object||known->second.type!=row.type)return q::fail(error,"World Zone current non-Monster full UID/type/owner lacks confirmed alias");
                key=known->second.key;
            }
            if(key.empty()||!keys.insert(key).second||!pointers.insert(row.object).second||!uids.insert(row.uid).second)
                return q::fail(error,"World Zone current canonical key/pointer/full UID duplicated");
            next.push_back({key,scene,row.object,root,region,manager,row.uid,row.type});
        }
        if(monster_count!=monsters.array.zones.size()||extra_count!=(saved==world_zone_pools.end()?0:saved->second.rows.size()))
            return q::fail(error,"World Zone current roster omitted a confirmed live member");
        return true;
    };
    const auto map=reinterpret_cast<uintptr_t>(world.map),root=map+0x1c0;
    region_saved_identity::Index region_index;
    const auto capture_regions=[&](){
#ifdef _WIN32
        mt2mp::game::live_host_state::apply_timing::PageReadScope region_pages;
#endif
        return region_saved_identity::capture(image,map,root,region_index,at,error);
    };
    if(!capture_regions())return false;
    if(region_index.saved.size()!=pools.size())return q::fail(error,"World Zone current complete root Region count differs");
    {
#ifdef _WIN32
    mt2mp::game::live_host_state::apply_timing::PageReadScope owner_pages(!mt2mp::game::live_host_state::apply_timing::PageReadScope::reader_active());
#endif
    for(const auto& [id,region]:region_index.saved){uintptr_t manager=0;const auto saved=pools.find(id);
        if(saved==pools.end())return q::fail(error,"World Zone current root Region lacks confirmed snapshot pool");
        if(!at(region,0x968,manager,error)||!add(manager,root,region,"root",saved->second))return false;}
    if(scenes.size()!=scene_pools.size())return q::fail(error,"World Zone current complete Dungeon scene count differs");
    for(const auto& scene:scenes){uintptr_t manager=0;if(!at(scene.owner.region,0x968,manager,error))return false;
        const auto saved=scene_pools.find(manager);
        if(saved==scene_pools.end()||saved->second.scene.owner.key!=scene.owner.key||saved->second.scene.owner.shard!=scene.owner.shard||
           saved->second.scene.owner.region!=scene.owner.region||saved->second.scene.owner.dungeon!=scene.owner.dungeon||saved->second.scene.dungeon_uid!=scene.dungeon_uid)
            return q::fail(error,"World Zone current Dungeon scene differs from confirmed Quest owner");
        if(!add(manager,scene.owner.shard,scene.owner.region,scene.owner.key,saved->second.saved))return false;
    }
    for(const auto& [manager,saved]:world_zone_pools)if(!managers.contains(manager)&&!saved.rows.empty())
        return q::fail(error,"World Zone non-Monster owner retired without confirmed lifecycle");
    }
    opened::CurrentWorld after;
    const auto verify_regions=[&](){
#ifdef _WIN32
        mt2mp::game::live_host_state::apply_timing::PageReadScope region_pages;
#endif
        return region_saved_identity::verify(image,map,root,region_index,at,error);
    };
    if(!verify_regions()||!native_open_runtime::read_current_world(after,error)||!same_world_zone_origin(world,after))
        return q::fail(error,error.empty()?"World Zone owner generation changed during current census":error);
    out=std::move(next);error.clear();return true;
}

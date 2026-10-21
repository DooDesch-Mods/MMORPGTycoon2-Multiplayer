struct CloneData {std::map<uintptr_t,WorldZonePool> world_before,world_after;std::map<uintptr_t,ScenePool> before,after;dungeon_scene_identity_runtime::World world;bool prepared=false;};
namespace {
#include "../world_zone_clone.inc"
bool clone_current(const dungeon_scene_identity_runtime::World& world,std::string& error){opened::CurrentWorld actual;
    return healthy(error)&&GetCurrentThreadId()==main_thread&&!host_active&&!client_active&&world.uuid==origin.uuid&&world.sha256==origin.sha256&&net::current_snapshot_id()==uuid&&
        native_open_runtime::read_current_world(actual,error)&&actual.uuid==world.uuid&&actual.sha256==world.sha256&&same_world(origin,actual)&&same_world(world,actual)||q::fail(error,error.empty()?"Scene clone Quest Zone current world/Mainthread differs":error);
}
bool clone_same(const std::map<uintptr_t,ScenePool>& a,const std::map<uintptr_t,ScenePool>& b){
    if(a.size()!=b.size())return false;
    for(const auto& [manager,left]:a){const auto found=b.find(manager);if(found==b.end())return false;const auto& right=found->second;
        scene_clone::Pool x,y;x.scene=left.scene;y.scene=right.scene;x.managers[1]=left.manager;y.managers[1]=right.manager;
        if(!scene_clone::same_owner(x,y)||left.saved.origin!=right.saved.origin||left.saved.saved_region!=right.saved.saved_region||left.saved.array!=right.saved.array||left.saved.action_keys!=right.saved.action_keys)return false;
    }return true;
}
}
bool clone_alias(const scene_clone::Raw& raw,const scene_clone::Pool& pool,const scene_clone::Member& member,std::string& key,std::string& error){
    key.clear();std::lock_guard lock(gate);if(!clone_current(raw.world,error))return false;
    if(scene_clone::zone_type(member.family)>=0)return clone_world_alias(pool,member,key,error);
    const auto found=scene_pools.find(pool.managers[1]);
    if(found==scene_pools.end())return q::fail(error,"Scene clone retained Zone manager has no confirmed aliases");
    scene_clone::Pool expected;expected.scene=found->second.scene;expected.managers=pool.managers;expected.building_manager=pool.building_manager;
    if(!scene_clone::same_owner(expected,pool)||!validate_scene(found->second,error))return q::fail(error,error.empty()?"Scene clone retained Zone owner differs":error);
    const auto row=std::find_if(found->second.saved.array.zones.begin(),found->second.saved.array.zones.end(),[&](const auto& value){return value.pointer==member.object&&value.uid==member.uid;});
    if(row==found->second.saved.array.zones.end())return q::fail(error,"Scene clone retained Zone full UID lacks confirmed alias");
    key=q::zone_key(found->second.saved,*row);error.clear();return true;
}
bool preview_clones(const scene_clone::Raw& raw,const scene_clone::Mapping& mapping,ClonePreview& out,std::string& error,const std::string& template_exterior,const scene_clone::Raw* restoration,const scene_clone::Raw* held){
    out={};std::lock_guard lock(gate);if(!clone_current(raw.world,error)||!scene_clone::valid(mapping,error))return false;
    auto next=std::make_shared<CloneData>();next->before=scene_pools;next->after=scene_pools;next->world=raw.world;next->world_before=world_zone_pools;
    const auto allowed_template=[&](const dungeon_scene_roster::Scene& scene){return !template_exterior.empty()&&scene.owner.type==1&&scene.owner.exterior_key==template_exterior;};
    const auto restored_scene=[&](const std::string& key){return restoration&&std::any_of(restoration->pools.begin(),restoration->pools.end(),[&](const auto& p){return p.scene.owner.key==key&&p.scene.owner.exterior_key==template_exterior;});};
    if(!preview_world_zones(raw,mapping,next->world_after,error,template_exterior,restoration,held))return false;
    for(const auto& [manager,old]:scene_pools){
        const auto live=std::find_if(raw.pools.begin(),raw.pools.end(),[&](const auto& pool){return pool.managers[1]==manager;});
        if(live!=raw.pools.end()){
            scene_clone::Pool expected;expected.scene=old.scene;expected.managers=live->managers;expected.building_manager=live->building_manager;expected.managers[1]=old.manager;
            if(!scene_clone::same_owner(expected,*live)||!validate_scene(next->after.at(manager),error))return q::fail(error,error.empty()?"Scene clone surviving Quest Zone owner differs":error);
            continue;
        }
        if(old.scene.owner.type!=2&&!allowed_template(old.scene))return q::fail(error,"Scene clone cannot retire a Quest Zone template pool outside the exact pending action");
        for(const auto& zone:old.saved.array.zones){const auto key=q::zone_key(old.saved,zone);uintptr_t found{};const auto exception=exact_uid(zone.uid,found);
            if(exception||(scene_clone::held_member(held,old.scene.owner.key,zone.pointer,zone.uid)?found!=zone.pointer:found!=0))return q::fail(error,"Scene clone Quest Zone original deletion incomplete SEH="+std::to_string(exception));
            const auto assignment=std::find_if(mapping.retired.begin(),mapping.retired.end(),[&](const auto& row){return row.family==scene_clone::Family::MonsterZone&&row.scene==old.scene.owner.key&&row.key==key;});
            if(assignment==mapping.retired.end())return q::fail(error,"Scene clone Quest Zone retirement omitted canonical member");
        }
        next->after.erase(manager);
    }
    std::set<std::string> keys;for(const auto& [manager,pool]:next->after){(void)manager;for(const auto& zone:pool.saved.array.zones)if(!keys.insert(q::zone_key(pool.saved,zone)).second)return q::fail(error,"Scene clone existing Quest Zone aliases duplicate");}
    for(const auto& pool:raw.pools){const auto manager=pool.managers[1];if(next->after.contains(manager))continue;
        if(pool.scene.owner.type!=2&&!allowed_template(pool.scene))return q::fail(error,"Scene clone new Quest Zone manager is not an Instance or exact pending template");
        ScenePool fresh;fresh.scene=pool.scene;fresh.manager=manager;fresh.saved.origin={uuid,sha,raw.world.world_epoch,raw.world.generation};fresh.saved.saved_region=pool.scene.exterior_region;fresh.saved.array=pool.zones;
        for(size_t child=0;child<fresh.saved.array.zones.size();++child){const auto& zone=fresh.saved.array.zones[child];
            const auto assignment=std::find_if(mapping.members.begin(),mapping.members.end(),[&](const auto& row){return row.family==scene_clone::Family::MonsterZone&&row.scene==pool.scene.owner.key&&row.child==child;});
            if(assignment==mapping.members.end()||(assignment->origin.empty()&&!restored_scene(pool.scene.owner.key))||!keys.insert(assignment->key).second||!fresh.saved.action_keys.emplace(zone.pointer,assignment->key).second)return q::fail(error,"Scene clone Quest Zone exact ordered birth assignment missing or reused");
        }
        if(!validate_scene(fresh,error)||!next->after.emplace(manager,std::move(fresh)).second)return false;
    }
    if(next->after.size()!=raw.pools.size())return q::fail(error,"Scene clone Quest Zone preview omitted native scene");
    next->prepared=true;out.state=std::move(next);error.clear();return true;
}
bool check_clones(const ClonePreview& preview,std::string& error){std::lock_guard lock(gate);
    return preview.state&&preview.state->prepared&&clone_current(preview.state->world,error)&&clone_same(scene_pools,preview.state->before)&&clone_world_same(world_zone_pools,preview.state->world_before)||q::fail(error,error.empty()?"Scene clone Quest Zone preview changed or consumed":error);
}
bool publish_clones(ClonePreview& preview,std::string& error){std::lock_guard lock(gate);
    if(!preview.state||!preview.state->prepared||!clone_current(preview.state->world,error)||!clone_same(scene_pools,preview.state->before)||!clone_world_same(world_zone_pools,preview.state->world_before))return q::fail(error,error.empty()?"Scene clone Quest Zone publication differs from complete preview":error);
    scene_pools=std::move(preview.state->after);world_zone_pools=std::move(preview.state->world_after);preview.state->prepared=false;error.clear();return true;
}

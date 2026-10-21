namespace root_saved_fixture {
namespace scenery_scene_key=mt2mp::game::scenery_scene_key;
namespace scenery_scene_snapshot_alias=mt2mp::game::scenery_scene_snapshot_alias;
namespace dungeon_scene_identity_runtime=mt2mp::game::dungeon_scene_identity_runtime;
namespace prop_lease {struct World {uint64_t epoch=0,generation=0;void* world=nullptr;void* map=nullptr;bool operator==(const World&)const=default;};}
namespace net {inline std::string current_snapshot_id(){return "11111111-2222-4333-8444-555555555555";}}
inline void* GetModuleHandleW(const void*){return reinterpret_cast<void*>(uintptr_t(0x100000));}
using json=nlohmann::json;
struct Bound {
    uint64_t generation=4,uid=0,entry=0;std::string sha;json metadata;prop_lease::World world;std::string snapshot,key,scene_owner,scene_record;
    uintptr_t object=0,class_addresspoint=0,scene_manager=0,scene_shard=0,scene_region=0;
    bool unknown=false,revoked=false;
};
struct SavedAdmission {dungeon_scene_identity_runtime::World world;std::vector<dungeon_scene_identity_runtime::ScenerySaved> rows;scenery_scene_snapshot_alias::Manifest aliases;bool consumed=false;};
inline std::map<std::string,Bound> registry;
inline std::optional<SavedAdmission> saved_admission;
inline uint64_t registry_generation=4;
struct Row {std::string key,scene;uintptr_t object=0,shard=0,region=0,manager=0;uint64_t uid=0;bool scenery=false;};
namespace host::opened {using CurrentWorld=dungeon_scene_identity_runtime::World;}
inline dungeon_scene_identity_runtime::World fixture_world;
inline std::vector<dungeon_scene_identity_runtime::ScenerySaved> fixture_members;
inline std::string fixture_read_failure;
inline uint64_t next_entry=10;
inline bool current(host::opened::CurrentWorld& out,std::string& e){
    if(fixture_read_failure=="world"){e="original NativeOpen world failure";return false;}out=fixture_world;return true;
}
inline bool owner(uintptr_t object,uint64_t uid,Row& out,dungeon_scene_identity_runtime::World& world,std::string& e){
    if(fixture_read_failure=="owner"){e="original Scenery owner failure";return false;}
    const auto found=std::find_if(fixture_members.begin(),fixture_members.end(),[&](const auto& row){return row.object==object&&row.uid==uid;});
    if(found==fixture_members.end()){e="original full UID is absent";return false;}
    out={"",found->scene,object,found->shard,found->region,found->manager,uid,true};world=fixture_world;return true;
}
namespace scene_clone {
inline bool same_world(const dungeon_scene_identity_runtime::World& a,const dungeon_scene_identity_runtime::World& b){
    return std::tie(a.game,a.mode,a.state,a.world,a.map,a.world_epoch,a.generation,a.uuid,a.sha256)==std::tie(b.game,b.mode,b.state,b.world,b.map,b.world_epoch,b.generation,b.uuid,b.sha256);
}
}
namespace host {
struct RootSceneryReadBatch {
    dungeon_scene_identity_runtime::World expected=fixture_world;
    bool finish(std::string& error){
        if(fixture_read_failure=="final"){error="original final Root census failure";return false;}
        if(!scene_clone::same_world(expected,fixture_world)){error="original final Root World changed";return false;}
        return true;
    }
};
}
namespace native {struct BuildingState {uint64_t uid=0;};}
namespace scenery_kiss {
inline bool read_action_prop(uint64_t uid,native::BuildingState& out,std::string& e){
    if(fixture_read_failure=="prop"){e="original native Scenery property failure";return false;}
    if(std::none_of(fixture_members.begin(),fixture_members.end(),[&](const auto& row){return row.uid==uid;})){e="original current Scenery UID absent";return false;}
    out.uid=uid;return true;
}
}
inline json building_metadata(const native::BuildingState& value){
    return {{"type","cactus"},{"variant",std::to_string(value.uid)},{"x",1},{"y",2},{"z",3},{"rotation",{0,0,0,1}},{"scale",{1,1,1}}};
}
inline std::string registry_id(const std::string& snapshot,const std::string& key){return snapshot+":"+key;}
#include "root_scenery_admission_body.inc"
inline dungeon_scene_identity_runtime::Census census(){
    dungeon_scene_identity_runtime::Census result;result.snapshot_census=true;
    result.world.uuid=net::current_snapshot_id();result.world.sha256=std::string(64,'b');
    result.world.world_epoch=7;result.world.generation=9;result.world.world=reinterpret_cast<void*>(uintptr_t(10));result.world.map=reinterpret_cast<void*>(uintptr_t(20));
    result.scenery_saved.push_back({scenery_scene_key::root_record(result.world.sha256,59,8,0),"world",100,200,300,400,500});
    result.scenery_saved.push_back({scenery_scene_key::root_record(result.world.sha256,59,8,1),"world",101,200,300,400,501});return result;
}
inline Bound binding(const dungeon_scene_identity_runtime::Census& c,size_t index,const std::string& key,bool legacy=false){
    const auto& row=c.scenery_saved.at(index);Bound bound;bound.world={c.world.world_epoch,c.world.generation,c.world.world,c.world.map};
    bound.snapshot=net::current_snapshot_id();bound.key=key;bound.scene_owner=legacy?"":"world";bound.uid=row.uid;bound.object=row.object;
    bound.class_addresspoint=uintptr_t(GetModuleHandleW(nullptr))+0x16c1390;bound.scene_manager=row.manager;bound.scene_shard=row.shard;bound.scene_region=row.region;return bound;
}
inline void reset(){registry.clear();saved_admission.reset();registry_generation=4;next_entry=10;fixture_read_failure.clear();fixture_members.clear();fixture_world={};}
}

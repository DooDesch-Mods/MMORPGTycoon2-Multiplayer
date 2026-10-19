#include "npc_quest_targets.h"
#include "dungeon_scene_identity_runtime.h"
#include <algorithm>
#include <iostream>
#include <mutex>
#include <stdexcept>

namespace fixture {
namespace q=mt2mp::game::npc_quest_targets;
using World=mt2mp::game::dungeon_scene_identity_runtime::World;
using Scene=mt2mp::game::dungeon_scene_roster::Scene;
namespace opened{using CurrentWorld=World;}
struct ScenePool{Scene scene;uintptr_t manager=0;q::SavedPool saved;};
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,const T& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=p[i];}
    bool read(uintptr_t address,void* value,size_t size,std::string& error){auto* p=reinterpret_cast<unsigned char*>(value);for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){error="original Quest scene field read failed";return false;}p[i]=found->second;}return true;}
}memory;
constexpr uintptr_t fixture_image=0x10000000,fixture_map=0x20000000,fixture_root=fixture_map+0x1c0,fixture_region=0x21000000;
constexpr uintptr_t scene_region=0x30000000,scene_shard=0x31000000,scene_manager=0x32000000,zone=0x33000000,root_target=0x34000000;
const std::string uuid="11111111-1111-4111-8111-111111111111",sha(64,'a');
std::mutex gate;World origin,actual_world;uintptr_t image=fixture_image;unsigned main_thread=7,current_thread=7;
bool host_active=false,client_active=false,world_failure=false,scene_failure=false,root_failure=false,validation_failure=false;
unsigned root_calls=0,scene_reads=0,validations=0;int32_t root_id=-1;uintptr_t root_pointer=0;
std::string failure,active_uuid;std::map<uintptr_t,ScenePool> scene_pools;
unsigned GetCurrentThreadId(){return current_thread;}
bool healthy(std::string& error){error=failure;return failure.empty();}
template<class A,class B>bool same_world(const A& a,const B& b){return a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map;}
namespace net{std::string current_snapshot_id(){return active_uuid;}}
namespace native_open_runtime{bool read_current_world(World& out,std::string& error){if(world_failure){error="original current world read failed";return false;}out=actual_world;return true;}}
namespace dungeon_scene_identity_runtime {
using Census=mt2mp::game::dungeon_scene_identity_runtime::Census;
Census census;
bool read_current(Census& out,std::string& error){++scene_reads;if(scene_failure){error="original current scene census failed";return false;}out=census;return true;}
}
bool validate_scene(ScenePool&,std::string& error){++validations;if(validation_failure){error="original saved scene validation failed";return false;}return true;}
bool bind_destination(int32_t id,uintptr_t pointer,std::string& key,std::string& error){++root_calls;root_id=id;root_pointer=pointer;if(root_failure){error="original root destination binding failed";return false;}key=sha+":world:building:8";return true;}
#include "dungeon_quest_destination_actual.inc"
size_t checks=0;
void need(bool okay,const std::string& cause){++checks;if(!okay)throw std::runtime_error(cause+" comparison="+std::to_string(checks));}
void setup(){
    memory={};scene_pools.clear();failure.clear();active_uuid=uuid;main_thread=current_thread=7;host_active=client_active=world_failure=scene_failure=root_failure=validation_failure=false;
    root_calls=scene_reads=validations=0;root_id=-1;root_pointer=0;origin={};origin.uuid=uuid;origin.sha256=sha;origin.generation=9;origin.world_epoch=6;origin.map=reinterpret_cast<void*>(fixture_map);actual_world=origin;
    memory.put(root_target+0x170,fixture_root);memory.put(root_target+0x178,fixture_region);memory.put(fixture_region+0x70,fixture_root);memory.put(fixture_region+0x834,int32_t(67));
    memory.put(zone+0x170,scene_shard);memory.put(zone+0x178,scene_region);memory.put(scene_region+0x70,scene_shard);
    ScenePool pool;pool.manager=scene_manager;pool.scene.owner.key=sha+":world:building:17/template";pool.scene.owner.shard=scene_shard;pool.scene.owner.region=scene_region;pool.scene.owner.dungeon=0x40000000;
    pool.scene.owner.exterior_uid=0x100000007;pool.scene.dungeon_uid=0x200000007;pool.saved.origin={uuid,sha,6,9};pool.saved.saved_region=67;
    pool.saved.array.zones.push_back(q::Zone{zone,0x300000007,7,0,{}});pool.saved.action_keys[zone]=sha+":world:zone:7";
    scene_pools.emplace(scene_manager,pool);auto& census=dungeon_scene_identity_runtime::census;census={};census.world=origin;
    mt2mp::game::dungeon_scene_identity_runtime::Pool fresh;fresh.scene=pool.scene;fresh.managers[1]=scene_manager;census.pools.push_back(fresh);
}
void tests(){
    std::string key,error;bool in_dungeon=false;setup();
    need(bind_current_destination(root_target,key,in_dungeon,error),error);
    need(!in_dungeon&&root_calls==1&&root_id==67&&root_pointer==root_target&&scene_reads==0,"Root reference left its exact existing saved Region resolver");
    setup();need(bind_current_destination(zone,key,in_dungeon,error),error);
    need(in_dungeon&&key==sha+":world:zone:7"&&root_calls==0&&scene_reads==1&&validations==1,"Scene reference used root IDs, lost canonical key or skipped current ownership");
    setup();origin.uuid="53aebc01-fd57-45c5-8b76-b45c69d59e9d";origin.sha256=std::string(64,'b');actual_world=origin;dungeon_scene_identity_runtime::census.world=origin;
    need(bind_current_destination(zone,key,in_dungeon,error),"Host local NativeOpen UUID/SHA was equated with shared saved Zone origin: "+error);
    need(in_dungeon&&key==sha+":world:zone:7"&&scene_pools.at(scene_manager).saved.origin.uuid==uuid&&root_calls==0,"Host native admission rewrote the shared Zone alias");
    for(int field=0;field<4;++field){setup();origin.uuid="53aebc01-fd57-45c5-8b76-b45c69d59e9d";origin.sha256=std::string(64,'b');actual_world=origin;auto& census=dungeon_scene_identity_runtime::census;census.world=origin;
        if(field==0)census.world.uuid=uuid;if(field==1)census.world.sha256=sha;if(field==2)actual_world.uuid=uuid;if(field==3)actual_world.sha256=sha;
        need(!bind_current_destination(zone,key,in_dungeon,error)&&!error.empty()&&root_calls==0,"Foreign native UUID/SHA acquired a saved scene alias");}
    for(int mode=0;mode<14;++mode){setup();auto& census=dungeon_scene_identity_runtime::census;
        if(mode==0)current_thread++;if(mode==1)host_active=true;if(mode==2)client_active=true;if(mode==3)active_uuid="foreign";if(mode==4)origin.generation=0;if(mode==5)origin.world_epoch=0;
        if(mode==6)actual_world.generation++;if(mode==7)memory.put(scene_region+0x70,fixture_root);if(mode==8)census.world.generation++;if(mode==9)census.pools.clear();
        if(mode==10)census.pools[0].scene.owner.exterior_uid^=uint64_t(1)<<32;if(mode==11)census.pools[0].scene.dungeon_uid^=uint64_t(1)<<32;
        if(mode==12)census.pools[0].scene.owner.key+="/foreign";if(mode==13)census.pools[0].scene.owner.shard=fixture_root;
        need(!bind_current_destination(zone,key,in_dungeon,error)&&!error.empty()&&root_calls==0,"Foreign/racing scene reference fell back to a root pool mode="+std::to_string(mode));
    }
    setup();scene_failure=true;need(!bind_current_destination(zone,key,in_dungeon,error)&&error=="original current scene census failed"&&root_calls==0,"Scene census failure was hidden or root fallback executed");
    setup();validation_failure=true;need(!bind_current_destination(zone,key,in_dungeon,error)&&error=="original saved scene validation failed"&&root_calls==0,"Saved scene identity failure was replaced");
    setup();world_failure=true;need(!bind_current_destination(zone,key,in_dungeon,error)&&error=="original current world read failed"&&scene_reads==0,"Native world read failure was replaced");
    setup();memory.bytes.erase(zone+0x170);need(!bind_current_destination(zone,key,in_dungeon,error)&&error=="original Quest scene field read failed","Native memory read cause was replaced");
    setup();root_failure=true;need(!bind_current_destination(root_target,key,in_dungeon,error)&&error=="original root destination binding failed"&&scene_reads==0,"Root binding failure caused a scene retry");
    setup();failure="original retained target observer failure";need(!bind_current_destination(zone,key,in_dungeon,error)&&error==failure,"Original observer failure was lost");
    setup();need(!bind_current_destination(zone+8,key,in_dungeon,error)&&root_calls==0,"Unbound scene pointer was inferred");
}
}
int main()try{fixture::tests();std::cout<<fixture::checks<<" actual Quest destination root/scene dispatch, owner, full UID and original cause comparisons passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}

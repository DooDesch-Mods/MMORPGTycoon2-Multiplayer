#include "npc_quest_targets.h"
#include "dungeon_scene_identity_runtime.h"
#include <iostream>
#include <optional>
#include <stdexcept>
#define LOG_INFO(...) ((void)0)
namespace fixture {
namespace q=mt2mp::game::npc_quest_targets;
namespace snapshot_zone_key=mt2mp::game::snapshot_zone_key;
using World=mt2mp::game::dungeon_scene_identity_runtime::World;
namespace opened {using CurrentWorld=World;}
using Scene=mt2mp::game::dungeon_scene_roster::Scene;
struct ScenePool {Scene scene;uintptr_t manager=0;q::SavedPool saved;};
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,const T& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=p[i];}
    bool read(uintptr_t address,void* value,size_t size,std::string& e){auto* p=reinterpret_cast<unsigned char*>(value);
        for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){e="original Dungeon Quest Zone field read failure";return false;}p[i]=found->second;}return true;}
}memory;
constexpr uintptr_t image=0x10000000,fixture_map=0x20000000,fixture_root=fixture_map+0x1c0,fixture_region=0x21000000,root_zones=0x22000000,buildings=0x23000000;
constexpr uintptr_t scene_region=0x30000000,scene_shard=0x31000000,scene_manager=0x32000000,scene_data=0x33000000;
constexpr std::array<uintptr_t,2> actors{0x40000000,0x40004000};
constexpr std::array<uint64_t,2> uids{0x100000012,0x200000012};
constexpr q::Budget budget{4096,4096,1048576,4096};
const std::string uuid="11111111-1111-4111-8111-111111111111",sha(64,'a');
uintptr_t bound_context=9;std::map<int,int> saves,loads,load_stack;std::string failure;
std::map<uintptr_t,q::Array> observed;std::map<int32_t,q::SavedPool> pools;
std::optional<std::vector<ScenePool>> staged_scenes;World staged_scene_world,origin,fixture_world;
std::map<uintptr_t,ScenePool> scene_pools;std::map<uint64_t,uintptr_t> uid_registry;unsigned long uid_exception=0;
bool healthy(std::string& e){e=failure;return failure.empty();}
bool reject(std::string& e,const std::string& cause){failure=cause;e=cause;pools.clear();scene_pools.clear();return false;}
bool same_world(const World& a,const World& b){return a.uuid==b.uuid&&a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.map==b.map;}
unsigned long exact_uid(uint64_t uid,uintptr_t& found){found=uid_registry[uid];return uid_exception;}
#include "dungeon_quest_seal_actual.inc"
size_t comparisons=0;
void need(bool okay,const std::string& why){++comparisons;if(!okay)throw std::runtime_error(why+" comparison="+std::to_string(comparisons));}
void zone(uintptr_t object,uintptr_t shard,uintptr_t owner,uint64_t uid){
    memory.put(object,image+0x1745ce0);memory.put(object+0x18,uid);memory.put(object+0x170,shard);memory.put(object+0x178,owner);
    memory.put(object+0x24c,int32_t(1));memory.put(object+0x250,int32_t(2));memory.put(object+0x360,int32_t(4));memory.put(object+0x560,int32_t(3));memory.put(object+0x56c,int32_t(12));
    memory.put(object+0x2f0,image+0x16e0140);memory.put(object+0x330,image+0x16eb080);memory.put(object+0x338,uintptr_t(0));memory.put(object+0x340,int32_t(0));memory.put(object+0x344,int32_t(0));uid_registry[uid]=object;
}
void setup(){
    memory={};failure.clear();observed.clear();pools.clear();scene_pools.clear();uid_registry.clear();uid_exception=0;
    fixture_world={};fixture_world.uuid=uuid;fixture_world.sha256=sha;fixture_world.world_epoch=7;fixture_world.generation=9;fixture_world.map=reinterpret_cast<void*>(fixture_map);staged_scene_world=fixture_world;
    memory.put(fixture_map,image+0x1742860);memory.put(fixture_root,image+0x174ad20);memory.put(fixture_root+0x48,int32_t(0));memory.put(fixture_map+0x230,image+0x16f04e0);
    memory.put(fixture_map+0x238,uintptr_t(0x24000000));memory.put(fixture_map+0x240,int32_t(1));memory.put(fixture_map+0x244,int32_t(1));memory.put(0x24000000,fixture_region);
    memory.put(fixture_map+0x358,uintptr_t(0));memory.put(fixture_map+0x360,int32_t(0));memory.put(fixture_map+0x364,int32_t(0));
    memory.put(fixture_region,image+0x1787930);memory.put(fixture_region+0x70,fixture_root);memory.put(fixture_region+0x834,int32_t(67));memory.put(fixture_region+0x950,buildings);memory.put(fixture_region+0x968,root_zones);
    memory.put(buildings,image+0x17183c0);memory.put(buildings+0x450,fixture_root);memory.put(buildings+0x458,fixture_region);
    memory.put(root_zones,image+0x16fe170);memory.put(root_zones+0xf0,fixture_region);memory.put(root_zones+0x48,image+0x172e1e0);memory.put(root_zones+0x50,uintptr_t(0));memory.put(root_zones+0x58,int32_t(0));memory.put(root_zones+0x5c,int32_t(0));
    observed.emplace(root_zones+0x48,q::Array{root_zones+0x48,{}});
    memory.put(scene_manager,image+0x16fe170);memory.put(scene_manager+0xf0,scene_region);memory.put(scene_region+0x70,scene_shard);
    memory.put(scene_manager+0x48,image+0x172e1e0);memory.put(scene_manager+0x50,scene_data);memory.put(scene_manager+0x58,int32_t(2));memory.put(scene_manager+0x5c,int32_t(2));
    for(size_t i=0;i<actors.size();++i){memory.put(scene_data+i*8,actors[i]);zone(actors[i],scene_shard,scene_region,uids[i]);}
    q::Array pool;std::string e;auto link=[](uintptr_t p,int32_t& id,std::string&){id=p==actors[0]?7:0;return true;};
    need(q::observe_array(memory,image,scene_manager+0x48,budget,link,pool,e),e);observed.emplace(pool.pointer,std::move(pool));
    ScenePool scene;scene.manager=scene_manager;scene.scene.exterior_region=67;scene.scene.exterior_slot=3;scene.scene.owner.key=sha+":world:building:17/template";
    scene.scene.owner.region=scene_region;scene.scene.owner.shard=scene_shard;scene.scene.owner.instance_index=-1;staged_scenes=std::vector<ScenePool>{scene};
}
void tests(){
    std::string e;setup();need(seal_world(fixture_world,false,e),e);need(pools.size()==1&&scene_pools.size()==1,"Complete root and Dungeon Quest pools were not retained");
    const auto& saved=scene_pools.at(scene_manager).saved;
    need(saved.array.zones[0].uid==uids[0]&&saved.array.zones[1].uid==uids[1],"Native complete UIDs/order were lost");
    need(q::zone_key(saved,saved.array.zones[0])==sha+":world:zone:7"&&q::zone_key(saved,saved.array.zones[1])==sha+":world:zone:"+snapshot_zone_key::scene_suffix(67,3,-1,1),"Original linked/inline scene destination keys differ");
    for(int mode=0;mode<12;++mode){setup();
        if(mode==0)staged_scenes.reset();if(mode==1)observed.erase(scene_manager+0x48);if(mode==2)staged_scene_world.generation++;
        if(mode==3)memory.put(scene_manager+0xf0,fixture_region);if(mode==4)memory.put(actors[0]+0x170,fixture_root);if(mode==5)memory.put(actors[0]+0x178,fixture_region);
        if(mode==6)uid_registry[uids[0]]=actors[1];if(mode==7)memory.put(actors[0]+0x360,int32_t(8));if(mode==8)memory.put(scene_data,actors[1]);
        if(mode==9)staged_scenes->push_back(staged_scenes->front());if(mode==10)uid_exception=0xc0000005;if(mode==11)observed.emplace(0x55000000,q::Array{0x55000000,{}});
        need(!seal_world(fixture_world,false,e)&&!e.empty(),"Invalid actual Dungeon Quest pool admitted mode="+std::to_string(mode));
        need(pools.empty()&&scene_pools.empty(),"Rejected Dungeon Quest seal published a partial destination map");
    }
    setup();memory.bytes.erase(actors[1]+0x560);need(!seal_world(fixture_world,false,e)&&e=="original Dungeon Quest Zone field read failure","Original native field cause was replaced");
    setup();staged_scenes->front().scene.owner.instance_index=9;need(seal_world(fixture_world,false,e),e);
    need(q::zone_key(scene_pools.at(scene_manager).saved,scene_pools.at(scene_manager).saved.array.zones[1]).find("/instance[9]/")!=std::string::npos,"Native saved Instance index not retained in Quest target key");
}
}
int main()try{fixture::tests();std::cout<<fixture::comparisons<<" actual root/Dungeon Quest pool, owner, native record and complete count comparisons passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

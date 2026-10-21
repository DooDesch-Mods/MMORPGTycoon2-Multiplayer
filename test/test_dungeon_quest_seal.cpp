#ifdef _WIN32
#include "live_host_state/memory_query_timing.h"
#endif
#include "npc_quest_targets.h"
#include "world_zone_serialization.h"
#include "world_zone_runtime.h"
#include "scene_clone/raw.h"
#include "npc_quest_target_runtime.h"
#include "dungeon_scene_identity_runtime.h"
#include <iostream>
#include <optional>
#include <mutex>
#include <stdexcept>
#define LOG_INFO(...) ((void)0)
namespace fixture {
namespace q=mt2mp::game::npc_quest_targets;
namespace action_session=mt2mp::action_session;
namespace scene_clone=mt2mp::game::scene_clone;
namespace world_zone_runtime=mt2mp::game::world_zone_runtime;
namespace world_zone_serialization=mt2mp::game::world_zone_serialization;
namespace dungeon_zone_roster=mt2mp::game::dungeon_zone_roster;
namespace dungeon_scene_roster=mt2mp::game::dungeon_scene_roster;
namespace region_saved_identity=mt2mp::game::region_saved_identity;
namespace snapshot_zone_key=mt2mp::game::snapshot_zone_key;
using World=mt2mp::game::dungeon_scene_identity_runtime::World;
namespace opened {using CurrentWorld=World;}
using Scene=mt2mp::game::dungeon_scene_roster::Scene;
struct ScenePool {Scene scene;uintptr_t manager=0;q::SavedPool saved;};
struct WorldZonePool {std::string scene;uintptr_t root=0,region=0,manager=0;std::map<uint64_t,world_zone_runtime::Current> rows;};
std::map<uintptr_t,world_zone_serialization::Pool> world_zone_observed;
std::map<uintptr_t,WorldZonePool> world_zone_pools;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,const T& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=p[i];}
    bool read(uintptr_t address,void* value,size_t size,std::string& e){auto* p=reinterpret_cast<unsigned char*>(value);
        for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){e="original Dungeon Quest Zone field read failure";return false;}p[i]=found->second;}return true;}
}memory;
constexpr uintptr_t image=0x10000000,fixture_map=0x20000000,fixture_root=fixture_map+0x1c0,fixture_region=0x21000000,root_zones=0x22000000,buildings=0x23000000;
constexpr uintptr_t scene_region=0x30000000,scene_shard=0x31000000,scene_manager=0x32000000,scene_data=0x33000000;
constexpr std::array<uintptr_t,2> actors{0x40000000,0x40004000};
constexpr std::array<uint64_t,2> fixture_uids{0x100000012,0x200000012};
constexpr q::Budget budget{4096,4096,1048576,4096};
const std::string uuid="11111111-1111-4111-8111-111111111111",sha(64,'a');
uintptr_t bound_context=9;std::map<int,int> saves,loads,load_stack;std::string failure;
std::map<uintptr_t,q::Array> observed;std::map<int32_t,q::SavedPool> pools;
std::optional<std::vector<ScenePool>> staged_scenes;World staged_scene_world,origin,fixture_world;
std::map<uintptr_t,ScenePool> scene_pools;std::map<uint64_t,uintptr_t> uid_registry;unsigned long uid_exception=0;
bool healthy(std::string& e){e=failure;return failure.empty();}
bool reject(std::string& e,const std::string& cause){failure=cause;e=cause;pools.clear();scene_pools.clear();return false;}
#include "quest_same_world_actual.inc"
unsigned long exact_uid(uint64_t uid,uintptr_t& found){found=uid_registry[uid];return uid_exception;}
#include "dungeon_quest_seal_actual.inc"
std::mutex gate;bool host_active=false,client_active=false;uint32_t main_thread=3;
uint32_t GetCurrentThreadId(){return 3;}
std::optional<World> scene_override,after_override;size_t world_reads=0;std::string active_snapshot=uuid;
namespace net {std::string current_snapshot_id(){return active_snapshot;}}
namespace native_open_runtime {bool read_current_world(World& out,std::string&){
    out=world_reads++&&after_override?*after_override:fixture_world;return true;}}
namespace dungeon_scene_identity_runtime {
using World=fixture::World;
using Census=mt2mp::game::dungeon_scene_identity_runtime::Census;
std::vector<Scene> current_scenes;
bool read_scene_roster(World& out,std::vector<Scene>& scenes,std::string&){out=scene_override?*scene_override:fixture_world;scenes=current_scenes;return true;}
}
#include "world_zone_current.inc"
#include "world_zone_clone.inc"
using DungeonCellCascade=mt2mp::game::npc_quest_target_runtime::DungeonCellCascade;
#include "world_zone_cells.inc"
size_t comparisons=0;
void need(bool okay,const std::string& why){++comparisons;if(!okay)throw std::runtime_error(why+" comparison="+std::to_string(comparisons));}
void zone(uintptr_t object,uintptr_t shard,uintptr_t owner,uint64_t uid){
    memory.put(object,image+0x1745ce0);memory.put(object+0x18,uid);memory.put(object+0x170,shard);memory.put(object+0x178,owner);
    memory.put(object+0x24c,int32_t(1));memory.put(object+0x250,int32_t(2));memory.put(object+0x360,int32_t(4));memory.put(object+0x560,int32_t(3));memory.put(object+0x56c,int32_t(12));
    memory.put(object+0x2f0,image+0x16e0140);memory.put(object+0x330,image+0x16eb080);memory.put(object+0x338,uintptr_t(0));memory.put(object+0x340,int32_t(0));memory.put(object+0x344,int32_t(0));uid_registry[uid]=object;
}
void setup(){
    scene_override.reset();after_override.reset();world_reads=0;active_snapshot=uuid;
    memory={};failure.clear();observed.clear();world_zone_observed.clear();world_zone_pools.clear();pools.clear();scene_pools.clear();uid_registry.clear();uid_exception=0;
    fixture_world={};fixture_world.uuid=uuid;fixture_world.sha256=sha;fixture_world.world_epoch=7;fixture_world.generation=9;fixture_world.map=reinterpret_cast<void*>(fixture_map);staged_scene_world=fixture_world;
    memory.put(fixture_map,image+0x1742860);memory.put(fixture_root,image+0x174ad20);memory.put(fixture_root+0x48,int32_t(0));memory.put(fixture_map+0x230,image+0x16f04e0);
    memory.put(fixture_map+0x238,uintptr_t(0x24000000));memory.put(fixture_map+0x240,int32_t(1));memory.put(fixture_map+0x244,int32_t(1));memory.put(0x24000000,fixture_region);
    memory.put(fixture_map+0x358,uintptr_t(0));memory.put(fixture_map+0x360,int32_t(0));memory.put(fixture_map+0x364,int32_t(0));
    memory.put(fixture_region,image+0x1787930);memory.put(fixture_region+0x70,fixture_root);memory.put(fixture_region+0x834,int32_t(67));memory.put(fixture_region+0x950,buildings);memory.put(fixture_region+0x968,root_zones);
    memory.put(buildings,image+0x17183c0);memory.put(buildings+0x450,fixture_root);memory.put(buildings+0x458,fixture_region);
    memory.put(root_zones,image+0x16fe170);memory.put(root_zones+0xf0,fixture_region);memory.put(root_zones+0x48,image+0x172e1e0);memory.put(root_zones+0x50,uintptr_t(0));memory.put(root_zones+0x58,int32_t(0));memory.put(root_zones+0x5c,int32_t(0));
    observed.emplace(root_zones+0x48,q::Array{root_zones+0x48,{}});
    memory.put(scene_manager,image+0x16fe170);memory.put(scene_manager+0xf0,scene_region);memory.put(scene_region+0x70,scene_shard);
    memory.put(scene_region+0x968,scene_manager);
    memory.put(scene_manager+0x48,image+0x172e1e0);memory.put(scene_manager+0x50,scene_data);memory.put(scene_manager+0x58,int32_t(2));memory.put(scene_manager+0x5c,int32_t(2));
    for(size_t i=0;i<actors.size();++i){memory.put(scene_data+i*8,actors[i]);zone(actors[i],scene_shard,scene_region,fixture_uids[i]);}
    q::Array pool;std::string e;auto link=[](uintptr_t p,int32_t& id,std::string&){id=p==actors[0]?7:0;return true;};
    need(q::observe_array(memory,image,scene_manager+0x48,budget,link,pool,e),e);observed.emplace(pool.pointer,std::move(pool));
    ScenePool scene;scene.manager=scene_manager;scene.scene.exterior_region=67;scene.scene.exterior_slot=3;scene.scene.owner.key=sha+":world:building:17/template";
    scene.scene.owner.region=scene_region;scene.scene.owner.shard=scene_shard;scene.scene.owner.instance_index=-1;staged_scenes=std::vector<ScenePool>{scene};
    dungeon_scene_identity_runtime::current_scenes={scene.scene};
    for(const auto manager:{root_zones,scene_manager})for(size_t type=0;type<4;++type)if(type!=1){const auto array=manager+0x18+type*0x30;
        memory.put(array,image+0x172e1e0);memory.put(array+8,uintptr_t(0));memory.put(array+0x10,int32_t(0));memory.put(array+0x14,int32_t(0));}
    for(const auto manager:{root_zones,scene_manager}){world_zone_serialization::Pool all;need(world_zone_serialization::read(memory,image,manager,link,all,e),e);world_zone_observed.emplace(manager,std::move(all));}

}
void extra_zone(int32_t type){
    const uintptr_t object=0x50000000+uintptr_t(type)*0x10000,slots=0x60000000+uintptr_t(type)*0x1000;
    zone(object,scene_shard,scene_region,0x900000000+uint64_t(type));memory.put(object+0x24c,type);
    const auto array=scene_manager+0x18+uintptr_t(type)*0x30;memory.put(array+8,slots);memory.put(array+0x10,int32_t(1));memory.put(array+0x14,int32_t(1));memory.put(slots,object);
    auto links=[](uintptr_t p,int32_t& id,std::string&){id=p==actors[0]?7:0;return true;};
    world_zone_serialization::Pool all;std::string error;need(world_zone_serialization::read(memory,image,scene_manager,links,all,error),error);world_zone_observed[scene_manager]=std::move(all);
}
void origin_binding_tests(){
    for(const bool host_capture:{true,false}){
        std::string e;setup();extra_zone(0);extra_zone(2);extra_zone(3);
        const uintptr_t root_actor=0x70000000,root_slots=0x71000000;const uint64_t root_uid=0xb00000001;
        zone(root_actor,fixture_root,fixture_region,root_uid);
        memory.put(root_zones+0x50,root_slots);memory.put(root_zones+0x58,int32_t(1));memory.put(root_zones+0x5c,int32_t(1));memory.put(root_slots,root_actor);
        auto links=[](uintptr_t p,int32_t& id,std::string&){id=p==root_actor?11:p==actors[0]?7:0;return true;};
        q::Array root_pool;need(q::observe_array(memory,image,root_zones+0x48,budget,links,root_pool,e),e);observed[root_pool.pointer]=std::move(root_pool);
        world_zone_serialization::Pool root_all;need(world_zone_serialization::read(memory,image,root_zones,links,root_all,e),e);world_zone_observed[root_zones]=std::move(root_all);
        if(host_capture){fixture_world.uuid="33333333-3333-4333-8333-333333333333";fixture_world.sha256=std::string(64,'b');}
        // The captured Scene census names the shared snapshot; the local origin remains the loaded file.
        need(seal_world(fixture_world,false,e),e);
        need(origin.uuid==fixture_world.uuid&&origin.sha256==fixture_world.sha256&&pools.at(67).origin.uuid==uuid&&
             pools.at(67).origin.sha256==sha,"Seal lost physical origin or logical shared pool identity");
        const action_session::Binding binding{fixture_world.world_epoch,fixture_world.generation};
        std::vector<world_zone_runtime::Current> canonical;
        world_reads=0;need(read_world_zones(binding,uuid,canonical,e)&&canonical.size()==6,
            "Shared capture/current roster admission failed: "+e);
        for(const auto& row:canonical){world_zone_runtime::Current selected;world_reads=0;
            need(resolve_world_zone(binding,uuid,row.object,row.uid,selected,e)&&selected==row,
                 "Shared capture/target admission failed: "+e);}
        auto reject_both=[&](const action_session::Binding& current,const std::string& snapshot){
            std::vector<world_zone_runtime::Current> rows=canonical;world_reads=0;
            need(!read_world_zones(current,snapshot,rows,e)&&rows.empty()&&!e.empty(),"Current roster admitted changed binding");
            for(const auto& row:canonical){world_zone_runtime::Current selected=row;world_reads=0;
                // Root targets do not ask for a Dungeon census.
                if(scene_override&&row.scene_key=="root")continue;
                need(!resolve_world_zone(current,snapshot,row.object,row.uid,selected,e)&&selected.key.empty()&&!e.empty(),
                     "Target admitted changed binding");}
        };
        auto stale=binding;++stale.native_generation;reject_both(stale,uuid);
        stale=binding;++stale.native_epoch;reject_both(stale,uuid);
        reject_both(binding,"44444444-4444-4444-8444-444444444444");
        active_snapshot="44444444-4444-4444-8444-444444444444";reject_both(binding,uuid);active_snapshot=uuid;
        const auto exact=fixture_world;
        for(int phase=0;phase<3;++phase)for(int field=0;field<9;++field){
            auto changed=exact;
            if(field==0)changed.uuid="55555555-5555-4555-8555-555555555555";
            if(field==1)changed.sha256=std::string(64,'c');
            if(field==2)++changed.generation;if(field==3)++changed.world_epoch;
            if(field==4)changed.game=reinterpret_cast<void*>(0x77700000);
            if(field==5)changed.mode=reinterpret_cast<void*>(0x77700010);
            if(field==6)changed.state=reinterpret_cast<void*>(0x77700020);
            if(field==7)changed.world=reinterpret_cast<void*>(0x77700030);
            if(field==8)changed.map=reinterpret_cast<void*>(0x77700040);
            if(phase==0)fixture_world=changed;if(phase==1)scene_override=changed;if(phase==2)after_override=changed;
            reject_both(binding,uuid);
            fixture_world=exact;scene_override.reset();after_override.reset();
        }
        std::vector<world_zone_runtime::Current> retained;world_reads=0;
        need(read_world_zones(binding,uuid,retained,e)&&retained==canonical,"Rejected origin changed canonical Zone aliases");
    }
}
void tests(){
    origin_binding_tests();
    std::string e;setup();need(seal_world(fixture_world,false,e),e);need(pools.size()==1&&scene_pools.size()==1,"Complete root and Dungeon Quest pools were not retained");
    const auto& saved=scene_pools.at(scene_manager).saved;
    need(saved.array.zones[0].uid==fixture_uids[0]&&saved.array.zones[1].uid==fixture_uids[1],"Native complete UIDs/order were lost");
    need(q::zone_key(saved,saved.array.zones[0])==sha+":world:zone:7"&&q::zone_key(saved,saved.array.zones[1])==sha+":world:zone:"+snapshot_zone_key::scene_suffix(67,3,-1,1),"Original linked/inline scene destination keys differ");
    for(int mode=0;mode<12;++mode){setup();
        if(mode==0)staged_scenes.reset();if(mode==1)observed.erase(scene_manager+0x48);if(mode==2)staged_scene_world.generation++;
        if(mode==3)memory.put(scene_manager+0xf0,fixture_region);if(mode==4)memory.put(actors[0]+0x170,fixture_root);if(mode==5)memory.put(actors[0]+0x178,fixture_region);
        if(mode==6)uid_registry[fixture_uids[0]]=actors[1];if(mode==7)memory.put(actors[0]+0x360,int32_t(8));if(mode==8)memory.put(scene_data,actors[1]);
        if(mode==9)staged_scenes->push_back(staged_scenes->front());if(mode==10)uid_exception=0xc0000005;if(mode==11)observed.emplace(0x55000000,q::Array{0x55000000,{}});
        need(!seal_world(fixture_world,false,e)&&!e.empty(),"Invalid actual Dungeon Quest pool admitted mode="+std::to_string(mode));
        need(pools.empty()&&scene_pools.empty(),"Rejected Dungeon Quest seal published a partial destination map");
    }
    setup();memory.bytes.erase(actors[1]+0x560);need(!seal_world(fixture_world,false,e)&&e=="original Dungeon Quest Zone field read failure","Original native field cause was replaced");
    setup();extra_zone(0);extra_zone(2);extra_zone(3);need(seal_world(fixture_world,false,e),e);
    need(world_zone_pools.at(scene_manager).rows.size()==3,"Non-Monster original saved identities omitted");
    for(const auto& [uid,row]:world_zone_pools.at(scene_manager).rows){need(uid==row.uid&&row.region==scene_region&&row.root==scene_shard&&row.scene_key==sha+":world:building:17/template","Non-Monster saved canonical scene ownership lost");}
    std::vector<world_zone_runtime::Current> current;action_session::Binding binding{fixture_world.world_epoch,fixture_world.generation};
    need(read_world_zones(binding,uuid,current,e)&&current.size()==5,e);
    for(const auto& row:current){world_zone_runtime::Current selected;need(resolve_world_zone(binding,uuid,row.object,row.uid,selected,e)&&selected==row,e);
        need(!resolve_world_zone(binding,uuid,row.object,row.uid^0x100000000,selected,e)&&selected.key.empty(),"Targeted Zone resolver accepted partial UID");}
    DungeonCellCascade before_cells;need(prepare_world_zone_cells(before_cells,e),e);
    need(before_cells.other_before.at(scene_manager).size()==3,"Dungeon Close before receipt omitted non-Monster families");
    auto native_at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    auto native_find=[&](uint64_t uid,uintptr_t& value,std::string&){exact_uid(uid,value);return true;};
    dungeon_zone_roster::Census before_native;need(dungeon_zone_roster::read(native_at,native_find,image,scene_manager,scene_shard,scene_region,before_native,e),e);
    const auto before_pool=world_zone_pools.at(scene_manager);auto cut=before_native;
    cut.members.erase(std::remove_if(cut.members.begin(),cut.members.end(),[](const auto& row){return row.type==2;}),cut.members.end());
    WorldZonePool cut_pool;need(!reconcile_world_zone_cells(before_pool,cut,nullptr,nullptr,cut_pool,e),"Close accepted a Zone still registered by full UID");
    const auto restored_uid=uint64_t(0x900000002);const auto restored_object=uid_registry.at(restored_uid);uid_registry.erase(restored_uid);
    need(reconcile_world_zone_cells(before_pool,cut,nullptr,nullptr,cut_pool,e)&&cut_pool.rows.size()==2,e);
    auto inverse=before_native;for(auto& row:inverse.members)if(row.uid==restored_uid)row.object+=0x100000;
    uid_registry[restored_uid]=restored_object+0x100000;std::set<uint64_t> restored{restored_uid};WorldZonePool rebuilt;
    need(!reconcile_world_zone_cells(cut_pool,inverse,nullptr,nullptr,rebuilt,e),"Unconfirmed new non-Monster member admitted");
    need(reconcile_world_zone_cells(cut_pool,inverse,&before_pool.rows,&restored,rebuilt,e),e);
    need(rebuilt.rows.at(restored_uid).key==before_pool.rows.at(restored_uid).key&&rebuilt.rows.at(restored_uid).object==restored_object+0x100000,"Original Make full UID did not restore canonical key to actual new allocation");
    need(!reconcile_world_zone_cells(cut_pool,cut,&before_pool.rows,&restored,rebuilt,e),"Original Make receipt accepted without actual restored Zone");
    auto wrong_type=inverse;for(auto& row:wrong_type.members)if(row.uid==restored_uid)row.type=3;
    need(!reconcile_world_zone_cells(cut_pool,wrong_type,&before_pool.rows,&restored,rebuilt,e),"Original Make restored a different Zone type");
    uid_registry[restored_uid]=restored_object;
    const auto canonical=current;const auto canonical_aliases=scene_pools.at(scene_manager).saved.action_keys;
    scene_pools.at(scene_manager).saved.action_keys[actors[0]]="action:22222222-2222-4222-8222-222222222222";
    need(read_world_zones(binding,uuid,current,e),e);need(std::any_of(current.begin(),current.end(),[](const auto& row){return row.object==actors[0]&&row.key=="action:22222222-2222-4222-8222-222222222222";}),"Current reader replaced confirmed Monster action alias");
    scene_pools.at(scene_manager).saved.action_keys=canonical_aliases;
    for(int mode=0;mode<7;++mode){const auto retained=world_zone_pools;const auto saved_scene=scene_pools;const auto saved_world=fixture_world;auto stale=binding;
        if(mode==0)++stale.native_generation;if(mode==1)world_zone_pools.at(scene_manager).rows.erase(0x900000002);
        if(mode==2)world_zone_pools.at(scene_manager).rows.at(0x900000002).object+=8;if(mode==3)world_zone_pools.at(scene_manager).rows.at(0x900000002).type=3;
        if(mode==4)scene_pools.at(scene_manager).saved.array.zones.pop_back();if(mode==5)world_zone_pools.at(scene_manager).scene="foreign";if(mode==6)++fixture_world.generation;
        need(!read_world_zones(stale,uuid,current,e)&&current.empty()&&!e.empty(),"Current all-type Zone identity guard weakened mode="+std::to_string(mode));
        world_zone_pools=retained;scene_pools=saved_scene;fixture_world=saved_world;
    }
    need(read_world_zones(binding,uuid,current,e)&&current==canonical,"Read-only failed admission changed current aliases");
    scene_clone::Raw raw;raw.world=fixture_world;scene_clone::Pool existing;existing.scene=scene_pools.at(scene_manager).scene;existing.scene.owner.type=1;existing.managers[1]=scene_manager;
    scene_clone::Mapping mapping;mapping.operation="22222222-2222-4222-8222-222222222222";
    for(const auto& [uid,row]:world_zone_pools.at(scene_manager).rows){const auto family=row.type==0?scene_clone::Family::NoneZone:row.type==2?scene_clone::Family::PuzzleZone:scene_clone::Family::StoryZone;
        existing.members.push_back({family,0,row.object,uid,row.key});mapping.members.push_back({row.scene_key,row.key,family,0,0,{}});}
    raw.pools.push_back(existing);std::map<uintptr_t,WorldZonePool> preview;
    need(preview_world_zones(raw,mapping,preview,e,{},nullptr,nullptr)&&clone_world_same(preview,world_zone_pools),e);
    auto instance=existing;instance.scene.owner.type=2;instance.scene.owner.key=sha+":world:building:17/instance/3";instance.scene.owner.region+=0x8000000;instance.scene.owner.shard+=0x8000000;instance.managers[1]+=0x8000000;
    for(auto& member:instance.members){member.object+=0x8000000;member.uid+=0x100000000;member.key.clear();const auto key="action:33333333-3333-4333-8333-33333333333"+std::to_string(uint8_t(member.family));
        mapping.members.push_back({instance.scene.owner.key,key,member.family,0,uint32_t(member.family),mapping.operation});}
    raw.pools.push_back(instance);need(preview_world_zones(raw,mapping,preview,e,{},nullptr,nullptr),e);
    need(preview.size()==world_zone_pools.size()+1&&preview.at(instance.managers[1]).rows.size()==3,"Actual non-Monster clone preview omitted complete family batch");
    need(!world_zone_pools.contains(instance.managers[1]),"Clone preview published aliases before original final readback");
    auto invalid_mapping=mapping;invalid_mapping.members.pop_back();need(!preview_world_zones(raw,invalid_mapping,preview,e,{},nullptr,nullptr),"Partial non-Monster ordered birth mapping accepted");
    need(preview_world_zones(raw,mapping,preview,e,{},nullptr,nullptr),e);world_zone_pools=preview;
    ScenePool born;born.scene=instance.scene;born.manager=instance.managers[1];scene_pools.emplace(born.manager,born);
    std::string alias;need(clone_world_alias(instance,instance.members.back(),alias,e)&&!alias.empty(),e);
    auto partial_uid=instance.members.back();partial_uid.uid^=0x100000000;need(!clone_world_alias(instance,partial_uid,alias,e),"Partial clone UID alias admitted");
    for(const auto& row:mapping.members)if(row.scene==instance.scene.owner.key)mapping.retired.push_back(row);
    mapping.members.erase(std::remove_if(mapping.members.begin(),mapping.members.end(),[&](const auto& row){return row.scene==instance.scene.owner.key;}),mapping.members.end());raw.pools.pop_back();
    need(preview_world_zones(raw,mapping,preview,e,{},nullptr,nullptr)&&!preview.contains(instance.managers[1]),e);
    uid_registry[instance.members.back().uid]=instance.members.back().object;need(!preview_world_zones(raw,mapping,preview,e,{},nullptr,nullptr),"Partially completed non-Monster native deletion admitted");
    uid_registry.erase(instance.members.back().uid);mapping.retired.pop_back();need(!preview_world_zones(raw,mapping,preview,e,{},nullptr,nullptr),"Missing non-Monster retirement mapping admitted");

    for(int mode=0;mode<6;++mode){setup();extra_zone(2);
        if(mode==0)world_zone_observed.erase(scene_manager);if(mode==1)world_zone_observed.at(scene_manager).arrays[2][0].uid^=0x100000000;
        if(mode==2)world_zone_observed.at(scene_manager).arrays[2].clear();if(mode==3)memory.put(0x50020000+0x170,fixture_root);
        if(mode==4)uid_registry[0x900000002]=actors[0];if(mode==5)world_zone_observed.emplace(99,world_zone_serialization::Pool{});
        need(!seal_world(fixture_world,false,e)&&!e.empty()&&world_zone_pools.empty(),"Unproven all-type Zone identity published mode="+std::to_string(mode));}
    setup();staged_scenes->front().scene.owner.instance_index=9;need(seal_world(fixture_world,false,e),e);
    need(q::zone_key(scene_pools.at(scene_manager).saved,scene_pools.at(scene_manager).saved.array.zones[1]).find("/instance[9]/")!=std::string::npos,"Native saved Instance index not retained in Quest target key");
}
}
int main()try{fixture::tests();std::cout<<fixture::comparisons<<" actual root/Dungeon Quest pool, owner, native record and complete count comparisons passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

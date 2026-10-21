#include "raw.h"
#include "../dungeon_held_template.h"
#include "../dungeon_template_owner.h"
#include "../object_identity/remove_runtime/copy/world_prop_reader.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
namespace mt2mp::game::scene_clone {
namespace {
namespace native_host=mt2mp::build_actions::private_native::host;
constexpr const char* executable="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
uint32_t call_find(native_host::FindUid method,uint64_t uid,uintptr_t& out)noexcept{__try{out=uintptr_t(method(uid));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool lookup(native_host::ProcessMemory& memory,uintptr_t& image,native_host::LookupApi& api,std::string& e){uint32_t thread{};std::string sha;
    return native::read_resolver_receipt(image,thread,sha,e)&&((thread==GetCurrentThreadId()&&sha==executable)||fail(e,"Scene clone raw reader requires exact executable/Mainthread"))&&native_host::resolve_lookup(memory,image,sha,api,e);}
}
bool find_uid(uint64_t uid,uintptr_t& out,std::string& e){out=0;native_host::ProcessMemory memory;native_host::LookupApi api;uintptr_t image{};if(!uid||!lookup(memory,image,api,e))return false;const auto exception=call_find(api.find,uid,out);return !exception||fail(e,"Scene clone original FindUID SEH="+std::to_string(exception));}
namespace {
bool read_selected(Raw& next,const std::vector<dungeon_scene_roster::Scene>& scenes,std::string& e){
    native_host::ProcessMemory memory;native_host::LookupApi api;uintptr_t image{};if(!lookup(memory,image,api,e))return false;
    const auto at=[&](uintptr_t pointer,size_t offset,auto& value,std::string& cause){if(!pointer||pointer>UINTPTR_MAX-offset||pointer+offset>UINTPTR_MAX-sizeof(value))return fail(cause,"Scene clone native field span invalid");return memory.copy(pointer+offset,&value,sizeof value,cause);};
    struct Reader{native_host::ProcessMemory& memory;bool read(uintptr_t p,void* out,size_t bytes,std::string& e){return memory.copy(p,out,bytes,e);}} reader{memory};
    const auto find=[&](uint64_t uid,uintptr_t& result,std::string& cause){const auto exception=call_find(api.find,uid,result);return !exception||fail(cause,"Scene clone original FindUID SEH="+std::to_string(exception));};
    const auto no_link=[](uintptr_t,int32_t& id,std::string&){id=0;return true;};
    std::set<uintptr_t> managers,objects;std::set<uint64_t> uids;
    for(const auto& scene:scenes){Pool pool;pool.scene=scene;
        for(size_t i=0;i<4;++i){uintptr_t ap{},region{},shard{};if(!at(scene.owner.region,dungeon_template_owner::region_offsets[i],pool.managers[i],e)||!at(pool.managers[i],0,ap,e)||!at(pool.managers[i],dungeon_template_owner::reverse_region[i],region,e))return false;
            if(ap!=image+dungeon_template_owner::addresspoints[i]||region!=scene.owner.region||!managers.insert(pool.managers[i]).second)return fail(e,"Scene clone manager class/reciprocal Region differs");
            if(i==0||i==3){if(!at(pool.managers[i],i==0?0x908:0x68,shard,e))return false;if(shard!=scene.owner.shard)return fail(e,"Scene clone manager reciprocal Shard differs");}}
        dungeon_scene_roster::Array gizmos;if(!dungeon_scene_roster::array(at,pool.managers[3]+0x18,gizmos,e))return false;
        if(gizmos.members.size()>4096)return fail(e,"Scene clone raw Gizmo budget exceeded");
        for(size_t n=0;n<gizmos.members.size();++n){Member value;value.family=Family::Gizmo;value.child=uint32_t(n);value.object=gizmos.members[n];uintptr_t found{};
            if(!at(value.object,0x18,value.uid,e)||!value.uid||!find(value.uid,found,e)||found!=value.object||!objects.insert(value.object).second||!uids.insert(value.uid).second)return fail(e,e.empty()?"Scene clone raw Gizmo full UID duplicated or replaced":e);pool.members.push_back(value);}
        if(!dungeon_scene_roster::unchanged(at,pool.managers[3]+0x18,gizmos,e))return false;
        dungeon_zone_roster::Census zones;if(!dungeon_zone_roster::read(at,find,image,pool.managers[1],scene.owner.shard,scene.owner.region,zones,e)||!npc_quest_targets::observe_array(reader,image,pool.managers[1]+0x48,{4096,4096,1048576,4096},no_link,pool.zones,e))return false;
        for(size_t n=0;n<pool.zones.zones.size();++n){const auto& zone=pool.zones.zones[n];if(!objects.insert(zone.pointer).second||!uids.insert(zone.uid).second)return fail(e,"Scene clone MonsterZone full UID/pointer duplicated");pool.members.push_back({Family::MonsterZone,uint32_t(n),zone.pointer,zone.uid,{}});}
        if(!monster_saved_identity::observe(reader,image,pool.managers[2],no_link,pool.monsters,e))return false;
        uint32_t direct_child=0;
        for(const auto& monster:pool.monsters.records){uintptr_t region{},shard{},zone{},proxy{},found{};
            if(!at(monster.pointer,0x178,region,e)||!at(monster.pointer,0x170,shard,e)||!at(monster.pointer,0x8d0,zone,e)||!at(monster.pointer,0x8d8,proxy,e)||!find(monster.uid,found,e))return false;
            if(region!=scene.owner.region||shard!=scene.owner.shard||found!=monster.pointer)return fail(e,"Scene clone Monster full UID/scene owner differs");
            if(zone)continue;if(proxy)return fail(e,"Scene clone direct Monster has a Zone proxy");
            if(!objects.insert(monster.pointer).second||!uids.insert(monster.uid).second)return fail(e,"Scene clone direct Monster full UID/pointer duplicated");
            pool.members.push_back({Family::DirectMonster,direct_child++,monster.pointer,monster.uid,{}});}
        for(const auto type:{0,2,3}){uint32_t child=0;const auto family=type==0?Family::NoneZone:type==2?Family::PuzzleZone:Family::StoryZone;
            for(const auto& zone:zones.members)if(zone.type==type){if(!objects.insert(zone.object).second||!uids.insert(zone.uid).second)return fail(e,"Scene clone non-Monster Zone full UID/pointer duplicated");
                pool.members.push_back({family,child++,zone.object,zone.uid,{}});}}
        uintptr_t building_ap=0,building_region=0,building_shard=0;
        if(!at(scene.owner.region,0x950,pool.building_manager,e)||!at(pool.building_manager,0,building_ap,e)||
           !at(pool.building_manager,0x450,building_shard,e)||!at(pool.building_manager,0x458,building_region,e))return false;
        if(building_ap!=image+0x17183c0||building_shard!=scene.owner.shard||building_region!=scene.owner.region||
           !managers.insert(pool.building_manager).second)return fail(e,"Scene clone Building manager reciprocal ownership differs");
        if(!at(pool.building_manager,0x408,building_ap,e)||building_ap!=image+0x172d7c0)return fail(e,e.empty()?"Scene clone Building array class differs":e);
        dungeon_scene_roster::Array buildings;if(!dungeon_scene_roster::array(at,pool.building_manager+0x408,buildings,e))return false;
        if(buildings.members.size()>4096)return fail(e,"Scene clone raw Building budget exceeded");
        for(size_t n=0;n<buildings.members.size();++n){Member value;value.family=Family::Building;value.child=uint32_t(n);value.object=buildings.members[n];
            uintptr_t found=0,owner_region=0,owner_shard=0;
            if(!at(value.object,0,building_ap,e)||!at(value.object,0x18,value.uid,e)||!at(value.object,0x170,owner_shard,e)||
               !at(value.object,0x178,owner_region,e)||!value.uid||!find(value.uid,found,e))return false;
            if(building_ap!=image+0x16d93c0||owner_shard!=scene.owner.shard||owner_region!=scene.owner.region||found!=value.object||
               !objects.insert(value.object).second||!uids.insert(value.uid).second)return fail(e,"Scene clone Building class/full UID/owner differs or duplicates");
            pool.members.push_back(std::move(value));
        }
        if(!dungeon_scene_roster::unchanged(at,pool.building_manager+0x408,buildings,e))return false;
        if(!dungeon_scene_identity_runtime::read_scenery_pool(next.world,scene,pool.managers,pool.scenery,e))return false;
        if(pool.scenery.context.manager!=pool.managers[0]||pool.scenery.context.region!=scene.owner.region||pool.scenery.context.shard!=scene.owner.shard)
            return fail(e,"Scene clone Scenery witness reciprocal owner differs");
        if(pool.scenery.members.size()>4096)return fail(e,"Scene clone raw Scenery budget exceeded");
        for(size_t n=0;n<pool.scenery.members.size();++n){const auto& row=pool.scenery.members[n];uintptr_t addresspoint=0;
            if(!at(row.object,0,addresspoint,e))return false;
            if(addresspoint!=image+0x16c1390||!row.uid||!objects.insert(row.object).second||!uids.insert(row.uid).second)
                return fail(e,"Scene clone Scenery class/full UID duplicated or differs");
            pool.members.push_back({Family::Scenery,uint32_t(n),row.object,row.uid,{}});
        }
        next.pools.push_back(std::move(pool));
    }
    return true;
}
}
bool read_raw(Raw& out,std::string& e){
    out={};Raw next;std::vector<dungeon_scene_roster::Scene> scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(next.world,scenes,e)||!read_selected(next,scenes,e))return false;
    dungeon_scene_identity_runtime::World final_world;std::vector<dungeon_scene_roster::Scene> final_scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(final_world,final_scenes,e)||!same_world(final_world,next.world)||final_scenes.size()!=next.pools.size())return fail(e,e.empty()?"Scene clone raw roster changed while reading":e);
    for(size_t n=0;n<final_scenes.size();++n){Pool final;final.scene=final_scenes[n];final.managers=next.pools[n].managers;final.building_manager=next.pools[n].building_manager;if(!same_owner(final,next.pools[n]))return fail(e,"Scene clone raw scene identity/order changed while reading");}
    out=std::move(next);e.clear();return true;
}
bool read_held_template(const Raw& before,const std::string& exterior,uintptr_t node,Raw& out,std::string& e){
    out={};const Pool* prior=nullptr;
    for(const auto& pool:before.pools)if(pool.scene.owner.exterior_key==exterior&&pool.scene.owner.type==1){
        if(prior)return fail(e,"Held Dungeon template baseline duplicated");prior=&pool;
    }
    if(!prior)return fail(e,"Held Dungeon template has no confirmed before scene");
    native_host::ProcessMemory memory;native_host::LookupApi api;uintptr_t image=0;
    if(!lookup(memory,image,api,e))return false;
    const auto at=[&](uintptr_t pointer,size_t offset,auto& value,std::string& cause){
        if(!pointer||pointer>UINTPTR_MAX-offset||pointer+offset>UINTPTR_MAX-sizeof(value))
            return fail(cause,"Held Dungeon template field span invalid");
        return memory.copy(pointer+offset,&value,sizeof value,cause);
    };
    const auto find=[&](uint64_t uid,uintptr_t& result,std::string& cause){
        const auto exception=call_find(api.find,uid,result);
        return !exception||fail(cause,"Held Dungeon template FindUID SEH="+std::to_string(exception));
    };
    std::vector<dungeon_scene_roster::Scene> scenes;
    std::vector<const Pool*> original;
    for(const auto& pool:before.pools)if(pool.scene.owner.exterior_key==exterior){
        scenes.push_back(pool.scene);original.push_back(&pool);
    }
    const auto owner=[&](){return dungeon_held_template::read(at,find,image,uintptr_t(before.world.map)+0x1c0,node,prior->scene,scenes,e);};
    if(!owner())return false;
    Raw next;next.world=before.world;
    if(!read_selected(next,scenes,e)||next.pools.size()!=original.size())
        return fail(e,e.empty()?"Held Dungeon template complete manager/member roster differs":e);
    for(size_t p=0;p<original.size();++p){
        const auto& old_pool=*original[p];auto& now_pool=next.pools[p];
        if(!same_owner(old_pool,now_pool)||old_pool.members.size()!=now_pool.members.size())
            return fail(e,"Held Dungeon complete scene manager/member roster differs");
        for(size_t i=0;i<old_pool.members.size();++i){
            const auto& old=old_pool.members[i];auto& now=now_pool.members[i];
            if(old.family!=now.family||old.child!=now.child||old.object!=now.object||old.uid!=now.uid)
                return fail(e,"Held Dungeon scene changed an original ordered member full UID");
            now.key=old.key;now.origin=old.origin;now.ordinal=old.ordinal;
        }
    }
    if(!owner())return false;
    out=std::move(next);e.clear();return true;
}
}

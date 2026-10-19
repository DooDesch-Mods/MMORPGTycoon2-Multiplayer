#include "intervention_census.h"
#include "intervention_monster_roster.h"
#include "direct_monster_identity.h"
#include "native_open_runtime.h"
#include "root_saved_building.h"
#include "npc_probe.h"
#include "remove_diagnostics.h"
#include "npc_quest_target_runtime.h"
#include "character_actor_party_owners.h"
#include <Windows.h>
#include <algorithm>

namespace mt2mp::game::interventions {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Memory {
    bool read(uintptr_t object,void* out,size_t size,std::string& error){return root_saved_building::bytes(reinterpret_cast<void*>(object),0,out,size,error);}
    bool copy(uintptr_t object,void* out,size_t size,std::string& error){return read(object,out,size,error);}
};
template<class T>bool at(Memory& memory,uintptr_t object,size_t offset,T& out,std::string& error){return npc_quest_targets::at(memory,object,offset,out,error);}
uint32_t lookup(uintptr_t image,uint64_t uid,uintptr_t& found)noexcept{__try{found=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool add(Census& out,uintptr_t object,const std::string& key,std::string& error){if(!object||key.empty())return fail(error,"Intervention canonical external owner is empty");const auto [found,inserted]=out.owners.emplace(object,key);return inserted||found->second==key||fail(error,"Intervention native external owner aliases two canonical keys");}
bool same(const building_mixed_identity::opened_file::CurrentWorld& a,const building_mixed_identity::opened_file::CurrentWorld& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.uuid==b.uuid&&a.sha256==b.sha256;}
bool subscribers(Memory& memory,uintptr_t image,Census& out,std::map<uintptr_t,int32_t>& toons,std::string& error){
    uintptr_t manager=0,ap=0,data=0;int32_t count=-1,capacity=-1;
    if(!at(memory,image,0x13df690,manager,error)||!at(memory,manager,0,ap,error)||ap!=image+0x1725f40||!at(memory,manager,0x40,data,error)||!at(memory,manager,0x48,count,error)||!at(memory,manager,0x4c,capacity,error))return fail(error,error.empty()?"Intervention Subscriber manager class/current header differs":error);
    if(count<0||count>1000000||capacity<count||(count&&(!data||data>UINTPTR_MAX-size_t(count)*8)))return fail(error,"Intervention Subscriber complete current array extent invalid");
    if(!add(out,manager,"subscriber-manager",error))return false;std::vector<uintptr_t> rows(size_t(count),uintptr_t{});
    if(count&&!memory.read(data,rows.data(),rows.size()*8,error))return false;std::set<uintptr_t> pointers;std::set<int32_t> ids;
    for(const auto subscriber:rows){int32_t id=-1;uintptr_t toon=0,brain=0,reverse=0;
        if(!at(memory,subscriber,0,ap,error)||ap!=image+0x16e75f0||!at(memory,subscriber,0x20,id,error)||id<0||!pointers.insert(subscriber).second||!ids.insert(id).second||!at(memory,subscriber,0x80,toon,error)||!at(memory,subscriber,0x88,brain,error))return fail(error,error.empty()?"Intervention Subscriber actual class/ID is foreign or duplicated":error);
        if(!add(out,subscriber,"subscriber:"+std::to_string(id),error))return false;
        if(toon){uint64_t uid=0;uintptr_t found=0;if(!at(memory,toon,0,ap,error)||ap!=image+0x1744e90||!at(memory,toon,0x18,uid,error)||!uid||!at(memory,toon,0x940,reverse,error)||reverse!=subscriber)return fail(error,error.empty()?"Intervention Toon reciprocal Subscriber/class/full UID differs":error);
            const auto exception=lookup(image,uid,found);if(exception||found!=toon)return fail(error,"Intervention Toon original full UID lookup differs SEH="+std::to_string(exception));
            if(!toons.emplace(toon,id).second||!add(out,toon,"toon:"+std::to_string(id),error))return fail(error,error.empty()?"Intervention Subscriber repeats its actual Toon":error);}
        if(brain){if(!at(memory,brain,0,ap,error)||ap!=image+0x16fdd60||!at(memory,brain,0x28,reverse,error)||reverse!=subscriber||!add(out,brain,"subscriber:"+std::to_string(id)+"/brain",error))return fail(error,error.empty()?"Intervention Brain reciprocal Subscriber/class differs":error);}}
    uintptr_t checked_manager=0,checked_data=0;int32_t checked_count=-1,checked_capacity=-1;
    if(!at(memory,image,0x13df690,checked_manager,error)||!at(memory,manager,0x40,checked_data,error)||!at(memory,manager,0x48,checked_count,error)||!at(memory,manager,0x4c,checked_capacity,error))return false;
    return (checked_manager==manager&&checked_data==data&&checked_count==count&&checked_capacity==capacity)||fail(error,"Intervention Subscriber complete current manager changed during census");
}
}
bool census(const action_session::Context& context,const action_session::Binding& binding,Census& out,std::string& error){
    live_host_state::apply_timing::PageReadScope census_pages(!live_host_state::apply_timing::PageReadScope::reader_active());
    out={};Census next;if(!boss_current::read_with_characters(context,binding,next.bosses,next.definitions,next.users,error))return false;
    const auto& world=next.bosses.world;const auto image=uintptr_t(GetModuleHandleW(nullptr)),map=uintptr_t(world.map),root=map+0x1c0;Memory memory;
    if(!add(next,root,"root_shard",error)||!add(next,map,"world-map",error)||!add(next,uintptr_t(world.state),"native-state",error)||!add(next,next.definitions.manager,"character-manager",error))return false;
    for(const auto& definition:next.definitions.definitions){if(!add(next,definition.object,definition.key,error))return false;
        if(definition.variants&&!add(next,definition.variants,"character-variants:"+definition.key,error))return false;
        if(!definition.deleted&&definition.owning_collection&&!next.owners.contains(definition.owning_collection)&&!add(next,definition.owning_collection,"character-collection:"+definition.key,error))return false;}
    for(const auto& definition:next.definitions.definitions)if(!definition.deleted){uintptr_t data=0;int32_t count=-1,capacity=-1;
        if(!at(memory,definition.object,0x130,data,error)||!at(memory,definition.object,0x138,count,error)||!at(memory,definition.object,0x13c,capacity,error))return false;
        if(count<0||count>10000||capacity<count||(count&&(!data||data>UINTPTR_MAX-size_t(count)*8)))return fail(error,"Intervention complete current Ability prototype array outside native extent");
        for(int32_t slot=0;slot<count;++slot){uintptr_t prototype=0,ap=0,found=0;uint64_t uid=0;
            if(!at(memory,data,size_t(slot)*8,prototype,error)||!at(memory,prototype,0,ap,error)||ap!=image+0x171c930||!at(memory,prototype,0x18,uid,error)||!uid)return fail(error,error.empty()?"Intervention current Ability prototype class/full UID differs":error);
            const auto exception=lookup(image,uid,found);if(exception||found!=prototype)return fail(error,"Intervention current Ability prototype original full UID lookup differs SEH="+std::to_string(exception));
            if(!add(next,prototype,definition.key+"/ability/"+std::to_string(slot),error))return false;}
        uintptr_t current_data=0;int32_t current_count=-1,current_capacity=-1;
        if(!at(memory,definition.object,0x130,current_data,error)||!at(memory,definition.object,0x138,current_count,error)||!at(memory,definition.object,0x13c,current_capacity,error)||current_data!=data||current_count!=count||current_capacity!=capacity)return fail(error,error.empty()?"Intervention current Ability prototype array changed during census":error);}
    std::vector<party_saved_identity_runtime::Current> parties;
    if(!party_saved_identity_runtime::read_current(parties,error)||!character_actor_party_owners::append({world.uuid,binding.native_epoch,binding.native_generation,root},parties,next.owners,error))return false;
    dungeon_scene_identity_runtime::Census scenes;if(!dungeon_scene_identity_runtime::read_current(scenes,error))return false;
    if(scenes.world.world!=world.world||scenes.world.map!=world.map||scenes.world.uuid!=world.uuid||scenes.world.generation!=world.generation||scenes.world.world_epoch!=world.world_epoch)return fail(error,"Intervention complete external Dungeon scenes differ from actual NativeOpen");
    for(const auto& pool:scenes.pools){const auto& scene=pool.scene.owner;
        if(!add(next,scene.exterior,scene.exterior_key,error)||!add(next,scene.dungeon,scene.exterior_key+"/dungeon",error)||!add(next,scene.shard,scene.key+"/shard",error)||!add(next,scene.region,scene.key+"/region",error))return false;
        if(scene.instance&&!add(next,scene.instance,scene.key+"/instance-object",error))return false;
        for(size_t n=0;n<pool.managers.size();++n)if(!add(next,pool.managers[n],scene.key+"/manager/"+std::to_string(n),error))return false;
        for(const auto& gizmo:pool.local.members){std::string key;if(!dungeon_scene_identity_runtime::resolve(gizmo.object,gizmo.uid,key,error)||!add(next,gizmo.object,key,error))return false;}}
    nlohmann::json props;if(!([&]{live_host_state::apply_timing::PageReadScope original_read(live_host_state::apply_timing::PageBoundary::OriginalCall);return remove_diagnostics::identity_bindings(props,error);}()))return false;
    if(props.at("world_epoch").get<uint64_t>()!=binding.native_epoch||props.at("world_generation").get<uint64_t>()!=binding.native_generation||
       world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation)
        return fail(error,"Intervention current external prop native lifetime differs: session_snapshot="+context.snapshot_uuid+" native_snapshot="+world.uuid+
            " prop_epoch="+props.at("world_epoch").dump()+" native_epoch="+std::to_string(world.world_epoch)+" binding_epoch="+std::to_string(binding.native_epoch)+
            " prop_generation="+props.at("world_generation").dump()+" native_generation="+std::to_string(world.generation)+" binding_generation="+std::to_string(binding.native_generation));
    for(const auto& prop:props.at("bindings")){if(prop.at("revoked").get<bool>())continue;if(prop.at("unknown").get<bool>()||prop.at("snapshot_id")!=context.snapshot_uuid)
        return fail(error,"Intervention current external prop alias is unknown or foreign: session_snapshot="+context.snapshot_uuid+" native_snapshot="+world.uuid+
            " alias_snapshot="+prop.at("snapshot_id").get<std::string>()+" unknown="+prop.at("unknown").dump()+" local_uid="+prop.at("local_uid").dump());
        uintptr_t found=0;const auto exception=lookup(image,prop.at("local_uid").get<uint64_t>(),found);if(exception||!found)return fail(error,"Intervention current external prop full UID lookup differs SEH="+std::to_string(exception));
        if(!add(next,found,prop.at("record_key").get<std::string>(),error))return false;}
    std::vector<npc_probe::action_runtime::StateOwner> npcs;if(!npc_probe::action_runtime::state_owners(context,binding,npcs,error))return false;
    std::map<uintptr_t,std::string> npc_keys;
    for(const auto& npc:npcs){if(!npc_keys.emplace(npc.npc,npc.key).second||!add(next,npc.npc,npc.key,error))return fail(error,error.empty()?"Intervention current NPC owner duplicated":error);
        for(const auto& [sequence,quest]:npc.quests)if(!add(next,quest,npc.key+"/quest/"+std::to_string(sequence),error))return false;}
    std::map<uintptr_t,int32_t> toon_ids;if(!subscribers(memory,image,next,toon_ids,error))return false;
    std::map<uintptr_t,std::string> monster_keys;std::set<uint64_t> monster_uids;std::set<uintptr_t> zone_pointers;std::set<std::string> zone_keys;
    for(const auto& pool:next.bosses.pools){const auto& scope=pool.scope;
        if(scope.scene=="world"){int32_t id=-1;if(!at(memory,scope.region,0x834,id,error)||id<0||!add(next,scope.region,"region:"+std::to_string(id),error))return fail(error,error.empty()?"Intervention Root Region external ID invalid":error);}
        monster_zone_read::State monsters;if(!([&]{live_host_state::apply_timing::PageReadScope monster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return monster_roster(memory,image,map,scope,monsters,error);}()))return false;
        std::map<uintptr_t,std::string> local_zones;
        for(const auto& zone:monsters.zones){std::string key;bool dungeon=false;
            if(!([&]{live_host_state::apply_timing::PageReadScope original_read(live_host_state::apply_timing::PageBoundary::OriginalCall);return npc_quest_target_runtime::bind_current_destination(zone.value.pointer,key,dungeon,error);}())||!zone_pointers.insert(zone.value.pointer).second||!zone_keys.insert(key).second||!add(next,zone.value.pointer,key,error))return fail(error,error.empty()?"Intervention Monster Zone canonical current owner duplicated":error);
            if(dungeon!=(scope.scene!="world"))return fail(error,"Intervention Monster Zone canonical scene classification differs");
            local_zones.emplace(zone.value.pointer,key);next.zones.push_back({key,zone.value.pointer,zone.value.uid});}
        const direct_monster_identity::Scope direct{world.uuid,world.world_epoch,world.generation,root,scope.region,monsters.monster_manager};
        for(const auto& monster:monsters.monsters){std::string key;uintptr_t registered=0;const auto exception=lookup(image,monster.uid,registered);
            if(exception||registered!=monster.pointer||!monster_uids.insert(monster.uid).second)return fail(error,"Intervention Monster actual full UID lookup/unique census differs SEH="+std::to_string(exception));
            if(!monster.zone){if(!direct_monster_identity::registry.resolve(direct,monster.pointer,monster.uid,key,error))return false;}
            else{const auto zone=std::find_if(monsters.zones.begin(),monsters.zones.end(),[&](const auto& value){return value.value.pointer==monster.zone;});
                if(zone==monsters.zones.end())return fail(error,"Intervention Monster current Zone reciprocal owner absent");const auto member=std::find(zone->monsters.begin(),zone->monsters.end(),monster.uid);
                if(member==zone->monsters.end())return fail(error,"Intervention Monster absent from exact current Zone member roster");key=local_zones.at(monster.zone)+":monster:"+std::to_string(member-zone->monsters.begin());}
            if(!monster_keys.emplace(monster.pointer,key).second||!add(next,monster.pointer,key,error))return fail(error,error.empty()?"Intervention Monster canonical actor duplicated":error);}}
    std::map<uintptr_t,std::string> boss_keys;for(const auto& [key,entry]:next.bosses.live){boss_keys.emplace(entry.pointer,key);if(!add(next,entry.pointer,key,error))return false;}
    std::set<uintptr_t> actors;std::set<std::pair<uint8_t,std::string>> canonical;
    for(const auto& user:next.users.rows){Actor actor;actor.object=user.object;actor.uid=user.uid;actor.target.family=Family(uint8_t(user.family));
        switch(actor.target.family){
        case Family::Boss:if(!boss_keys.contains(user.object))return fail(error,"Intervention current Character Boss lacks saved/original-birth identity");actor.target.key=boss_keys.at(user.object);break;
        case Family::Monster:if(!monster_keys.contains(user.object))return fail(error,"Intervention current Character Monster absent from complete scene roster");actor.target.key=monster_keys.at(user.object);break;
        case Family::Npc:if(!npc_keys.contains(user.object))return fail(error,"Intervention current Character NPC lacks current original identity");actor.target.key=npc_keys.at(user.object);break;
        case Family::Toon:if(!toon_ids.contains(user.object))return fail(error,"Intervention current Character Toon lacks reciprocal Subscriber identity");actor.target.key="subscriber:"+std::to_string(toon_ids.at(user.object));break;
        default:return fail(error,"Intervention current Character concrete family invalid");}
        if(!valid(actor.target)||!actors.insert(user.object).second||!canonical.emplace(uint8_t(actor.target.family),actor.target.key).second)return fail(error,"Intervention complete current Actor canonical identity duplicated or invalid");next.actors.push_back(std::move(actor));}
    if(monster_keys.size()!=std::count_if(next.actors.begin(),next.actors.end(),[](const auto& value){return value.target.family==Family::Monster;})||boss_keys.size()!=std::count_if(next.actors.begin(),next.actors.end(),[](const auto& value){return value.target.family==Family::Boss;}))return fail(error,"Intervention complete Character user list omits a current physical Monster/Boss");
    std::sort(next.actors.begin(),next.actors.end(),[](const auto& a,const auto& b){return std::pair{uint8_t(a.target.family),a.target.key}<std::pair{uint8_t(b.target.family),b.target.key};});std::sort(next.zones.begin(),next.zones.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    building_mixed_identity::opened_file::CurrentWorld after;if(!native_open_runtime::read_current_world(after,error)||!same(world,after))return fail(error,error.empty()?"Intervention actual NativeOpen changed during complete canonical census":error);
    out=std::move(next);error.clear();return true;
}
}

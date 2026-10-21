#include "npc_quest_target_runtime.h"
#include "world_zone_runtime.h"
#include "world_zone_edit.h"
#include "zone_edit_identity.h"
#include "world_zone_serialization.h"
#include "scene_clone/quest_preview.h"
#include "npc_zone_append.h"
#include "npc_quest_building_key.h"
#include "building_capture_identity.h"
#include "native_open_runtime.h"
#include "root_saved_building.h"
#include "typed_action_runtime.h"
#include "dungeon_zone_cascade.h"
#ifndef MT2MP_QUEST_TARGET_RUNTIME_TEST
#include "mt2mp/core/logger.h"
#else
#include <iostream>
template<class... Args> void quest_test_log(const char* message, const Args&...) { std::cerr << message << '\n'; }
#define LOG_ERROR(...) quest_test_log(__VA_ARGS__)
#define LOG_INFO(...) quest_test_log(__VA_ARGS__)
#endif
#include "mt2mp/net/snapshot_sync.h"
#include <atomic>
#include <mutex>
#include <optional>

namespace mt2mp::game::npc_quest_target_runtime {
namespace {
namespace q = npc_quest_targets;
namespace opened = building_mixed_identity::opened_file;
using Owner = building_capture_identity::StreamOwner;
constexpr q::Budget budget{4096, 4096, 1048576, 4096};
uintptr_t image = 0;
uint32_t main_thread = 0;
bool hooks = false;
std::atomic<bool> host_active = false, client_active = false, emergency = false;
std::mutex gate;
std::string uuid, sha, failure;
uint64_t generation = 0, next_ticket = 0;
uintptr_t bound_context = 0;
opened::CurrentWorld origin;
struct Save { world_zone_serialization::Pool all; q::SaveFrame frame; Owner owner; uint32_t thread = 0; };
struct Load { world_zone_serialization::Load all; q::LoadFrame frame; uintptr_t property = 0, manager = 0; uint32_t thread = 0; q::ChildHeader child; bool child_pending = false; };
std::map<Ticket, Save> saves;
std::map<Ticket, Load> loads;
std::map<uint32_t, Ticket> load_stack;
std::map<uintptr_t, q::Array> observed;
std::map<int32_t, q::SavedPool> pools;
struct ScenePool {dungeon_scene_roster::Scene scene;uintptr_t manager=0;q::SavedPool saved;};
std::optional<std::vector<ScenePool>> staged_scenes;
dungeon_scene_identity_runtime::World staged_scene_world;
std::map<uintptr_t,ScenePool> scene_pools;
struct WorldZonePool {
    std::string scene;
    uintptr_t root=0,region=0,manager=0;
    std::map<uint64_t,world_zone_runtime::Current> rows;
};
std::map<uintptr_t,world_zone_serialization::Pool> world_zone_observed;
std::map<uintptr_t,WorldZonePool> world_zone_pools;

std::map<std::string, npc_zone_append::Receipt> zone_appends;
#ifdef MT2MP_QUEST_TARGET_RUNTIME_TEST
using TestRead = bool(*)(uintptr_t, void*, size_t, std::string&);
using TestLink = bool(*)(uintptr_t, uintptr_t, int32_t&, std::string&);
using TestUid=uint32_t(*)(uint64_t,uintptr_t&);
TestUid test_uid=nullptr;
TestRead test_read = nullptr;
TestLink test_link = nullptr;
#endif
struct Memory {
    bool read(uintptr_t p, void* out, size_t bytes, std::string& error) {
#ifdef MT2MP_QUEST_TARGET_RUNTIME_TEST
        if (test_read) return test_read(p, out, bytes, error);
#endif
        return root_saved_building::bytes(reinterpret_cast<void*>(p), 0, out, bytes, error);
    }
} memory;
bool reject(std::string& error, const std::string& cause) {
    if (failure.empty()) { failure = cause; LOG_ERROR("[QuestTargets] snapshot={} cause={}; original serialization unchanged", uuid, cause); }
    error = failure; pools.clear();scene_pools.clear();world_zone_pools.clear(); return false;
}
void caught(const char* cause) noexcept {
    emergency = true; OutputDebugStringA("ERROR QuestTargets observer exception: ");
    OutputDebugStringA(cause); OutputDebugStringA("; target bindings blocked, original unchanged\n");
}
bool healthy(std::string& error) {
    if (emergency) return reject(error, "Quest target observer exception; original diagnostic retained");
    if (!failure.empty()) { error = failure; return false; }
    return true;
}
template<class Left,class Right>bool same_world(const Left& a,const Right& b) {
    return a.generation == b.generation && a.world_epoch == b.world_epoch && a.game == b.game &&
        a.mode == b.mode && a.state == b.state && a.world == b.world && a.map == b.map;
}
void reset(const std::string& id, const std::string& digest) {
    uuid = id; sha = digest; failure.clear(); emergency = false; saves.clear(); loads.clear();
    load_stack.clear(); observed.clear(); pools.clear();scene_pools.clear();world_zone_observed.clear();world_zone_pools.clear();staged_scenes.reset();staged_scene_world={}; zone_appends.clear(); bound_context = 0; main_thread = GetCurrentThreadId();
}
bool context(uintptr_t actual, std::string& error) {
    if (!actual || (bound_context && bound_context != actual)) return reject(error, "Zone property save/load context differs");
    bound_context = actual; return true;
}
DWORD contains_call(uintptr_t ctx, uintptr_t object, int32_t& id, bool& present) {
    __try { present = reinterpret_cast<bool(*)(void*, const void*, int32_t*)>(image + 0x89e690)(
        reinterpret_cast<void*>(ctx), reinterpret_cast<void*>(object), &id); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool contains(uintptr_t ctx, uintptr_t object, int32_t& id, std::string& error) {
#ifdef MT2MP_QUEST_TARGET_RUNTIME_TEST
    if (test_link) return test_link(ctx, object, id, error);
#endif
    bool present = false; id = 0; const auto code = contains_call(ctx, object, id, present);
    if (code || (present ? id <= 0 : id != 0)) return q::fail(error, "Zone SaveContext ContainsObject differs exception=" + std::to_string(code));
    return true;
}
#ifndef MT2MP_QUEST_TARGET_RUNTIME_TEST
DWORD lookup_call(uintptr_t ctx, int32_t id, uintptr_t& out) {
    __try { out = reinterpret_cast<uintptr_t(*)(void*, int32_t)>(image + 0x89e0f0)(reinterpret_cast<void*>(ctx), id); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
#endif
bool live_reader(uintptr_t reader, uintptr_t ctx, std::string& error) {
    uintptr_t mode = 0, actual = 0;
    if (!opened::validate_reader(reinterpret_cast<void*>(reader), generation, error) ||
        !q::at(memory, image, 0x13df320, mode, error) || !q::typed(memory, image, mode, 0x16e6730, error) ||
        !q::at(memory, mode, 0xd8, actual, error)) return false;
    return actual == ctx || q::fail(error, "Zone LoadContext is not active Mode context");
}
Load* top(std::string& error) {
    const auto thread = load_stack.find(GetCurrentThreadId());
    if (thread == load_stack.end()) { reject(error, "Zone event outside its owned property frame"); return nullptr; }
    const auto frame = loads.find(thread->second);
    if (frame == loads.end()) { reject(error, "Zone property frame missing"); return nullptr; }
    return &frame->second;
}
bool retain(q::Array value, std::string& error) {
    if (observed.size() >= 4096 || !observed.emplace(value.pointer, std::move(value)).second)
        return reject(error, "Zone property array duplicate or observation budget exceeded");
    return true;
}
DWORD exact_uid(uint64_t uid,uintptr_t& out){
#ifdef MT2MP_QUEST_TARGET_RUNTIME_TEST
    if(test_uid)return test_uid(uid,out);
#endif
    __try{out=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool validate_scene(ScenePool& scene,std::string& error){
    uintptr_t owner=0,shard=0;
    if(!q::typed(memory,image,scene.manager,0x16fe170,error)||!q::at(memory,scene.manager,0xf0,owner,error)||
       !q::at(memory,scene.scene.owner.region,0x70,shard,error))return false;
    if(owner!=scene.scene.owner.region||shard!=scene.scene.owner.shard)return q::fail(error,"Dungeon Quest Zone manager reciprocal Region/Shard differs");
    auto links=[&](uintptr_t pointer,int32_t& link,std::string& e){for(const auto& row:scene.saved.array.zones)if(row.pointer==pointer){link=row.link_id;return true;}
        return q::fail(e,"Dungeon Quest Zone member lacks original saved record");};
    q::Array actual;if(!q::observe_array(memory,image,scene.manager+0x48,budget,links,actual,error))return false;
    if(actual!=scene.saved.array)return q::fail(error,"Dungeon Quest Zone native saved identity/order/content changed");
    for(const auto& row:actual.zones){uintptr_t actor_region=0,actor_shard=0,found=0;
        if(!q::at(memory,row.pointer,0x178,actor_region,error)||!q::at(memory,row.pointer,0x170,actor_shard,error))return false;
        const auto exception=exact_uid(row.uid,found);
        if(exception||found!=row.pointer||actor_region!=owner||actor_shard!=shard)return q::fail(error,"Dungeon Quest Zone original full UID/scene owner differs exception="+std::to_string(exception));
    }return true;
}
#include "world_zone_snapshot.inc"
bool seal_world(const opened::CurrentWorld& world, bool captured, std::string& error) {
    if (!healthy(error) || !saves.empty() || !loads.empty() || !load_stack.empty() || !bound_context || observed.empty())
        return reject(error, failure.empty() ? "Zone snapshot lacks complete property returns/context" : failure);
    const auto map = reinterpret_cast<uintptr_t>(world.map), root = map + 0x1c0;
    q::Bounds regions;
    if (!q::typed(memory, image, map, 0x1742860, error) || !q::typed(memory, image, map + 0x230, 0x16f04e0, error) ||
        !q::bounds(memory, map + 0x230, 4096, regions, error)) return reject(error, error);
    std::map<int32_t, q::SavedPool> ready;
    for (int32_t i = 0; i < regions.count; ++i) {
        uintptr_t region = 0, manager = 0; int32_t id = -1;
        if (!q::at(memory, regions.data, static_cast<size_t>(i) * 8, region, error) ||
            !q::at(memory, region, 0x834, id, error) || !q::at(memory, region, 0x968, manager, error)) return reject(error, error);
        const auto found = observed.find(manager + 0x48);
        if (found == observed.end()) return reject(error, "saved root Region Monster property unobserved id=" + std::to_string(id));
        q::World current{{uuid, sha, world.world_epoch, world.generation}, image, map, root, region, 0, id};
        q::SavedPool saved{current.origin, id, found->second};
        const bool valid = captured ? q::validate_capture(memory, current, saved, budget, error) :
                                      q::validate_saved(memory, current, saved, budget, error);
        if (!valid || !ready.emplace(id, std::move(saved)).second)
            return reject(error, error.empty() ? "saved Region id repeated" : error);
    }
    if (ready.empty() || !q::unchanged(memory, map + 0x230, 4096, regions, error)) return reject(error, error.empty() ? "saved root Regions absent" : error);
    std::map<uintptr_t,ScenePool> ready_scenes;std::set<std::string> scene_keys;
    for(const auto& [id,saved]:ready)for(const auto& row:saved.array.zones)
        if(!scene_keys.insert(q::zone_key(sha,id,row)).second)return reject(error,"Quest Zone original global saved record key duplicated");
    if(staged_scenes){
        if(staged_scene_world.uuid!=uuid||staged_scene_world.sha256!=sha||!same_world(staged_scene_world,world))return reject(error,"Dungeon Quest Zone staged snapshot/world lifetime differs");
        for(auto scene:*staged_scenes){const auto found=observed.find(scene.manager+0x48);
            if(found==observed.end())return reject(error,"Dungeon scene Quest Zone native property unobserved");
            scene.saved={q::Origin{uuid,sha,world.world_epoch,world.generation},scene.scene.exterior_region,found->second};
            if(!validate_scene(scene,error))return reject(error,error);
            for(const auto& row:scene.saved.array.zones){const auto key=sha+":world:zone:"+(row.link_id>0?std::to_string(row.link_id):snapshot_zone_key::scene_suffix(
                scene.scene.exterior_region,scene.scene.exterior_slot,scene.scene.owner.instance_index,int32_t(row.saved_child)));
                if(!snapshot_zone_key::valid(key)||!scene_keys.insert(key).second)return reject(error,"Dungeon Quest Zone canonical saved record duplicated or invalid");
                scene.saved.action_keys.emplace(row.pointer,key);
            }
            if(!ready_scenes.emplace(scene.manager,std::move(scene)).second)return reject(error,"Dungeon Quest Zone manager duplicated");
        }
    }
    if(ready.size()+ready_scenes.size()!=observed.size())return reject(error,"Quest Zone serialization contains an unclassified native scene array");
    if(!world_zone_seal(world,ready,ready_scenes,error))return reject(error,error);
    origin = world; pools = std::move(ready);scene_pools=std::move(ready_scenes); observed.clear();staged_scenes.reset();staged_scene_world={};
    LOG_INFO("[QuestTargets] Native snapshot Monster pools bound uuid={} sha={} regions={}", uuid, sha, pools.size());
    error.clear(); return true;
}
bool current(int32_t id, q::World& world, const q::SavedPool*& saved, std::string& error) {
    if (!healthy(error)) return false;
    if (GetCurrentThreadId() != main_thread || host_active || client_active || net::current_snapshot_id() != uuid || !net::current_world_epoch())
        return q::fail(error, "Quest targets require current active snapshot on bound Mainthread");
    if (!origin.generation || !origin.world_epoch)
        return q::fail(error, "Quest targets require completed snapshot load after Host capture");
    opened::CurrentWorld actual;
    if (!native_open_runtime::read_current_world(actual, error)) return false;
    if (!same_world(origin, actual)) return q::fail(error, "Quest target native world generation changed");
    const auto found = pools.find(id);
    if (found == pools.end()) return q::fail(error, "Quest target saved Region pool absent id=" + std::to_string(id));
    const auto map = reinterpret_cast<uintptr_t>(actual.map); uintptr_t region = 0, selection = 0;
    auto read = [&](uintptr_t p, size_t offset, auto& out, std::string& cause) { return q::at(memory, p, offset, out, cause); };
    if (!region_saved_identity::resolve(image, map, map + 0x1c0, id, 0, region, read, error) ||
        !q::at(memory, image, 0x13df630, selection, error) || !q::typed(memory, image, selection, 0x17205d0, error)) return false;
    world = {found->second.origin, image, map, map + 0x1c0, region, selection, id}; saved = &found->second; return true;
}
}
bool stage_scenes(const dungeon_scene_identity_runtime::Census& census,std::string& error){
    std::lock_guard lock(gate);
    if((!host_active&&!client_active)||staged_scenes||GetCurrentThreadId()!=main_thread||!saves.empty()||!loads.empty()||!load_stack.empty())
        return reject(error,"Dungeon Quest Zone staging requires its idle active native snapshot observer");
    if(!healthy(error)||!census.snapshot_census||census.world.uuid!=uuid)return reject(error,error.empty()?"Dungeon Quest Zone staged owned snapshot UUID/census differs":error);
    std::vector<ScenePool> next;for(const auto& row:census.pools)next.push_back({row.scene,row.managers[1],{}});
    staged_scene_world=census.world;staged_scenes=std::move(next);error.clear();return true;
}
#include "scene_clone/quest_preview.inc"
bool bind_scene_destination(uintptr_t destination,std::string& key,std::string& error){
    key.clear();
    dungeon_scene_identity_runtime::Census current_scenes;
    if(!dungeon_scene_identity_runtime::read_current(current_scenes,error))return false;
    std::lock_guard lock(gate);key.clear();opened::CurrentWorld actual;
    if(!healthy(error)||GetCurrentThreadId()!=main_thread||host_active||client_active||net::current_snapshot_id()!=uuid||
       !native_open_runtime::read_current_world(actual,error)||!same_world(origin,actual))return q::fail(error,error.empty()?"Dungeon Quest Zone requires current admitted NativeOpen snapshot":error);
    for(auto& [manager,scene]:scene_pools){(void)manager;for(const auto& row:scene.saved.array.zones)if(row.pointer==destination){
        const auto fresh=std::find_if(current_scenes.pools.begin(),current_scenes.pools.end(),[&](const auto& value){return value.managers[1]==scene.manager;});
        if(current_scenes.world.uuid!=actual.uuid||current_scenes.world.sha256!=actual.sha256||actual.uuid!=origin.uuid||actual.sha256!=origin.sha256||
           !same_world(current_scenes.world,actual)||fresh==current_scenes.pools.end()||
           fresh->scene.owner.key!=scene.scene.owner.key||fresh->scene.owner.shard!=scene.scene.owner.shard||fresh->scene.owner.region!=scene.scene.owner.region||
           fresh->scene.owner.dungeon!=scene.scene.owner.dungeon||fresh->scene.dungeon_uid!=scene.scene.dungeon_uid||
           fresh->scene.owner.exterior_uid!=scene.scene.owner.exterior_uid)
            return q::fail(error,"Dungeon Quest Zone current exterior/template/Instance ownership differs");
        if(!validate_scene(scene,error))return false;key=q::zone_key(scene.saved,row);error.clear();return true;}}
    return q::fail(error,"Dungeon Quest Zone destination lacks a confirmed saved scene record");
}
namespace {
#include "world_zone_cells.inc"
bool cell_lease(const action_session::Binding& binding,const action_session::Request& request,std::string& error){
    const auto controller=typed_action_runtime::controller_status();opened::CurrentWorld actual;
    const bool stroke=(request.action==action_session::Action::SetDungeonPalette&&request.dungeon_palette.edit==action_session::DungeonPaletteEdit::Cells)||request.action==action_session::Action::DungeonInput;
    const bool history=(request.action==action_session::Action::UndoBuilding||request.action==action_session::Action::RedoBuilding)&&(request.canonical_record_key=="dungeon-tiles:"+request.target_action_uuid||request.canonical_record_key=="dungeon-input:"+request.target_action_uuid);
    if(!healthy(error)||GetCurrentThreadId()!=main_thread||host_active||client_active||request.snapshot_uuid!=uuid||
       (!stroke&&!history)||binding.native_epoch!=origin.world_epoch||binding.native_generation!=origin.generation||
       !controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid||
       !native_open_runtime::read_current_world(actual,error)||!same_world(origin,actual))
        return q::fail(error,error.empty()?"Dungeon cell Quest Zone cascade requires exact current pending Close stroke":error);
    return true;
}
}
bool prepare_dungeon_cells(const action_session::Binding& binding,const action_session::Request& request,DungeonCellCascade& out,std::string& error){
    out={};std::lock_guard lock(gate);if(!cell_lease(binding,request,error))return false;
    DungeonCellCascade next;next.snapshot=request.snapshot_uuid;next.request=request.action_uuid;next.epoch=binding.native_epoch;next.generation=binding.native_generation;
    for(auto& [manager,scene]:scene_pools){if(!validate_scene(scene,error))return false;next.before.emplace(manager,scene.saved);}
    if(!prepare_world_zone_cells(next,error))return false;
    out=std::move(next);error.clear();return true;
}
bool confirm_dungeon_cells(const action_session::Binding& binding,const action_session::Request& request,DungeonCellCascade& receipt,std::string& error){
    if(request.action!=action_session::Action::SetDungeonPalette&&request.action!=action_session::Action::DungeonInput)return q::fail(error,"Dungeon cell forward Zone confirmation requires original stroke/input");
    dungeon_scene_identity_runtime::Census current_scenes;if(!dungeon_scene_identity_runtime::read_current(current_scenes,error))return false;
    std::lock_guard lock(gate);if(!cell_lease(binding,request,error))return false;
    if(receipt.confirmed||receipt.snapshot!=request.snapshot_uuid||receipt.request!=request.action_uuid||receipt.epoch!=binding.native_epoch||
       receipt.generation!=binding.native_generation||receipt.before.size()!=scene_pools.size())return q::fail(error,"Dungeon cell Quest Zone cascade receipt is absent, foreign or replayed");
    auto next=scene_pools;
    for(auto& [manager,scene]:next){const auto retained=receipt.before.find(manager);const auto& old=scene.saved;
        if(retained==receipt.before.end()||retained->second.origin!=old.origin||retained->second.saved_region!=old.saved_region||
           retained->second.array!=old.array||retained->second.action_keys!=old.action_keys)return q::fail(error,"Dungeon cell Quest Zone saved canonical state changed before original cascade completion");
        const auto fresh=std::find_if(current_scenes.pools.begin(),current_scenes.pools.end(),[&](const auto& value){return value.managers[1]==manager;});
        if(fresh==current_scenes.pools.end()||fresh->scene.owner.key!=scene.scene.owner.key||fresh->scene.owner.region!=scene.scene.owner.region||
           fresh->scene.owner.shard!=scene.scene.owner.shard||fresh->scene.owner.dungeon!=scene.scene.owner.dungeon||fresh->scene.dungeon_uid!=scene.scene.dungeon_uid)
            return q::fail(error,"Dungeon cell Quest Zone cascade changed an original complete scene owner");
        auto links=[&](uintptr_t pointer,int32_t& link,std::string& e){for(const auto& row:old.array.zones)if(row.pointer==pointer){link=row.link_id;return true;}
            return q::fail(e,"Dungeon cell Close cascade introduced an unbound Monster Zone");};
        q::Array actual;if(!q::observe_array(memory,image,manager+0x48,budget,links,actual,error))return false;
        const auto find=[](uint64_t uid,uintptr_t& found,std::string& e){const auto exception=exact_uid(uid,found);
            return !exception||q::fail(e,"Dungeon cell Quest Zone cascade original FindUID exception="+std::to_string(exception));};
        q::SavedPool changed;if(!dungeon_zone_cascade::reconcile(old,actual,find,changed,error))return false;scene.saved=std::move(changed);
        if(!validate_scene(scene,error))return false;
    }
    std::map<uintptr_t,WorldZonePool> other;
    if(!confirm_world_zone_cells(receipt,current_scenes,nullptr,nullptr,other,error))return false;
    scene_pools=std::move(next);world_zone_pools=std::move(other);receipt.confirmed=true;error.clear();return true;
}
bool confirm_dungeon_cell_history(const action_session::Binding& binding,const action_session::Request& request,DungeonCellCascade& receipt,
                                 const DungeonCellCascade& original,const std::set<uint64_t>& restored_zones,std::string& error){
    dungeon_scene_identity_runtime::Census scenes;if(!dungeon_scene_identity_runtime::read_current(scenes,error))return false;
    std::lock_guard lock(gate);if(!cell_lease(binding,request,error))return false;
    if((request.action!=action_session::Action::UndoBuilding&&request.action!=action_session::Action::RedoBuilding)||receipt.confirmed||
       receipt.snapshot!=request.snapshot_uuid||receipt.request!=request.action_uuid||receipt.epoch!=binding.native_epoch||receipt.generation!=binding.native_generation||
       receipt.before.size()!=scene_pools.size()||original.snapshot!=receipt.snapshot||original.epoch!=receipt.epoch||original.generation!=receipt.generation)
        return q::fail(error,"Dungeon cell history Zone receipts are absent, foreign or replayed");
    auto next=scene_pools;
    for(auto& [manager,scene]:next){const auto previous=receipt.before.find(manager);const auto source=original.before.find(manager);
        if(previous==receipt.before.end()||source==original.before.end()||previous->second.origin!=scene.saved.origin||
           previous->second.array!=scene.saved.array||previous->second.action_keys!=scene.saved.action_keys)
            return q::fail(error,"Dungeon cell history canonical Zone state changed before original graph completion");
        const auto fresh=std::find_if(scenes.pools.begin(),scenes.pools.end(),[&](const auto& pool){return pool.managers[1]==manager;});
        if(fresh==scenes.pools.end()||fresh->scene.owner.key!=scene.scene.owner.key||fresh->scene.owner.region!=scene.scene.owner.region||fresh->scene.owner.shard!=scene.scene.owner.shard)
            return q::fail(error,"Dungeon cell history Zone complete scene owner changed");
        std::set<uint64_t> restored;
        for(const auto& row:source->second.array.zones)if(restored_zones.contains(row.uid))restored.insert(row.uid);
        const auto links=[&](uintptr_t pointer,int32_t& link,std::string& e){uint64_t uid=0;if(!q::at(memory,pointer,0x18,uid,e))return false;
            for(const auto& row:scene.saved.array.zones)if(row.pointer==pointer&&row.uid==uid){link=row.link_id;return true;}
            for(const auto& row:source->second.array.zones)if(row.uid==uid&&restored.contains(uid)){link=row.link_id;return true;}
            return q::fail(e,"Dungeon cell history native Zone has no current or original Make/Edit receipt");};
        q::Array actual;if(!q::observe_array(memory,image,manager+0x48,budget,links,actual,error))return false;
        const auto find=[](uint64_t uid,uintptr_t& pointer,std::string& e){const auto exception=exact_uid(uid,pointer);return !exception||q::fail(e,"Dungeon cell history original Zone FindUID exception="+std::to_string(exception));};
        q::SavedPool changed;if(!dungeon_zone_cascade::reconcile_restored(scene.saved,source->second,actual,restored,find,changed,error))return false;
        scene.saved=std::move(changed);if(!validate_scene(scene,error))return false;
    }
    std::map<uintptr_t,WorldZonePool> other;
    if(!confirm_world_zone_cells(receipt,scenes,&original,&restored_zones,other,error))return false;
    scene_pools=std::move(next);world_zone_pools=std::move(other);receipt.confirmed=true;error.clear();return true;
}
bool bind_current_destination(uintptr_t destination,std::string& key,bool& in_dungeon,std::string& error){
    key.clear();in_dungeon=false;int32_t id=-1;
    {
        std::lock_guard lock(gate);opened::CurrentWorld actual;uintptr_t shard=0,region=0,reverse=0;
        if(!healthy(error))return false;
        if(GetCurrentThreadId()!=main_thread||host_active||client_active||net::current_snapshot_id()!=uuid||!origin.generation||!origin.world_epoch)
            return q::fail(error,"Quest reference requires its current admitted NativeOpen snapshot on Mainthread");
        if(!native_open_runtime::read_current_world(actual,error))return false;
        if(!same_world(origin,actual))return q::fail(error,"Quest reference current NativeOpen lifetime changed");
        if(!q::at(memory,destination,0x170,shard,error)||!q::at(memory,destination,0x178,region,error)||!q::at(memory,region,0x70,reverse,error))return false;
        if(!shard||!region||reverse!=shard)return q::fail(error,"Quest reference actual reciprocal Shard/Region differs");
        in_dungeon=shard!=uintptr_t(actual.map)+0x1c0;
        if(!in_dungeon&&!q::at(memory,region,0x834,id,error))return false;
    }
    if(in_dungeon){
        bool held=false;
        if(!typed_action_runtime::resolve_held_dungeon_destination(destination,key,held,error))return false;
        if(held)return true;
    }
    return in_dungeon?bind_scene_destination(destination,key,error):bind_destination(id,destination,key,error);
}
bool initialize(std::string& error) {
    std::lock_guard lock(gate);
    if (!native::ready()) return q::fail(error, "Quest targets exact native resolver unavailable");
    image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const std::array<unsigned char, 16> save{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x78};
    const std::array<unsigned char, 19> load{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xa8,0,0,0};
    auto guard = [&](uintptr_t rva, const auto& bytes) {
        MEMORY_BASIC_INFORMATION page{}; std::vector<unsigned char> actual(bytes.size());
        if (!VirtualQuery(reinterpret_cast<void*>(image + rva), &page, sizeof page) || page.Type != MEM_IMAGE ||
            page.AllocationBase != reinterpret_cast<void*>(image) || page.Protect != PAGE_EXECUTE_READ ||
            !memory.read(image + rva, actual.data(), actual.size(), error) || !std::equal(actual.begin(), actual.end(), bytes.begin()))
            return q::fail(error, "Quest property original entry guard differs rva=" + std::to_string(rva));
        return true;
    };
    if (!guard(q::save_property_rva, save) || !guard(q::load_property_rva, load)) { image = 0; return false; }
    error.clear(); return true;
}
void* entrypoint(bool save) { return image ? reinterpret_cast<void*>(image + (save ? q::save_property_rva : q::load_property_rva)) : nullptr; }
bool confirm_hooks(std::string& error) { std::lock_guard lock(gate); if (!image) return q::fail(error, "Quest property resolver absent"); hooks = true; error.clear(); return true; }
bool arm_host(const std::string& id, const CaptureWorld& capture, std::string& error) {
    std::lock_guard lock(gate);
    if (!hooks || host_active || client_active || !saves.empty() || !loads.empty()) return q::fail(error, "Quest snapshot observer busy/uninstalled");
    if (!capture.game || !capture.mode || !capture.state || !capture.world || !capture.map)
        return q::fail(error, "Quest Host capture requires complete BuildingCapture world witness");
    opened::CurrentWorld world;
    world.game = capture.game; world.mode = capture.mode; world.state = capture.state;
    world.world = capture.world; world.map = capture.map;
    // The first local capture precedes NativeOpen; a later distributed capture
    // preserves the already completed load of this same physical world.
    if (opened::status().current_origin_available) {
        opened::CurrentWorld loaded;
        if (!native_open_runtime::read_current_world(loaded, error)) return false;
        if (loaded.game != world.game || loaded.mode != world.mode || loaded.state != world.state ||
            loaded.world != world.world || loaded.map != world.map)
            return q::fail(error, "Quest Host loaded origin differs from BuildingCapture world witness");
        world = std::move(loaded);
    }
    reset(id, ""); origin = std::move(world);
    host_active = true; error.clear(); return true;
}
bool seal_host(const std::string& id, const std::string& digest, const CaptureWorld& capture, std::string& error) {
    std::lock_guard lock(gate);
    if (!host_active || id != uuid || GetCurrentThreadId() != main_thread) return reject(error, "Quest Host snapshot seal lifetime differs");
    if (capture.game != origin.game || capture.mode != origin.mode || capture.state != origin.state ||
        capture.world != origin.world || capture.map != origin.map)
        return reject(error, "Quest Host BuildingCapture world witness changed");
    if (origin.generation) {
        opened::CurrentWorld loaded;
        if (!native_open_runtime::read_current_world(loaded, error) || !same_world(loaded, origin))
            return reject(error, "Quest Host completed origin changed during capture: " + error);
    }
    sha = digest; const bool confirmed = seal_world(origin, !origin.generation, error); host_active = false; return confirmed;
}
bool arm_client(const std::string& id, const std::string& digest, std::string& error) {
    std::lock_guard lock(gate); const auto witness = opened::status();
    if (!hooks || host_active || client_active || !saves.empty() || !loads.empty() || witness.state != "pinned" ||
        !witness.held || witness.uuid != id || witness.sha256 != digest || !witness.generation)
        return q::fail(error, "Quest Client arm lacks idle pinned exact NativeOpen receipt");
    reset(id, digest); generation = witness.generation; client_active = true; error.clear(); return true;
}
bool seal_client(std::string& error) {
    std::lock_guard lock(gate);
    if (!client_active || GetCurrentThreadId() != main_thread) return reject(error, "Quest Client seal lifetime differs");
    opened::CurrentWorld world;
    if (!opened::read_completed_world(generation, world, error) || world.uuid != uuid || world.sha256 != sha)
        return reject(error, "Quest Client completed native snapshot differs: " + error);
    const bool confirmed = seal_world(world, false, error); client_active = false; return confirmed;
}
void cancel(const std::string& cause) { std::lock_guard lock(gate); host_active = false; client_active = false; failure = cause; saves.clear(); loads.clear(); load_stack.clear(); observed.clear(); pools.clear();scene_pools.clear();world_zone_observed.clear();world_zone_pools.clear();staged_scenes.reset();staged_scene_world={}; zone_appends.clear(); }
Ticket save_before(const void* property, void* manager, void* writer, void* ctx) noexcept try {
    if (!host_active) return 0;
    Owner owner; bool own = false; std::string error;
    const bool checked = building_capture_identity::stream_owner(writer, owner, own, error);
    std::lock_guard lock(gate);
    if (!host_active || !healthy(error)) return 0;
    if (!checked) { reject(error, "Quest owned snapshot writer: " + error); return 0; }
    if (!own) return 0;
    if (owner.capture_uuid != uuid || !context(reinterpret_cast<uintptr_t>(ctx), error)) { reject(error, "Quest writer UUID/context differs: " + error); return 0; }
    auto links = [&](uintptr_t object, int32_t& id, std::string& cause) { return contains(reinterpret_cast<uintptr_t>(ctx), object, id, cause); };
    Save value; value.owner = owner; value.thread = GetCurrentThreadId();
    if (!q::begin_save(memory, image, reinterpret_cast<uintptr_t>(property), reinterpret_cast<uintptr_t>(manager),
        reinterpret_cast<uintptr_t>(writer), reinterpret_cast<uintptr_t>(ctx), budget, links, value.frame, error)) { reject(error, error); return 0; }
    if(!world_zone_serialization::read(memory,image,reinterpret_cast<uintptr_t>(manager),links,value.all,error)){reject(error,error);return 0;}
    if (saves.size() >= 256 || next_ticket == UINT64_MAX) { reject(error, "Quest save frame budget/sequence exceeded"); return 0; }
    const auto ticket = ++next_ticket; saves.emplace(ticket, std::move(value)); return ticket;
} catch (const std::exception& e) { caught(e.what()); return 0; } catch (...) { caught("non-standard save before exception"); return 0; }
void save_after(Ticket ticket, const void* property, void* manager, void* writer, void* ctx, bool result) noexcept try {
    if (!ticket) return;
    Owner owner; bool own = false; std::string error;
    const bool checked = building_capture_identity::stream_owner(writer, owner, own, error);
    std::lock_guard lock(gate); if (!healthy(error)) return;
    const auto found = saves.find(ticket);
    if (!checked || !own || found == saves.end()) { reject(error, "Quest save return/file owner absent: " + error); return; }
    const auto& saved = found->second;
    if (saved.thread != GetCurrentThreadId() || saved.owner.capture_uuid != owner.capture_uuid ||
        saved.owner.stream != owner.stream || saved.owner.consumer != owner.consumer || saved.owner.file != owner.file ||
        (saved.owner.task && saved.owner.task != owner.task)) { reject(error, "Quest save writer/thread lifetime changed"); return; }
    auto links = [&](uintptr_t object, int32_t& id, std::string& cause) { return contains(reinterpret_cast<uintptr_t>(ctx), object, id, cause); };
    q::Array value;
    if (!q::finish_save(memory, image, saved.frame, reinterpret_cast<uintptr_t>(property), reinterpret_cast<uintptr_t>(manager),
        reinterpret_cast<uintptr_t>(writer), reinterpret_cast<uintptr_t>(ctx), result, budget, links, value, error) || !retain(std::move(value), error)) { reject(error, error); return; }
    world_zone_serialization::Pool all;
    if(!world_zone_serialization::read(memory,image,reinterpret_cast<uintptr_t>(manager),links,all,error)||all!=saved.all||!world_zone_retain(std::move(all),error)){reject(error,error.empty()?"World Zone identity/order/link changed during original save":error);return;}
    saves.erase(found);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard save after exception"); }
Ticket load_before(const void* property, void* manager, void* reader, void* ctx) noexcept try {
    if (!client_active) return 0;
    std::lock_guard lock(gate); std::string error;
    if (!healthy(error)) return 0;
    const auto r = reinterpret_cast<uintptr_t>(reader), c = reinterpret_cast<uintptr_t>(ctx);
    if (!live_reader(r, c, error) || !context(c, error)) { reject(error, "Quest load reader/context rejected: " + error); return 0; }
    Load value; value.property = reinterpret_cast<uintptr_t>(property); value.manager = reinterpret_cast<uintptr_t>(manager); value.thread = GetCurrentThreadId();
    if (load_stack.contains(value.thread) || loads.size() >= 256 || next_ticket == UINT64_MAX ||
        !q::begin_load(memory, image, value.property, value.manager, r, c, budget, value.frame, error)) { reject(error, "Quest load frame rejected: " + error); return 0; }
    for(size_t type=0;type<4;++type){q::Bounds bounds;if(!q::typed(memory,image,value.manager+0x18+type*0x30,0x172e1e0,error)||!q::bounds(memory,value.manager+0x18+type*0x30,4096,bounds,error)||bounds.count){reject(error,error.empty()?"World Zone native load requires four initially empty arrays":error);return 0;}}
    const auto ticket = ++next_ticket; load_stack.emplace(value.thread, ticket); loads.emplace(ticket, std::move(value)); return ticket;
} catch (const std::exception& e) { caught(e.what()); return 0; } catch (...) { caught("non-standard load before exception"); return 0; }
void children_before(uintptr_t caller, void* reader) noexcept try {
    if (!client_active || caller != q::load_children_return_rva) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (frame->child_pending || frame->frame.reader != reinterpret_cast<uintptr_t>(reader) ||
        !live_reader(frame->frame.reader, frame->frame.context, error) || !q::child_header(memory, frame->frame.reader, frame->child, error)) { reject(error, "Quest child header rejected: " + error); return; }
    frame->child_pending = true;
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard children before exception"); }
void children_after(uintptr_t caller, void* reader, int32_t count) noexcept try {
    if (!client_active || caller != q::load_children_return_rva) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (!frame->child_pending || !live_reader(frame->frame.reader, frame->frame.context, error) ||
        !frame->frame.children(caller, reinterpret_cast<uintptr_t>(reader), frame->child.label, frame->child.count, count, error)) { reject(error, "Quest BeginChildren return rejected: " + error); return; }
    if(!frame->all.children(frame->child.label,frame->child.count,count,error)){reject(error,error);return;}
    frame->child_pending = false;
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard children after exception"); }
void registered(uintptr_t caller, void* ctx, int32_t id, void* object) noexcept try {
    if (!client_active || (caller != q::load_link_return_rva && caller != q::load_inline_return_rva)) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (!live_reader(frame->frame.reader, frame->frame.context, error)) { reject(error, error); return; }
    if (id > 0) {
#ifndef MT2MP_QUEST_TARGET_RUNTIME_TEST
        uintptr_t actual = 0; const auto code = lookup_call(reinterpret_cast<uintptr_t>(ctx), id, actual);
        if (code || actual != reinterpret_cast<uintptr_t>(object)) { reject(error, "Quest registered ID lookup differs exception=" + std::to_string(code)); return; }
#endif
    }
    if (!q::registered(memory, image, frame->frame, caller, reinterpret_cast<uintptr_t>(ctx), id, reinterpret_cast<uintptr_t>(object), error)){reject(error,error);return;}
    int32_t type=-1;const bool linked=caller==q::load_link_return_rva;
    if((linked?id<=0:id!=0)||!q::at(memory,reinterpret_cast<uintptr_t>(object),0x24c,type,error)||!frame->all.registered(type,reinterpret_cast<uintptr_t>(object),id,error))reject(error,error.empty()?"World Zone original registration caller/link differs":error);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard registered exception"); }
void load_after(Ticket ticket, const void* property, void* manager, void* reader, void* ctx, bool result) noexcept try {
    if (!ticket) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    const auto found = loads.find(ticket);
    if (!result || found == loads.end() || frame != &found->second || frame->child_pending ||
        frame->property != reinterpret_cast<uintptr_t>(property) || frame->manager != reinterpret_cast<uintptr_t>(manager) ||
        frame->frame.reader != reinterpret_cast<uintptr_t>(reader) || frame->frame.context != reinterpret_cast<uintptr_t>(ctx) ||
        !live_reader(frame->frame.reader, frame->frame.context, error)) { reject(error, "Quest original LoadStream result/lifetime differs: " + error); return; }
    q::Array value;
    if (!frame->frame.finish(memory, image, budget, value, error) || !retain(std::move(value), error)) { reject(error, error); return; }
    world_zone_serialization::Pool all;if(!frame->all.finish(memory,image,frame->manager,all,error)||!world_zone_retain(std::move(all),error)){reject(error,error);return;}
    load_stack.erase(frame->thread); loads.erase(found);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard load after exception"); }
void original_failed(Ticket ticket, const std::string& cause) noexcept try {
    if (!ticket) return; std::lock_guard lock(gate); std::string error; reject(error, "original Zone property failed: " + cause);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("original failure reporting exception"); }
bool begin_zone_append(int32_t id, const std::string& action_uuid, std::string& error) {
    std::lock_guard lock(gate); q::World world; const q::SavedPool* saved = nullptr;
    if (!current(id, world, saved, error)) return false;
    if (zone_appends.contains(action_uuid)) return q::fail(error, "Zone append action already attempted; no replay");
    if (zone_appends.size() >= 4096) return q::fail(error, "Zone append retained action budget exceeded");
    npc_zone_append::Receipt receipt;
    if (!npc_zone_append::begin(memory, world, *saved, budget, action_uuid, receipt, error)) return false;
    zone_appends.emplace(action_uuid, std::move(receipt)); return true;
}
bool finish_zone_append(int32_t id, const std::string& action_uuid, uintptr_t created,
    uint64_t uid, const npc_quest_targets::Content& required, std::string& key, std::string& error) {
    std::lock_guard lock(gate); key.clear();
    const auto held = zone_appends.find(action_uuid);
    if (held == zone_appends.end()) return q::fail(error, "Zone append has no bound before receipt");
    if (held->second.consumed) return q::fail(error, "Zone append receipt already consumed; no replay");
    q::World world; const q::SavedPool* saved = nullptr;
    if (!current(id, world, saved, error)) { held->second.consumed = true; return false; }
    auto found = pools.find(id);
    if (found == pools.end() || &found->second != saved) {
        held->second.consumed = true; return q::fail(error, "Zone append current pool changed");
    }
    return npc_zone_append::finish(memory, world, found->second, budget, held->second,
        created, uid, required, key, error);
}
bool read_current(int32_t id, CurrentTargets& out, std::string& error) {
    std::lock_guard lock(gate); out = {}; q::World world; const q::SavedPool* saved = nullptr;
    if (!current(id, world, saved, error)) return false;
    auto binding = [&](uintptr_t pointer, std::string& key, std::string& cause) { return q::building_key(world, pointer, key, cause); };
    q::Pools result; if (!q::read(memory, world, *saved, budget, binding, result, error)) return false;
    out = {uuid, id, std::move(result.buildings), std::move(result.monster_zones)}; return true;
}
bool bind_destination(int32_t id, uintptr_t destination, std::string& key, std::string& error) {
    std::lock_guard lock(gate); key.clear(); q::World world; const q::SavedPool* saved = nullptr;
    if (!current(id, world, saved, error)) return false;
    auto binding = [&](uintptr_t pointer, std::string& result, std::string& cause) { return q::building_key(world, pointer, result, cause); };
    return q::destination(memory, world, *saved, budget, binding, destination, key, error);
}
#include "world_zone_current.inc"
#include "world_zone_edit.inc"
}
namespace mt2mp::game::world_zone_runtime {
bool prepare_edit(const action_session::Binding& binding,const std::string& snapshot,int32_t region,const std::string& operation,Edit& out,std::string& error){
    return npc_quest_target_runtime::prepare_world_zone_edit(binding,snapshot,region,operation,out,error);
}
bool preview_edit(const Edit& edit,uintptr_t born,uint64_t born_uid,const std::map<uint64_t,Current>& restored,Preview& out,std::string& error){
    return npc_quest_target_runtime::preview_world_zone_edit(edit,born,born_uid,restored,out,error);
}
bool publish_edit(Edit& edit,const Preview& preview,std::string& error){
    return npc_quest_target_runtime::publish_world_zone_edit(edit,preview,error);
}
bool resolve_current(const action_session::Binding& binding,const std::string& snapshot,uintptr_t object,uint64_t full_uid,Current& out,std::string& error){
    return npc_quest_target_runtime::resolve_world_zone(binding,snapshot,object,full_uid,out,error);
}
bool read_current(const action_session::Binding& binding,const std::string& snapshot,std::vector<Current>& out,std::string& error){
    return npc_quest_target_runtime::read_world_zones(binding,snapshot,out,error);
}
}

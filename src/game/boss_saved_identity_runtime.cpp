#include "boss_saved_identity_runtime.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
#include "boss_identity.h"
#include "boss_snapshot_alias.h"
#include "mt2mp/net/snapshot_buffer.h"
#include <fstream>
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "building_capture_identity.h"
#include "root_saved_building.h"
#ifndef MT2MP_BOSS_SAVED_RUNTIME_TEST
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

namespace mt2mp::game::native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld&, std::string&);
}

namespace mt2mp::game::boss_saved_identity_runtime {
namespace {
namespace q = boss_saved_identity;
namespace opened = building_mixed_identity::opened_file;
using Owner = building_capture_identity::StreamOwner;
uintptr_t image = 0;
uint32_t main_thread = 0;
bool hooks = false;
std::atomic<bool> host_active = false, client_active = false, emergency = false;
std::mutex gate;
std::string uuid, sha, failure;
uint64_t generation = 0, next_ticket = 0;
uintptr_t bound_context = 0;
opened::CurrentWorld origin;
struct Save { q::SaveFrame frame; Owner owner; uint32_t thread = 0; };
struct Load { q::LoadFrame frame; uintptr_t property = 0, manager = 0; uint32_t thread = 0; q::ChildHeader child; bool child_pending = false; };
std::map<Ticket, Save> saves;
std::map<Ticket, Load> loads;
std::map<uint32_t, Ticket> load_stack;
std::map<uintptr_t, q::Pool> observed;
std::map<uintptr_t, q::SavedPool> pools;
std::optional<dungeon_scene_identity_runtime::Census> scene_census;
namespace attachment=boss_snapshot_alias;
attachment::Manifest prepared, captured;
std::string prepared_uuid;
bool prepared_present=false;
uint64_t prepared_bytes=0;
std::string prepared_sha;
DWORD exact_uid(uint64_t uid,uintptr_t& out){
    __try{out=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

#ifdef MT2MP_BOSS_SAVED_RUNTIME_TEST
using TestRead = bool(*)(uintptr_t, void*, size_t, std::string&);
using TestLink = bool(*)(uintptr_t, uintptr_t, int32_t&, std::string&);
TestRead test_read = nullptr;
TestLink test_link = nullptr;
#endif
struct Memory {
    bool copy(uintptr_t p, void* out, size_t bytes, std::string& error) { return read(p, out, bytes, error); }
    bool read(uintptr_t p, void* out, size_t bytes, std::string& error) {
#ifdef MT2MP_BOSS_SAVED_RUNTIME_TEST
        if (test_read) return test_read(p, out, bytes, error);
#endif
        return root_saved_building::bytes(reinterpret_cast<void*>(p), 0, out, bytes, error);
    }
} memory;
bool reject(std::string& error, const std::string& cause) {
    if (failure.empty()) { failure = cause; LOG_ERROR("[SavedBoss] snapshot={} cause={}; original serialization unchanged", uuid, cause); }
    error = failure; pools.clear(); return false;
}
void caught(const char* cause) noexcept {
    emergency = true; OutputDebugStringA("ERROR SavedBoss observer exception: ");
    OutputDebugStringA(cause); OutputDebugStringA("; target bindings blocked, original unchanged\n");
}
bool healthy(std::string& error) {
    if (emergency) return reject(error, "Boss identity observer exception; original diagnostic retained");
    if (!failure.empty()) { error = failure; return false; }
    return true;
}
template<class Left,class Right>bool same_world(const Left& a,const Right& b) {
    return a.generation == b.generation && a.world_epoch == b.world_epoch && a.game == b.game &&
        a.mode == b.mode && a.state == b.state && a.world == b.world && a.map == b.map;
}
void reset(const std::string& id, const std::string& digest) {
    uuid = id; sha = digest; captured={}; failure.clear(); emergency = false; saves.clear(); loads.clear();
    load_stack.clear(); observed.clear(); pools.clear(); scene_census.reset(); bound_context = 0; main_thread = GetCurrentThreadId();
}
bool context(uintptr_t actual, std::string& error) {
    if (!actual || (bound_context && bound_context != actual)) return reject(error, "Boss property save/load context differs");
    bound_context = actual; return true;
}
DWORD contains_call(uintptr_t ctx, uintptr_t object, int32_t& id, bool& present) {
    __try { present = reinterpret_cast<bool(*)(void*, const void*, int32_t*)>(image + 0x89e690)(
        reinterpret_cast<void*>(ctx), reinterpret_cast<void*>(object), &id); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool contains(uintptr_t ctx, uintptr_t object, int32_t& id, std::string& error) {
#ifdef MT2MP_BOSS_SAVED_RUNTIME_TEST
    if (test_link) return test_link(ctx, object, id, error);
#endif
    bool present = false; id = 0; const auto code = contains_call(ctx, object, id, present);
    if (code || (present ? id <= 0 : id != 0)) return q::fail(error, "Boss SaveContext ContainsObject differs exception=" + std::to_string(code));
    return true;
}
#ifndef MT2MP_BOSS_SAVED_RUNTIME_TEST
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
    return actual == ctx || q::fail(error, "Boss LoadContext is not active Mode context");
}
Load* top(std::string& error) {
    const auto thread = load_stack.find(GetCurrentThreadId());
    if (thread == load_stack.end()) { reject(error, "Boss event outside its owned property frame"); return nullptr; }
    const auto frame = loads.find(thread->second);
    if (frame == loads.end()) { reject(error, "Boss property frame missing"); return nullptr; }
    return &frame->second;
}
bool retain(q::Pool value, std::string& error) {
    if (observed.size() >= 4096 || !observed.emplace(value.manager, std::move(value)).second)
        return reject(error, "Boss property array duplicate or observation budget exceeded");
    return true;
}
bool seal_world(const opened::CurrentWorld& world,bool first,std::string& error){
    if(!healthy(error)||!saves.empty()||!loads.empty()||!load_stack.empty())return reject(error,error.empty()?"Boss snapshot has unfinished original property scopes":error);
    const auto map=uintptr_t(world.map),root=map+0x1c0;
    q::Bounds regions;if(!map||!q::typed(memory,image,map,0x1742860,error)||!q::typed(memory,image,map+0x230,0x16f04e0,error)||
       !q::bounds(memory,map+0x230,4096,regions,error))return reject(error,error);
    // Root is the embedded Shard object itself, not its first vtable word.
    if(!q::typed(memory,image,root,0x174ad20,error))return reject(error,error);
    struct Scene {std::string key;uintptr_t shard=0,region=0;int32_t region_id=-1,exterior_slot=-1,instance=-1;bool dungeon=false;};
    std::vector<Scene> scenes;std::set<int32_t> ids;
    for(int32_t i=0;i<regions.count;++i){uintptr_t region=0,shard=0;int32_t id=-1;
        if(!q::at(memory,regions.data,size_t(i)*8,region,error)||!q::typed(memory,image,region,0x1787930,error)||!q::at(memory,region,0x70,shard,error)||!q::at(memory,region,0x834,id,error))return reject(error,error);
        if(shard!=root||id<0||!ids.insert(id).second)return reject(error,"Boss saved Root Region ownership/ID differs");
        scenes.push_back({"world",root,region,id,-1,-1,false});}
    if(scenes.empty()||!q::unchanged(memory,map+0x230,4096,regions,error))return reject(error,error.empty()?"Boss native saved Root Regions absent":error);
    if(!scene_census)return reject(error,"Boss snapshot lacks explicit complete Dungeon scene classification");
    if(!scene_census->snapshot_census||scene_census->world.uuid!=uuid||scene_census->world.sha256!=sha||!same_world(scene_census->world,world))return reject(error,"Boss Dungeon classification snapshot/world lifetime differs");
    for(const auto& scene:scene_census->pools)scenes.push_back({scene.scene.owner.key,scene.scene.owner.shard,scene.scene.owner.region,scene.scene.exterior_region,scene.scene.exterior_slot,scene.scene.owner.instance_index,true});
    std::map<uintptr_t,q::SavedPool> ready;std::set<std::string> keys;std::set<uintptr_t> objects;std::set<uint64_t> uids;
    std::vector<std::pair<std::string,boss_identity::Entry>> restored;attachment::Manifest records;records.present=true;
    for(const auto& scene:scenes){uintptr_t manager=0,owner=0,region_shard=0;
        if(!q::at(memory,scene.region,0x990,manager,error)||!q::typed(memory,image,manager,0x16fa780,error)||!q::at(memory,manager,0x68,owner,error)||!q::at(memory,scene.region,0x70,region_shard,error))return reject(error,error);
        if(owner!=scene.region||region_shard!=scene.shard||ready.contains(manager))return reject(error,"Boss snapshot current scene/manager reciprocity repeats or differs");
        const auto found=observed.find(manager);if(found==observed.end())return reject(error,"Boss native property unobserved in scene="+scene.key);
        auto links=[&](uintptr_t object,int32_t& link,std::string& cause){for(const auto& row:found->second.records)if(row.pointer==object){link=row.link_id;return true;}return q::fail(cause,"Boss current member absent from original serialized record");};
        q::Pool actual;if(!q::observe(memory,image,manager,links,actual,error)||actual!=found->second)return reject(error,error.empty()?"Boss native array changed before snapshot seal":error);
        boss_identity::Scope scope{first?uuid:world.uuid,scene.key,world.world_epoch,world.generation,root,scene.shard,scene.region,manager};
        for(const auto& row:actual.records){uintptr_t actor_region=0,actor_shard=0,registered=0;
            if(!q::at(memory,row.pointer,0x178,actor_region,error)||!q::at(memory,row.pointer,0x170,actor_shard,error))return reject(error,error);
            if(actor_region!=scene.region||actor_shard!=scene.shard||!objects.insert(row.pointer).second||!uids.insert(row.uid).second)return reject(error,"Boss original snapshot member aliases another scene/full UID");
            const auto exception=exact_uid(row.uid,registered);if(exception||registered!=row.pointer)return reject(error,"Boss original snapshot full UID lookup differs SEH="+std::to_string(exception));
            const auto suffix=scene.dungeon?snapshot_boss_key::scene_suffix(scene.region_id,scene.exterior_slot,scene.instance,row.child):snapshot_boss_key::root_suffix(scene.region_id,row.child);
            const auto record=sha+":world:boss:"+(row.link_id>0?std::to_string(row.link_id):suffix);if(!snapshot_boss_key::valid(record)||!keys.insert(record).second)return reject(error,"Boss exact saved record key duplicates or has invalid domain");
            std::string canonical;if(host_active){if(!boss_identity::registry.capture_key(scope,row.pointer,row.uid,record,canonical,error))return reject(error,error);}
            else if(prepared_present){const auto prior=std::find_if(prepared.rows.begin(),prepared.rows.end(),[&](const auto& saved){return saved.record==record;});if(prior==prepared.rows.end())return reject(error,"Boss snapshot aliases omit an actual original loaded record");canonical=prior->key;}
            else canonical=record;
            records.rows.push_back({record,canonical});restored.emplace_back(canonical,boss_identity::Entry{scope,row.pointer,row.uid,true,false});}
        ready.emplace(manager,q::SavedPool{scene.region_id,std::move(actual)});
    }
    if(ready.size()!=observed.size())return reject(error,"Boss original serialization contains a foreign manager");
    std::sort(records.rows.begin(),records.rows.end(),[](const auto& a,const auto& b){return a.record<b.record;});
    if(!attachment::valid(records,sha,error))return reject(error,error);
    if(!host_active&&prepared_present&&records.rows!=prepared.rows)return reject(error,"Boss snapshot complete original record set differs from alias manifest");
    if(world.generation){opened::CurrentWorld after;if(!native_open_runtime::read_current_world(after,error)||!same_world(world,after)||after.uuid!=world.uuid)return reject(error,error.empty()?"Boss native world changed before atomic identity publication":error);
        boss_identity::Scope scope;scope.snapshot=world.uuid;scope.epoch=world.world_epoch;scope.generation=world.generation;scope.root=root;
        if(!boss_identity::registry.restore(scope,restored,error))return reject(error,error);}
    captured=std::move(records);origin=world;pools=std::move(ready);observed.clear();scene_census.reset();
    LOG_INFO("[SavedBoss] Native snapshot Bosses bound uuid={} sha={} managers={}",uuid,sha,pools.size());error.clear();return true;
}

}
bool stage_scenes(dungeon_scene_identity_runtime::Census census,std::string& error){
    std::lock_guard lock(gate);
    if((!host_active&&!client_active)||scene_census||GetCurrentThreadId()!=main_thread||!saves.empty()||!loads.empty()||!load_stack.empty())
        return reject(error,"Dungeon Boss scene staging requires its idle active native snapshot observer");
    if(!healthy(error)||!census.snapshot_census||census.world.uuid!=uuid)return reject(error,error.empty()?"Dungeon Boss staged owned snapshot UUID/census differs":error);
    scene_census=std::move(census);error.clear();return true;
}
bool initialize(std::string& error) {
    std::lock_guard lock(gate);
    if (!native::ready()) return q::fail(error, "Boss identity exact native resolver unavailable");
    image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const std::array<unsigned char,16> save{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x78};
    const std::array<unsigned char,16> load{0x48,0x83,0xec,0x28,0x48,0x03,0x51,0x38,0x4c,0x8b,0x12,0x48,0x89,0xd1,0x4c,0x89};
    auto guard = [&](uintptr_t rva, const auto& bytes) {
        MEMORY_BASIC_INFORMATION page{}; std::vector<unsigned char> actual(bytes.size());
        if (!VirtualQuery(reinterpret_cast<void*>(image + rva), &page, sizeof page) || page.Type != MEM_IMAGE ||
            page.AllocationBase != reinterpret_cast<void*>(image) || page.Protect != PAGE_EXECUTE_READ ||
            !memory.read(image + rva, actual.data(), actual.size(), error) || !std::equal(actual.begin(), actual.end(), bytes.begin()))
            return q::fail(error, "Boss property original entry guard differs rva=" + std::to_string(rva));
        return true;
    };
    if (!guard(q::save_property_rva, save) || !guard(q::load_property_rva, load)) { image = 0; return false; }
    error.clear(); return true;
}
void* entrypoint(bool save) { return image ? reinterpret_cast<void*>(image + (save ? q::save_property_rva : q::load_property_rva)) : nullptr; }
bool confirm_hooks(std::string& error) { std::lock_guard lock(gate); if (!image) return q::fail(error, "Boss property resolver absent"); hooks = true; error.clear(); return true; }
bool arm_host(const std::string& id, const CaptureWorld& capture, std::string& error) {
    std::lock_guard lock(gate);
    if (!hooks || host_active || client_active || !saves.empty() || !loads.empty()) return q::fail(error, "Boss snapshot observer busy/uninstalled");
    if (!capture.game || !capture.mode || !capture.state || !capture.world || !capture.map)
        return q::fail(error, "Boss Host capture requires complete BuildingCapture world witness");
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
            return q::fail(error, "Boss Host loaded origin differs from BuildingCapture world witness");
        world = std::move(loaded);
    }
    reset(id, ""); origin = std::move(world);
    host_active = true; error.clear(); return true;
}
bool seal_host(const std::string& id, const std::string& digest, const CaptureWorld& capture, std::string& error) {
    std::lock_guard lock(gate);
    if (!host_active || id != uuid || GetCurrentThreadId() != main_thread) return reject(error, "Boss Host snapshot seal lifetime differs");
    if (capture.game != origin.game || capture.mode != origin.mode || capture.state != origin.state ||
        capture.world != origin.world || capture.map != origin.map)
        return reject(error, "Boss Host BuildingCapture world witness changed");
    if (origin.generation) {
        opened::CurrentWorld loaded;
        if (!native_open_runtime::read_current_world(loaded, error) || !same_world(loaded, origin))
            return reject(error, "Boss Host completed origin changed during capture: " + error);
    }
    sha = digest; const bool confirmed = seal_world(origin, !origin.generation, error); host_active = false; return confirmed;
}
bool arm_client(const std::string& id, const std::string& digest, std::string& error) {
    std::lock_guard lock(gate); const auto witness = opened::status();
    if (!hooks || host_active || client_active || !saves.empty() || !loads.empty() || witness.state != "pinned" ||
        !witness.held || witness.uuid != id || witness.sha256 != digest || !witness.generation)
        return q::fail(error, "Boss Client arm lacks idle pinned exact NativeOpen receipt");
    if(prepared_uuid!=id||prepared_sha!=digest||prepared_bytes!=witness.bytes)
        return q::fail(error,"Boss load lacks exact prepared companion decision");
    // NativeOpen preparation precedes retirement of the previous world.
    // Arm this observer only after that retirement, alongside the other load observers.
    reset(id, digest); generation = witness.generation; client_active = true; error.clear(); return true;
}
bool seal_client(std::string& error) {
    std::lock_guard lock(gate);
    if (!client_active || GetCurrentThreadId() != main_thread) return reject(error, "Boss Client seal lifetime differs");
    opened::CurrentWorld world;
    if (!opened::read_completed_world(generation, world, error) || world.uuid != uuid || world.sha256 != sha)
        return reject(error, "Boss Client completed native snapshot differs: " + error);
    const bool confirmed = seal_world(world, false, error); client_active = false;
    return confirmed;
}
void cancel(const std::string& cause) { std::lock_guard lock(gate); host_active = false; client_active = false; failure = cause; saves.clear(); loads.clear(); load_stack.clear(); observed.clear(); pools.clear(); scene_census.reset(); }
Ticket save_before(const void* property, void* manager, void* writer, void* ctx) noexcept try {
    if (!host_active) return 0;
    Owner owner; bool own = false; std::string error;
    const bool checked = building_capture_identity::stream_owner(writer, owner, own, error);
    std::lock_guard lock(gate);
    if (!host_active || !healthy(error)) return 0;
    if (!checked) { reject(error, "Boss owned snapshot writer: " + error); return 0; }
    if (!own) return 0;
    if (owner.capture_uuid != uuid || !context(reinterpret_cast<uintptr_t>(ctx), error)) { reject(error, "Boss writer UUID/context differs: " + error); return 0; }
    auto links = [&](uintptr_t object, int32_t& id, std::string& cause) { return contains(reinterpret_cast<uintptr_t>(ctx), object, id, cause); };
    Save value; value.owner = owner; value.thread = GetCurrentThreadId();
    if (!q::begin_save(memory, image, reinterpret_cast<uintptr_t>(property), reinterpret_cast<uintptr_t>(manager),
        reinterpret_cast<uintptr_t>(writer), reinterpret_cast<uintptr_t>(ctx), links, value.frame, error)) { reject(error, error); return 0; }
    if (saves.size() >= 256 || next_ticket == UINT64_MAX) { reject(error, "Boss save frame budget/sequence exceeded"); return 0; }
    const auto ticket = ++next_ticket; saves.emplace(ticket, std::move(value)); return ticket;
} catch (const std::exception& e) { caught(e.what()); return 0; } catch (...) { caught("non-standard save before exception"); return 0; }
void save_after(Ticket ticket, const void* property, void* manager, void* writer, void* ctx, bool result) noexcept try {
    if (!ticket) return;
    Owner owner; bool own = false; std::string error;
    const bool checked = building_capture_identity::stream_owner(writer, owner, own, error);
    std::lock_guard lock(gate); if (!healthy(error)) return;
    const auto found = saves.find(ticket);
    if (!checked || !own || found == saves.end()) { reject(error, "Boss save return/file owner absent: " + error); return; }
    const auto& saved = found->second;
    if (saved.thread != GetCurrentThreadId() || saved.owner.capture_uuid != owner.capture_uuid ||
        saved.owner.stream != owner.stream || saved.owner.consumer != owner.consumer || saved.owner.file != owner.file ||
        (saved.owner.task && saved.owner.task != owner.task)) { reject(error, "Boss save writer/thread lifetime changed"); return; }
    auto links = [&](uintptr_t object, int32_t& id, std::string& cause) { return contains(reinterpret_cast<uintptr_t>(ctx), object, id, cause); };
    q::Pool value;
    if (!q::finish_save(memory, image, saved.frame, reinterpret_cast<uintptr_t>(property), reinterpret_cast<uintptr_t>(manager),
        reinterpret_cast<uintptr_t>(writer), reinterpret_cast<uintptr_t>(ctx), result, links, value, error) || !retain(std::move(value), error)) { reject(error, error); return; }
    saves.erase(found);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard save after exception"); }
Ticket load_before(const void* property, void* manager, void* reader, void* ctx) noexcept try {
    if (!client_active) return 0;
    std::lock_guard lock(gate); std::string error;
    if (!healthy(error)) return 0;
    const auto r = reinterpret_cast<uintptr_t>(reader), c = reinterpret_cast<uintptr_t>(ctx);
    if (!live_reader(r, c, error) || !context(c, error)) { reject(error, "Boss load reader/context rejected: " + error); return 0; }
    Load value; value.property = reinterpret_cast<uintptr_t>(property); value.manager = reinterpret_cast<uintptr_t>(manager); value.thread = GetCurrentThreadId();
    if (load_stack.contains(value.thread) || loads.size() >= 256 || next_ticket == UINT64_MAX ||
        !q::begin_load(memory, image, value.property, value.manager, r, c, value.frame, error)) { reject(error, "Boss load frame rejected: " + error); return 0; }
    const auto ticket = ++next_ticket; load_stack.emplace(value.thread, ticket); loads.emplace(ticket, std::move(value)); return ticket;
} catch (const std::exception& e) { caught(e.what()); return 0; } catch (...) { caught("non-standard load before exception"); return 0; }
void children_before(uintptr_t caller, void* reader) noexcept try {
    if (!client_active || caller != q::load_children_return_rva) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (frame->child_pending || frame->frame.reader != reinterpret_cast<uintptr_t>(reader) ||
        !live_reader(frame->frame.reader, frame->frame.context, error) || !q::child_header(memory, frame->frame.reader, frame->child, error)) { reject(error, "Boss child header rejected: " + error); return; }
    frame->child_pending = true;
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard children before exception"); }
void children_after(uintptr_t caller, void* reader, int32_t count) noexcept try {
    if (!client_active || caller != q::load_children_return_rva) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (!frame->child_pending || !live_reader(frame->frame.reader, frame->frame.context, error) ||
        !frame->frame.children(caller, reinterpret_cast<uintptr_t>(reader), frame->child.label, frame->child.count, count, error)) { reject(error, "Boss BeginChildren return rejected: " + error); return; }
    frame->child_pending = false;
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard children after exception"); }
void registered(uintptr_t caller, void* ctx, int32_t id, void* object) noexcept try {
    if (!client_active || (caller != q::load_link_return_rva && caller != q::load_inline_return_rva)) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (!live_reader(frame->frame.reader, frame->frame.context, error)) { reject(error, error); return; }
    if (id > 0) {
#ifndef MT2MP_BOSS_SAVED_RUNTIME_TEST
        uintptr_t actual = 0; const auto code = lookup_call(reinterpret_cast<uintptr_t>(ctx), id, actual);
        if (code || actual != reinterpret_cast<uintptr_t>(object)) { reject(error, "Boss registered ID lookup differs exception=" + std::to_string(code)); return; }
#endif
    }
    if (!q::registered(memory, image, frame->frame, caller, reinterpret_cast<uintptr_t>(ctx), id, reinterpret_cast<uintptr_t>(object), error)) reject(error, error);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard registered exception"); }
void load_after(Ticket ticket, const void* property, void* manager, void* reader, void* ctx, bool result) noexcept try {
    if (!ticket) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    const auto found = loads.find(ticket);
    if (!result || found == loads.end() || frame != &found->second || frame->child_pending ||
        frame->property != reinterpret_cast<uintptr_t>(property) || frame->manager != reinterpret_cast<uintptr_t>(manager) ||
        frame->frame.reader != reinterpret_cast<uintptr_t>(reader) || frame->frame.context != reinterpret_cast<uintptr_t>(ctx) ||
        !live_reader(frame->frame.reader, frame->frame.context, error)) { reject(error, "Boss original LoadStream result/lifetime differs: " + error); return; }
    q::Pool value;
    if (!frame->frame.finish(memory, image, value, error) || !retain(std::move(value), error)) { reject(error, error); return; }
    load_stack.erase(frame->thread); loads.erase(found);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard load after exception"); }
void original_failed(Ticket ticket, const std::string& cause) noexcept try {
    if (!ticket) return; std::lock_guard lock(gate); std::string error; reject(error, "original Boss property failed: " + cause);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("original failure reporting exception"); }
bool prepare_aliases(const std::string& id,const std::string& digest,uint64_t bytes,const attachment::Manifest& aliases,std::string& error){
    std::lock_guard lock(gate);if(host_active||client_active)return q::fail(error,"Boss aliases require an idle original snapshot observer");
    if(!boss_identity::uuid(id)||digest.size()!=64||digest.find_first_not_of("0123456789abcdef")!=std::string::npos||!bytes||!attachment::valid(aliases,digest,error))return q::fail(error,error.empty()?"Boss prepared snapshot UUID/SHA/length invalid":error);
    prepared=aliases;prepared_uuid=id;prepared_sha=digest;prepared_bytes=bytes;prepared_present=aliases.present;error.clear();return true;
}
bool aliases(const std::string& id,const std::string& digest,attachment::Manifest& out,std::string& error){
    std::lock_guard lock(gate);out={};if(!healthy(error))return false;
    if(host_active||client_active||id!=uuid||digest!=sha||!captured.present||pools.empty())return q::fail(error,"Boss original snapshot aliases are not sealed for this exact native file");
    if(!attachment::valid(captured,digest,error))return false;out=captured;error.clear();return true;
}
}

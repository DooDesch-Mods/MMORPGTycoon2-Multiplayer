#include "party_saved_identity_runtime.h"
#include <algorithm>
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
#include "party_identity.h"
#include "party_snapshot_alias.h"
#include "mt2mp/net/snapshot_buffer.h"
#include <fstream>
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "building_capture_identity.h"
#include "root_saved_building.h"
#ifndef MT2MP_PARTY_SAVED_RUNTIME_TEST
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

namespace mt2mp::game::party_saved_identity_runtime {
namespace {
namespace q = party_saved_identity;
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
namespace attachment=party_snapshot_alias;
attachment::Manifest prepared, captured;
std::string prepared_uuid;
bool prepared_present=false;
uint64_t prepared_bytes=0;
std::string prepared_sha;
DWORD exact_uid(uint64_t uid,uintptr_t& out){
    __try{out=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

#ifdef MT2MP_PARTY_SAVED_RUNTIME_TEST
using TestRead = bool(*)(uintptr_t, void*, size_t, std::string&);
using TestLink = bool(*)(uintptr_t, uintptr_t, int32_t&, std::string&);
TestRead test_read = nullptr;
TestLink test_link = nullptr;
#endif
struct Memory {
    bool copy(uintptr_t p, void* out, size_t bytes, std::string& error) { return read(p, out, bytes, error); }
    bool read(uintptr_t p, void* out, size_t bytes, std::string& error) {
#ifdef MT2MP_PARTY_SAVED_RUNTIME_TEST
        if (test_read) return test_read(p, out, bytes, error);
#endif
        return root_saved_building::bytes(reinterpret_cast<void*>(p), 0, out, bytes, error);
    }
} memory;
bool reject(std::string& error, const std::string& cause) {
    if (failure.empty()) { failure = cause; LOG_ERROR("[SavedParty] snapshot={} cause={}; original serialization unchanged", uuid, cause); }
    error = failure; pools.clear(); return false;
}
void caught(const char* cause) noexcept {
    emergency = true; OutputDebugStringA("ERROR SavedParty observer exception: ");
    OutputDebugStringA(cause); OutputDebugStringA("; target bindings blocked, original unchanged\n");
}
bool healthy(std::string& error) {
    if (emergency) return reject(error, "Party identity observer exception; original diagnostic retained");
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
    if (!actual || (bound_context && bound_context != actual)) return reject(error, "Party property save/load context differs");
    bound_context = actual; return true;
}
DWORD contains_call(uintptr_t ctx, uintptr_t object, int32_t& id, bool& present) {
    __try { present = reinterpret_cast<bool(*)(void*, const void*, int32_t*)>(image + 0x89e690)(
        reinterpret_cast<void*>(ctx), reinterpret_cast<void*>(object), &id); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool contains(uintptr_t ctx, uintptr_t object, int32_t& id, std::string& error) {
#ifdef MT2MP_PARTY_SAVED_RUNTIME_TEST
    if (test_link) return test_link(ctx, object, id, error);
#endif
    bool present = false; id = 0; const auto code = contains_call(ctx, object, id, present);
    if (code || (present ? id <= 0 : id != 0)) return q::fail(error, "Party SaveContext ContainsObject differs exception=" + std::to_string(code));
    return true;
}
#ifndef MT2MP_PARTY_SAVED_RUNTIME_TEST
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
    return actual == ctx || q::fail(error, "Party LoadContext is not active Mode context");
}
Load* top(std::string& error) {
    const auto thread = load_stack.find(GetCurrentThreadId());
    if (thread == load_stack.end()) { reject(error, "Party event outside its owned property frame"); return nullptr; }
    const auto frame = loads.find(thread->second);
    if (frame == loads.end()) { reject(error, "Party property frame missing"); return nullptr; }
    return &frame->second;
}
bool retain(q::Pool value, std::string& error) {
    if (observed.size() >= 4096 || !observed.emplace(value.manager, std::move(value)).second)
        return reject(error, "Party property array duplicate or observation budget exceeded");
    return true;
}
bool seal_world(const opened::CurrentWorld& world,bool first,std::string& error){
    if(!healthy(error)||!saves.empty()||!loads.empty()||!load_stack.empty())return reject(error,error.empty()?"Party snapshot has unfinished original property scopes":error);
    const auto map=uintptr_t(world.map),root=map+0x1c0;
    q::Bounds regions;if(!map||!q::typed(memory,image,map,0x1742860,error)||!q::typed(memory,image,map+0x230,0x16f04e0,error)||
       !q::bounds(memory,map+0x230,4096,regions,error))return reject(error,error);
    // Root is the embedded Shard object itself, not its first vtable word.
    if(!q::typed(memory,image,root,0x174ad20,error))return reject(error,error);
    struct Scene {std::string key;uintptr_t shard=0,region=0;int32_t region_id=-1,exterior_slot=-1,instance=-1;bool dungeon=false;};
    std::vector<Scene> scenes;std::set<int32_t> ids;
    for(int32_t i=0;i<regions.count;++i){uintptr_t region=0,shard=0;int32_t id=-1;
        if(!q::at(memory,regions.data,size_t(i)*8,region,error)||!q::typed(memory,image,region,0x1787930,error)||!q::at(memory,region,0x70,shard,error)||!q::at(memory,region,0x834,id,error))return reject(error,error);
        if(shard!=root||id<0||!ids.insert(id).second)return reject(error,"Party saved Root Region ownership/ID differs");
        scenes.push_back({"world",root,region,id,-1,-1,false});}
    if(scenes.empty()||!q::unchanged(memory,map+0x230,4096,regions,error))return reject(error,error.empty()?"Party native saved Root Regions absent":error);
    if(!scene_census)return reject(error,"Party snapshot lacks explicit complete Dungeon scene classification");
    if(!scene_census->snapshot_census||scene_census->world.uuid!=uuid||scene_census->world.sha256!=sha||!same_world(scene_census->world,world))return reject(error,"Party Dungeon classification snapshot/world lifetime differs");
    for(const auto& scene:scene_census->pools)scenes.push_back({scene.scene.owner.key,scene.scene.owner.shard,scene.scene.owner.region,scene.scene.exterior_region,scene.scene.exterior_slot,scene.scene.owner.instance_index,true});
    std::map<uintptr_t,q::SavedPool> ready;std::set<std::string> keys;std::set<uintptr_t> objects;std::set<uint64_t> uids;
    std::vector<std::pair<std::string,party_identity::Entry>> restored;attachment::Manifest records;records.present=true;
    for(const auto& scene:scenes){uintptr_t manager=0,owner=0,region_shard=0;
        if(!q::at(memory,scene.region,0x9d0,manager,error)||!q::typed(memory,image,manager,0x17018d0,error)||!q::at(memory,manager,0xa8,owner,error)||!q::at(memory,scene.region,0x70,region_shard,error))return reject(error,error);
        if(owner!=scene.region||region_shard!=scene.shard||ready.contains(manager))return reject(error,"Party snapshot current scene/manager reciprocity repeats or differs");
        const auto found=observed.find(manager);if(found==observed.end())return reject(error,"Party native property unobserved in scene="+scene.key);
        auto links=[&](uintptr_t object,int32_t& link,std::string& cause){for(const auto& row:found->second.records)if(row.pointer==object){link=row.link_id;return true;}return q::fail(cause,"Party current member absent from original serialized record");};
        q::Pool actual;if(!q::observe(memory,image,manager,links,actual,error)||actual!=found->second)return reject(error,error.empty()?"Party native array changed before snapshot seal":error);
        party_identity::Scope scope{first?uuid:world.uuid,world.world_epoch,world.generation,root};
        for(const auto& row:actual.records){uintptr_t actor_region=0,actor_shard=0,registered=0;
            if(!q::at(memory,row.pointer,0x178,actor_region,error)||!q::at(memory,row.pointer,0x170,actor_shard,error))return reject(error,error);
            if(!actor_region||!actor_shard||!objects.insert(row.pointer).second||!uids.insert(row.uid).second)return reject(error,"Party original snapshot member aliases another scene/full UID");
            const auto exception=exact_uid(row.uid,registered);if(exception||registered!=row.pointer)return reject(error,"Party original snapshot full UID lookup differs SEH="+std::to_string(exception));
            const auto suffix=scene.dungeon?snapshot_party_key::scene_suffix(scene.region_id,scene.exterior_slot,scene.instance,row.child):snapshot_party_key::root_suffix(scene.region_id,row.child);
            const auto record=sha+":world:party:"+(row.link_id>0?std::to_string(row.link_id):suffix);if(!snapshot_party_key::valid(record)||!keys.insert(record).second)return reject(error,"Party exact saved record key duplicates or has invalid domain");
            std::string canonical;if(host_active){if(!party_identity::registry.capture_key(scope,row.pointer,row.uid,record,canonical,error))return reject(error,error);}
            else if(prepared_present){const auto prior=std::find_if(prepared.rows.begin(),prepared.rows.end(),[&](const auto& saved){return saved.record==record;});if(prior==prepared.rows.end())return reject(error,"Party snapshot aliases omit an actual original loaded record");canonical=prior->key;}
            else canonical=record;
            records.rows.push_back({record,canonical});restored.emplace_back(canonical,party_identity::Entry{scope,row.pointer,row.uid,false});}
        ready.emplace(manager,q::SavedPool{scene.region_id,std::move(actual)});
    }
    if(ready.size()!=observed.size())return reject(error,"Party original serialization contains a foreign manager");
    std::sort(records.rows.begin(),records.rows.end(),[](const auto& a,const auto& b){return a.record<b.record;});
    if(!attachment::valid(records,sha,error))return reject(error,error);
    if(!host_active&&prepared_present&&records.rows!=prepared.rows)return reject(error,"Party snapshot complete original record set differs from alias manifest");
    if(world.generation){opened::CurrentWorld after;if(!native_open_runtime::read_current_world(after,error)||!same_world(world,after)||after.uuid!=world.uuid)return reject(error,error.empty()?"Party native world changed before atomic identity publication":error);
        party_identity::Scope scope;scope.snapshot=world.uuid;scope.epoch=world.world_epoch;scope.generation=world.generation;scope.root=root;
        if(!party_identity::registry.restore(scope,restored,error))return reject(error,error);}
    captured=std::move(records);origin=world;pools=std::move(ready);observed.clear();scene_census.reset();
    LOG_INFO("[SavedParty] Native snapshot Partyes bound uuid={} sha={} managers={}",uuid,sha,pools.size());error.clear();return true;
}

}
bool stage_scenes(dungeon_scene_identity_runtime::Census census,std::string& error){
    std::lock_guard lock(gate);
    if((!host_active&&!client_active)||scene_census||GetCurrentThreadId()!=main_thread||!saves.empty()||!loads.empty()||!load_stack.empty())
        return reject(error,"Dungeon Party scene staging requires its idle active native snapshot observer");
    if(!healthy(error)||!census.snapshot_census||census.world.uuid!=uuid)return reject(error,error.empty()?"Dungeon Party staged owned snapshot UUID/census differs":error);
    scene_census=std::move(census);error.clear();return true;
}
bool initialize(std::string& error) {
    std::lock_guard lock(gate);
    if (!native::ready()) return q::fail(error, "Party identity exact native resolver unavailable");
    image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const std::array<unsigned char,16> save{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x68};
    const std::array<unsigned char,16> load{0x48,0x83,0xec,0x28,0x48,0x03,0x51,0x38,0x4c,0x8b,0x12,0x48,0x89,0xd1,0x4c,0x89};
    auto guard = [&](uintptr_t rva, const auto& bytes) {
        MEMORY_BASIC_INFORMATION page{}; std::vector<unsigned char> actual(bytes.size());
        if (!VirtualQuery(reinterpret_cast<void*>(image + rva), &page, sizeof page) || page.Type != MEM_IMAGE ||
            page.AllocationBase != reinterpret_cast<void*>(image) || page.Protect != PAGE_EXECUTE_READ ||
            !memory.read(image + rva, actual.data(), actual.size(), error) || !std::equal(actual.begin(), actual.end(), bytes.begin()))
            return q::fail(error, "Party property original entry guard differs rva=" + std::to_string(rva));
        return true;
    };
    if (!guard(q::save_property_rva, save) || !guard(q::load_property_rva, load)) { image = 0; return false; }
    error.clear(); return true;
}
void* entrypoint(bool save) { return image ? reinterpret_cast<void*>(image + (save ? q::save_property_rva : q::load_property_rva)) : nullptr; }
bool confirm_hooks(std::string& error) { std::lock_guard lock(gate); if (!image) return q::fail(error, "Party property resolver absent"); hooks = true; error.clear(); return true; }
bool arm_host(const std::string& id, const CaptureWorld& capture, std::string& error) {
    std::lock_guard lock(gate);
    if (!hooks || host_active || client_active || !saves.empty() || !loads.empty()) return q::fail(error, "Party snapshot observer busy/uninstalled");
    if (!capture.game || !capture.mode || !capture.state || !capture.world || !capture.map)
        return q::fail(error, "Party Host capture requires complete BuildingCapture world witness");
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
            return q::fail(error, "Party Host loaded origin differs from BuildingCapture world witness");
        world = std::move(loaded);
    }
    reset(id, ""); origin = std::move(world);
    host_active = true; error.clear(); return true;
}
bool seal_host(const std::string& id, const std::string& digest, const CaptureWorld& capture, std::string& error) {
    std::lock_guard lock(gate);
    if (!host_active || id != uuid || GetCurrentThreadId() != main_thread) return reject(error, "Party Host snapshot seal lifetime differs");
    if (capture.game != origin.game || capture.mode != origin.mode || capture.state != origin.state ||
        capture.world != origin.world || capture.map != origin.map)
        return reject(error, "Party Host BuildingCapture world witness changed");
    if (origin.generation) {
        opened::CurrentWorld loaded;
        if (!native_open_runtime::read_current_world(loaded, error) || !same_world(loaded, origin))
            return reject(error, "Party Host completed origin changed during capture: " + error);
    }
    sha = digest; const bool confirmed = seal_world(origin, !origin.generation, error); host_active = false; return confirmed;
}
bool arm_client(const std::string& id, const std::string& digest, std::string& error) {
    std::lock_guard lock(gate); const auto witness = opened::status();
    if (!hooks || host_active || client_active || !saves.empty() || !loads.empty() || witness.state != "pinned" ||
        !witness.held || witness.uuid != id || witness.sha256 != digest || !witness.generation)
        return q::fail(error, "Party Client arm lacks idle pinned exact NativeOpen receipt");
    if(prepared_uuid!=id||prepared_sha!=digest||prepared_bytes!=witness.bytes)
        return q::fail(error,"Party load lacks exact prepared companion decision");
    // NativeOpen preparation precedes retirement of the previous world.
    // Arm this observer only after that retirement, alongside the other load observers.
    reset(id, digest); generation = witness.generation; client_active = true; error.clear(); return true;
}
bool seal_client(std::string& error) {
    std::lock_guard lock(gate);
    if (!client_active || GetCurrentThreadId() != main_thread) return reject(error, "Party Client seal lifetime differs");
    opened::CurrentWorld world;
    if (!opened::read_completed_world(generation, world, error) || world.uuid != uuid || world.sha256 != sha)
        return reject(error, "Party Client completed native snapshot differs: " + error);
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
    if (!checked) { reject(error, "Party owned snapshot writer: " + error); return 0; }
    if (!own) return 0;
    if (owner.capture_uuid != uuid || !context(reinterpret_cast<uintptr_t>(ctx), error)) { reject(error, "Party writer UUID/context differs: " + error); return 0; }
    auto links = [&](uintptr_t object, int32_t& id, std::string& cause) { return contains(reinterpret_cast<uintptr_t>(ctx), object, id, cause); };
    Save value; value.owner = owner; value.thread = GetCurrentThreadId();
    if (!q::begin_save(memory, image, reinterpret_cast<uintptr_t>(property), reinterpret_cast<uintptr_t>(manager),
        reinterpret_cast<uintptr_t>(writer), reinterpret_cast<uintptr_t>(ctx), links, value.frame, error)) { reject(error, error); return 0; }
    if (saves.size() >= 256 || next_ticket == UINT64_MAX) { reject(error, "Party save frame budget/sequence exceeded"); return 0; }
    const auto ticket = ++next_ticket; saves.emplace(ticket, std::move(value)); return ticket;
} catch (const std::exception& e) { caught(e.what()); return 0; } catch (...) { caught("non-standard save before exception"); return 0; }
void save_after(Ticket ticket, const void* property, void* manager, void* writer, void* ctx, bool result) noexcept try {
    if (!ticket) return;
    Owner owner; bool own = false; std::string error;
    const bool checked = building_capture_identity::stream_owner(writer, owner, own, error);
    std::lock_guard lock(gate); if (!healthy(error)) return;
    const auto found = saves.find(ticket);
    if (!checked || !own || found == saves.end()) { reject(error, "Party save return/file owner absent: " + error); return; }
    const auto& saved = found->second;
    if (saved.thread != GetCurrentThreadId() || saved.owner.capture_uuid != owner.capture_uuid ||
        saved.owner.stream != owner.stream || saved.owner.consumer != owner.consumer || saved.owner.file != owner.file ||
        (saved.owner.task && saved.owner.task != owner.task)) { reject(error, "Party save writer/thread lifetime changed"); return; }
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
    if (!live_reader(r, c, error) || !context(c, error)) { reject(error, "Party load reader/context rejected: " + error); return 0; }
    Load value; value.property = reinterpret_cast<uintptr_t>(property); value.manager = reinterpret_cast<uintptr_t>(manager); value.thread = GetCurrentThreadId();
    if (load_stack.contains(value.thread) || loads.size() >= 256 || next_ticket == UINT64_MAX ||
        !q::begin_load(memory, image, value.property, value.manager, r, c, value.frame, error)) { reject(error, "Party load frame rejected: " + error); return 0; }
    const auto ticket = ++next_ticket; load_stack.emplace(value.thread, ticket); loads.emplace(ticket, std::move(value)); return ticket;
} catch (const std::exception& e) { caught(e.what()); return 0; } catch (...) { caught("non-standard load before exception"); return 0; }
void children_before(uintptr_t caller, void* reader) noexcept try {
    if (!client_active || caller != q::load_children_return_rva) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (frame->child_pending || frame->frame.reader != reinterpret_cast<uintptr_t>(reader) ||
        !live_reader(frame->frame.reader, frame->frame.context, error) || !q::child_header(memory, frame->frame.reader, frame->child, error)) { reject(error, "Party child header rejected: " + error); return; }
    frame->child_pending = true;
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard children before exception"); }
void children_after(uintptr_t caller, void* reader, int32_t count) noexcept try {
    if (!client_active || caller != q::load_children_return_rva) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (!frame->child_pending || !live_reader(frame->frame.reader, frame->frame.context, error) ||
        !frame->frame.children(caller, reinterpret_cast<uintptr_t>(reader), frame->child.label, frame->child.count, count, error)) { reject(error, "Party BeginChildren return rejected: " + error); return; }
    frame->child_pending = false;
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard children after exception"); }
void registered(uintptr_t caller, void* ctx, int32_t id, void* object) noexcept try {
    if (!client_active || (caller != q::load_link_return_rva && caller != q::load_inline_return_rva)) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (!live_reader(frame->frame.reader, frame->frame.context, error)) { reject(error, error); return; }
    if (id > 0) {
#ifndef MT2MP_PARTY_SAVED_RUNTIME_TEST
        uintptr_t actual = 0; const auto code = lookup_call(reinterpret_cast<uintptr_t>(ctx), id, actual);
        if (code || actual != reinterpret_cast<uintptr_t>(object)) { reject(error, "Party registered ID lookup differs exception=" + std::to_string(code)); return; }
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
        !live_reader(frame->frame.reader, frame->frame.context, error)) { reject(error, "Party original LoadStream result/lifetime differs: " + error); return; }
    q::Pool value;
    if (!frame->frame.finish(memory, image, value, error) || !retain(std::move(value), error)) { reject(error, error); return; }
    load_stack.erase(frame->thread); loads.erase(found);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard load after exception"); }
void original_failed(Ticket ticket, const std::string& cause) noexcept try {
    if (!ticket) return; std::lock_guard lock(gate); std::string error; reject(error, "original Party property failed: " + cause);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("original failure reporting exception"); }
bool prepare_aliases(const std::string& id,const std::string& digest,uint64_t bytes,const attachment::Manifest& aliases,std::string& error){
    std::lock_guard lock(gate);if(host_active||client_active)return q::fail(error,"Party aliases require an idle original snapshot observer");
    if(!party_identity::uuid(id)||digest.size()!=64||digest.find_first_not_of("0123456789abcdef")!=std::string::npos||!bytes||!attachment::valid(aliases,digest,error))return q::fail(error,error.empty()?"Party prepared snapshot UUID/SHA/length invalid":error);
    prepared=aliases;prepared_uuid=id;prepared_sha=digest;prepared_bytes=bytes;prepared_present=aliases.present;error.clear();return true;
}
bool aliases(const std::string& id,const std::string& digest,attachment::Manifest& out,std::string& error){
    std::lock_guard lock(gate);out={};if(!healthy(error))return false;
    if(host_active||client_active||id!=uuid||digest!=sha||!captured.present||pools.empty())return q::fail(error,"Party original snapshot aliases are not sealed for this exact native file");
    if(!attachment::valid(captured,digest,error))return false;out=captured;error.clear();return true;
}
namespace {
bool collect_current_raw(NativeCensus& out,std::string& error){
    out={};
    if(!healthy(error)||host_active||client_active||GetCurrentThreadId()!=main_thread)
        return q::fail(error,error.empty()?"Party current aliases require idle original observer on native Mainthread":error);
    opened::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,error))return false;
    if(!same_world(world,origin)||world.uuid!=origin.uuid||!world.generation||!world.world_epoch)
        return q::fail(error,"Party current aliases have no sealed completed world lifetime");
    const party_identity::Scope current{world.uuid,world.world_epoch,world.generation,uintptr_t(world.map)+0x1c0};
    uintptr_t actual_image=0;uint32_t thread=0;std::string digest;
    if(!native::read_resolver_receipt(actual_image,thread,digest,error))return false;
    if(actual_image!=image||thread!=main_thread||digest!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")
        return q::fail(error,"Party current aliases require exact executable image/Mainthread");
    q::Bounds regions;const uintptr_t map=uintptr_t(world.map);
    std::vector<std::pair<uintptr_t,uintptr_t>> owners;
    {live_host_state::apply_timing::PageReadScope region_pages;
    if(!q::typed(memory,image,map,0x1742860,error)||!q::typed(memory,image,map+0x230,0x16f04e0,error)||
       !q::bounds(memory,map+0x230,4096,regions,error))return false;
    for(int32_t i=0;i<regions.count;++i){uintptr_t region=0;
        if(!q::at(memory,regions.data,size_t(i)*sizeof(uintptr_t),region,error))return false;
        owners.emplace_back(current.root,region);
    }
    }
    dungeon_scene_identity_runtime::World scene_world;std::vector<dungeon_scene_roster::Scene> scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(scene_world,scenes,error))return false;
    if(!same_world(world,scene_world)||world.uuid!=scene_world.uuid)
        return q::fail(error,"Party current Root and Dungeon scenes belong to different worlds");
    for(const auto& scene:scenes)owners.emplace_back(scene.owner.shard,scene.owner.region);
    NativeCensus ready;ready.scope=current;ready.world=scene_world;
    std::set<uintptr_t> managers;
    std::map<uint64_t,dungeon_party_census::Member> members;
    auto find=[&](uint64_t uid,uintptr_t& found,std::string& cause){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        const auto code=exact_uid(uid,found);return !code||q::fail(cause,"Party original full UID lookup SEH="+std::to_string(code));
    };
    {live_host_state::apply_timing::PageReadScope pool_pages;
    for(const auto& [shard,region]:owners){
        dungeon_party_census::Pool pool;
        if(!dungeon_party_census::read(memory,find,image,shard,region,pool,error))return false;
        if(!managers.insert(pool.manager).second)return q::fail(error,"Party complete current owner census repeats a manager");
        ready.pools.push_back(pool);
        for(const auto& row:pool.members){
            const auto [where,inserted]=members.emplace(row.uid,row);
            if(!inserted&&where->second!=row)return q::fail(error,"Party transfer has inconsistent actual full-UID owner fields");
        }
    }
    }
    opened::CurrentWorld after;
    if(!q::unchanged(memory,map+0x230,4096,regions,error)||!native_open_runtime::read_current_world(after,error))return false;
    if(!same_world(world,after)||after.uuid!=world.uuid)return q::fail(error,"Party world changed before copied raw census publication");
    for(const auto& row:members)ready.members.push_back(row.second);
    out=std::move(ready);error.clear();return true;
}
}
bool read_current_raw(NativeCensus& out,std::string& error)try{
    out={};std::lock_guard lock(gate);return collect_current_raw(out,error);
}catch(const std::exception& e){out={};return q::fail(error,std::string("Party current raw census: ")+e.what());}
bool read_current(std::vector<Current>& out,std::string& error)try{
    out.clear();std::lock_guard lock(gate);NativeCensus raw;
    if(!collect_current_raw(raw,error))return false;
    const auto& current=raw.scope;
    std::map<uint64_t,dungeon_party_census::Member> members;
    for(const auto& member:raw.members)members.emplace(member.uid,member);
    auto find=[&](uint64_t uid,uintptr_t& found,std::string& cause){
        const auto code=exact_uid(uid,found);return !code||q::fail(cause,"Party original full UID lookup SEH="+std::to_string(code));
    };
    std::vector<std::pair<std::string,party_identity::Entry>> prior;
    if(!party_identity::registry.enumerate(current,prior,error))return false;
    auto next=party_identity::registry;
    for(const auto& [key,row]:prior)if(!row.retired&&!members.contains(row.uid)){
        uintptr_t registered=0;if(!find(row.uid,registered,error))return false;
        if(registered)return q::fail(error,"Party remains registered outside the complete current manager census");
        if(!next.retire(key,row,error))return false;
    }
    std::vector<Current> ready;
    for(const auto& [uid,row]:members){Current value;value.member=row;
        if(!next.identify(current,row.pointer,uid,value.key,error)||!next.lookup(current,value.key,value.binding,error))return false;
        ready.push_back(std::move(value));
    }
    opened::CurrentWorld after;
    if(!native_open_runtime::read_current_world(after,error))return false;
    if(!same_world(raw.world,after)||after.uuid!=raw.world.uuid)
        return q::fail(error,"Party world changed before copied alias publication");
    std::sort(ready.begin(),ready.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    party_identity::registry=std::move(next);out=std::move(ready);error.clear();return true;
}catch(const std::exception& e){out.clear();return q::fail(error,std::string("Party current alias census: ")+e.what());}
}

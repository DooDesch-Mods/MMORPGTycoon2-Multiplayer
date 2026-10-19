#include "bug_authority/runtime.h"
#include "building_capture_identity.h"
#include "mt2mp/net/snapshot_world_path.h"
#include "npc_quest_target_runtime.h"
#include "npc_saved_identity_runtime.h"
#include "party_saved_identity_runtime.h"
#include "monster_saved_identity_runtime.h"
#include "boss_saved_identity_runtime.h"
#include "dungeon_scene_identity_runtime.h"
#include "district_saved_identity_runtime.h"
#include "mt2mp/core/logger.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include "root_saved_building.h"
#include "snapshot_building_key.h"

namespace mt2mp::game::building_capture_identity {
namespace {
constexpr uintptr_t entry_rva = 0xe0e500;
constexpr size_t maximum_buildings = 4096, maximum_contexts = 8, maximum_inflight = 256, maximum_arrays = 4096;
uintptr_t image = 0;
std::mutex mutex;
Status current;
std::string filename;
Ticket sequence = 0;
struct Item {
    int32_t id = 0, root_region = -1, child = -1;
    std::string suffix;
    uint64_t uid = 0;
    void* pointer = nullptr;
    void* shard = nullptr;
    void* region = nullptr;
    bool operator==(const Item&) const = default;
};
struct WriterWitness {
    void* stream = nullptr;
    void* consumer = nullptr;
    void* file = nullptr;
    void* task = nullptr;
    bool operator==(const WriterWitness&) const = default;
};
WriterWitness bound_writer;
struct Slice {
    const void* array = nullptr;
    void* writer = nullptr;
    void* context = nullptr;
    void* data = nullptr;
    void* manager = nullptr;
    void* region = nullptr;
    void* shard = nullptr;
    uint32_t thread = 0;
    int32_t count = 0, capacity = 0;
    std::vector<Item> items;
    WriterWitness file_owner;
    bool operator==(const Slice&) const = default;
};
std::map<Ticket, Slice> pending;
std::map<void*, std::map<std::string, Item>> groups;
std::set<std::pair<void*, const void*>> arrays;
std::vector<Item> sealing;
std::vector<Binding> sealed_readbacks;
size_t seal_cursor = 0;
bool armed = false;
// Undistributed diagnostic saves must not replace the active shared NPC/Quest keys.
bool capture_snapshot_identities = true;
std::atomic<bool> emergency_failure = false;
struct WorldWitness {
    void* game = nullptr;
    void* mode = nullptr;
    void* state = nullptr;
    void* world = nullptr;
    void* map = nullptr;
    bool operator==(const WorldWitness&) const = default;
};
WorldWitness bound_world;
using Contains = bool(*)(void*, void*, int32_t&, std::string&);
using Typed = bool(*)(void*, std::string&);
using Readback = bool(*)(uint64_t, void*, native::BuildingState&, std::string&);

bool valid_uuid(const std::string& value) {
    if (value.size() != 36) return false;
    for (size_t index = 0; index < value.size(); ++index) {
        const auto c = value[index];
        if (index == 8 || index == 13 || index == 18 || index == 23) { if (c != '-') return false; }
        else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
bool valid_sha(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
bool fail(std::string& error, const std::string& cause) {
    error = cause;
    if (current.state != "failed") {
        current.state = "failed"; current.error = cause; current.bindings_confirmed = false; current.buildings.clear();
        LOG_ERROR("[BuildingCapture] capture={} cause={}; native save unchanged", current.capture_uuid, cause);
    }
    return false;
}
bool readable(const void* pointer, size_t bytes) {
    MEMORY_BASIC_INFORMATION info{};
    const auto address = reinterpret_cast<uintptr_t>(pointer);
    if (!pointer || !bytes || !VirtualQuery(pointer, &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto start = reinterpret_cast<uintptr_t>(info.BaseAddress);
    return address >= start && bytes <= info.RegionSize && address - start <= info.RegionSize - bytes;
}
bool image_range(uintptr_t rva, size_t bytes, bool executable, std::string& error) {
    MEMORY_BASIC_INFORMATION info{};
    const auto* pointer = reinterpret_cast<void*>(image + rva);
    if (!readable(pointer, bytes) || !VirtualQuery(pointer, &info, sizeof(info)) || info.Type != MEM_IMAGE ||
        reinterpret_cast<uintptr_t>(info.AllocationBase) != image) { error = "native image range differs rva=" + std::to_string(rva); return false; }
    const auto protection = info.Protect & 0xff;
    if ((executable && protection != PAGE_EXECUTE_READ) || (!executable && protection != PAGE_READONLY)) {
        error = "native image protection differs rva=" + std::to_string(rva) + " actual=" + std::to_string(protection); return false;
    }
    return true;
}
DWORD copy_guarded(const void* source, void* destination, size_t bytes) {
    __try { std::memcpy(destination, source, bytes); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
bool read_bytes(const void* object, size_t offset, void* destination, size_t bytes, std::string& error) {
    const auto address = reinterpret_cast<uintptr_t>(object);
    if (!object || address > UINTPTR_MAX - offset || !readable(reinterpret_cast<void*>(address + offset), bytes)) { error = "fixed field unreadable offset=" + std::to_string(offset); return false; }
    const auto exception = copy_guarded(reinterpret_cast<void*>(address + offset), destination, bytes);
    if (exception) { error = "fixed field read exception=" + std::to_string(exception); return false; }
    return true;
}
template<class T> bool read(const void* object, size_t offset, T& value, std::string& error) { return read_bytes(object, offset, &value, sizeof(value), error); }
bool vtable(const void* object, uintptr_t rva, std::string& error) {
    uintptr_t actual = 0;
    if (!read(object, 0, actual, error)) return false;
    if (actual != image + rva) { error = "fixed native vtable differs expected_rva=" + std::to_string(rva); return false; }
    return true;
}
bool read_world_witness(WorldWitness& witness, std::string& error) {
    if (!native::in_game()) { error = "native active world unavailable"; return false; }
    void* active = nullptr; void* world_singleton = nullptr; void* map_singleton = nullptr;
    if (!read(reinterpret_cast<void*>(image), 0x13df8f0, witness.game, error) ||
        !read(reinterpret_cast<void*>(image), 0x13df320, witness.mode, error) ||
        !read(reinterpret_cast<void*>(image), 0x13df9c0, world_singleton, error) ||
        !read(reinterpret_cast<void*>(image), 0x13df8c0, map_singleton, error)) return false;
    if (!vtable(witness.game, 0x1743740, error) || !vtable(witness.mode, 0x16e6730, error) ||
        !read(witness.game, 0x28, active, error)) return false;
    if (active != witness.mode) { error = "native active mode differs from mode singleton"; return false; }
    if (!read(witness.mode, 0x58, witness.state, error) || !vtable(witness.state, 0x16de920, error) ||
        !read(witness.state, 0x160, witness.world, error) || !vtable(witness.world, 0x174aef0, error) ||
        !read(witness.world, 0x158, witness.map, error) || !vtable(witness.map, 0x1742860, error)) return false;
    if (witness.world != world_singleton || witness.map != map_singleton) { error = "native GameState/World/Map ownership differs from singleton receivers"; return false; }
    return true;
}
DWORD contains_native(void* context, void* object, int32_t& id, bool& found) {
    __try { found = reinterpret_cast<bool(*)(void*, void*, int32_t*)>(image + 0x89e690)(context, object, &id); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
bool contains_checked(void* context, void* object, int32_t& id, std::string& error) {
    bool found = false;
    const auto exception = contains_native(context, object, id, found);
    if (exception) { error = "ContainsObject exception=" + std::to_string(exception); return false; }
    if ((found && id <= 0) || (!found && id != 0)) { error = "ContainsObject invalid presence/link actual=" + std::to_string(id); return false; }
    return true;
}
DWORD typed_native(void* object, void*& typed) {
    __try { typed = reinterpret_cast<void*(*)(void*, const void*, const void*, ptrdiff_t)>(image + 0x12654f0)(object,
        reinterpret_cast<void*>(image + 0x1657270), reinterpret_cast<void*>(image + 0x1655090), -1); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
bool typed_checked(void* object, std::string& error) {
    void* typed = nullptr;
    const auto exception = typed_native(object, typed);
    if (exception || typed != object) { error = "native Building RTTI differs exception=" + std::to_string(exception); return false; }
    return true;
}
DWORD registry_native(uint64_t uid, void*& typed) {
    __try {
        auto* object = reinterpret_cast<void*(*)(uint64_t)>(image + 0x47ce20)(uid);
        typed = object ? reinterpret_cast<void*(*)(void*, const void*, const void*, ptrdiff_t)>(image + 0x12654f0)(object,
            reinterpret_cast<void*>(image + 0x1673900), reinterpret_cast<void*>(image + 0x1655090), 0) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
bool readback_checked(uint64_t uid, void* expected, native::BuildingState& building, std::string& error) {
    void* pointer = nullptr;
    const auto exception = registry_native(uid, pointer);
    if (exception || pointer != expected) { error = "UID registry pointer differs uid=" + std::to_string(uid) + " exception=" + std::to_string(exception); return false; }
    if (!native::read_building(uid, building, error)) return false;
    if (building.prop.uid != uid) { error = "native building readback UID differs"; return false; }
    return true;
}
Contains contains = contains_checked;
Typed typed = typed_checked;
Readback readback = readback_checked;

enum class WriterScope { Own, Foreign, Invalid };
WriterScope writer_scope(void* writer, WriterWitness& owner, std::string& error) {
    auto rejected = [&](const char* stage) { error = std::string(stage) + ": " + error; return WriterScope::Invalid; };
    if (!read(writer, 8, owner.stream, error)) return rejected("record writer stream");
    if (!vtable(owner.stream, 0x1736320, error)) return rejected("serialiser stream type");
    if (!read(owner.stream, 0x18, owner.consumer, error)) return rejected("stream consumer");
    uintptr_t consumer_vtable = 0;
    if (!read(owner.consumer, 0, consumer_vtable, error)) return rejected("consumer type");
    if (consumer_vtable == image + 0x1742fd0) { error = "direct vsFile metadata consumer"; return WriterScope::Foreign; }
    if (!vtable(owner.consumer, 0x1720790, error)) return rejected("file consumer type");
    if (!read(owner.consumer, 8, owner.file, error) || !read(owner.consumer, 0x10, owner.task, error)) return rejected("consumer owner fields");
    if (owner.file) {
        if (owner.task) { error = "consumer has both direct file and asynchronous task"; return WriterScope::Invalid; }
    } else {
        if (!vtable(owner.task, 0x1733bb0, error)) return rejected("stream file task type");
        uint8_t producing = 0;
        if (!read(owner.task, 0x48, producing, error)) return rejected("stream file task lifetime");
        if (producing != 1) { error = "stream file task producer completed actual=" + std::to_string(producing) + " required=1"; return WriterScope::Invalid; }
        if (!read(owner.task, 0x10, owner.file, error)) return rejected("stream file task file");
    }
    if (!vtable(owner.file, 0x1742fd0, error)) return rejected("owned file type");
    char* text = nullptr; size_t count = 0; int32_t mode = 0;
    if (!read(owner.file, 8, text, error) || !read(owner.file, 0x10, count, error) || !read(owner.file, 0x70, mode, error)) return rejected("owned file filename/mode");
    if (count != filename.size()) { error = "foreign filename length actual=" + std::to_string(count) + " required=" + std::to_string(filename.size()); return WriterScope::Foreign; }
    if (mode != 4) { error = "owned writer mode differs actual=" + std::to_string(mode) + " required=4"; return WriterScope::Invalid; }
    std::string actual(count, '\0');
    if (!read_bytes(text, 0, actual.data(), count, error)) return rejected("owned filename text");
    if (actual != filename) { error = "filename differs from bound capture UUID"; return WriterScope::Foreign; }
    return WriterScope::Own;
}
bool same_writer_lifetime(const WriterWitness& before, const WriterWitness& after, std::string& error) {
    if (before.stream != after.stream || before.consumer != after.consumer || before.file != after.file ||
        (before.task && before.task != after.task)) {
        error = "writer stream/consumer/file/task identity changed outside native direct-to-task handoff";
        return false;
    }
    return true;
}
bool bind_writer_lifetime(const WriterWitness& owner, std::string& error) {
    if (bound_writer.stream && !same_writer_lifetime(bound_writer, owner, error)) return false;
    bound_writer = owner;
    return true;
}

bool slice(const void* array, void* writer, void* context, Slice& result, std::string& error) {
    const auto address = reinterpret_cast<uintptr_t>(array);
    if (address < 0x408 || !readable(context, sizeof(void*))) { error = "array/context is not readable"; return false; }
    result.array = array; result.writer = writer; result.context = context; result.thread = GetCurrentThreadId();
    result.manager = reinterpret_cast<void*>(address - 0x408);
    if (!vtable(array, 0x172d7c0, error) || !vtable(result.manager, 0x17183c0, error) ||
        !read(array, 8, result.data, error) || !read(array, 0x10, result.count, error) || !read(array, 0x14, result.capacity, error) ||
        !read(result.manager, 0x450, result.shard, error) || !read(result.manager, 0x458, result.region, error)) return false;
    if (result.count < 0 || result.count > int32_t(maximum_buildings) || result.capacity < result.count) {
        error = "native array bounds actual_count=" + std::to_string(result.count) + " capacity=" + std::to_string(result.capacity) + " required_max=4096"; return false;
    }
    void* owner = nullptr; void* region_shard = nullptr;
    if (!result.shard || !result.region) { error = "manager shard/region is null"; return false; }
    if (!read(result.region, 0x950, owner, error) || !read(result.region, 0x70, region_shard, error)) return false;
    if (owner != result.manager || region_shard != result.shard) { error = "manager/region/shard reciprocal ownership differs"; return false; }
    for (int32_t index = 0; index < result.count; ++index) {
        Item item;
        if (!read(result.data, size_t(index) * sizeof(void*), item.pointer, error) || !typed(item.pointer, error) ||
            !read(item.pointer, 0x18, item.uid, error) || !read(item.pointer, 0x170, item.shard, error) || !read(item.pointer, 0x178, item.region, error)) return false;
        if (!item.uid || item.shard != result.shard || item.region != result.region) { error = "building UID/manager membership differs"; return false; }
        if (!contains(context, item.pointer, item.id, error)) return false;
        if (item.id < 0) { error = "native linkID is negative actual=" + std::to_string(item.id); return false; }
        item.suffix = std::to_string(item.id);
        if (!item.id) {
            item.child = index;
            if (!root_saved_building::region_id(image, bound_world.map, item.region, result.manager, item.shard, item.root_region, error)) {
                error = "inline saved building uid=" + std::to_string(item.uid) + " child=" + std::to_string(index) + ": " + error; return false;
            }
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
            uintptr_t addresspoint = 0, save_stream = 0, save_values = 0;
            if (!read(item.pointer, 0, addresspoint, error) || addresspoint < image ||
                !image_range(addresspoint - image, 0x70, false, error) ||
                !read(reinterpret_cast<void*>(addresspoint), 0x60, save_stream, error) ||
                !read(reinterpret_cast<void*>(addresspoint), 0x68, save_values, error)) return false;
            if (save_stream != image + 0xe572d0 || save_values != image + 0xe57610) {
                error = "inline Building derived save implementation is unsupported"; return false;
            }
#endif
            item.suffix = snapshot_building_key::root_suffix(item.root_region, index);
        }
        result.items.push_back(item);
    }
    return true;
}
void clear_private() { pending.clear(); groups.clear(); arrays.clear(); sealing.clear(); sealed_readbacks.clear(); bound_writer = {}; seal_cursor = 0; current.inflight_wrappers = 0; }
void caught(const char* operation, const char* cause) noexcept {
    try { std::lock_guard lock(mutex); std::string error; fail(error, std::string(operation) + ": " + cause); }
    catch (...) {
        emergency_failure = true;
        OutputDebugStringA("ERROR BuildingCapture: observer failure reporting failed; binding map blocked; native save unchanged\n");
    }
}
}

bool initialize(std::string& error) {
    std::lock_guard lock(mutex);
    if (!native::ready()) return fail(error, "exact-build native resolver unavailable");
    image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    constexpr std::array<unsigned char, 16> prefix{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x68};
    std::array<unsigned char, 16> actual{};
    auto rejected = [&](const std::string& cause) { image = 0; return fail(error, cause); };
    if (!image_range(entry_rva, actual.size(), true, error) || !read_bytes(reinterpret_cast<void*>(image), entry_rva, actual.data(), actual.size(), error)) return rejected(error);
    if (actual != prefix) return rejected("Building save wrapper prologue differs");
    const std::array<std::pair<uintptr_t, std::vector<unsigned char>>, 3> helpers{{
        {0x89e690, {0x57,0x56,0x53,0x48,0x83,0xec,0x20}},
        {0x12654f0, {0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x70}},
        {0x47ce20, {0x53,0x45,0x31,0xc0,0x48,0x85,0xc9}}
    }};
    for (const auto& [rva, expected] : helpers) {
        std::vector<unsigned char> body(expected.size());
        if (!image_range(rva, body.size(), true, error) || !read_bytes(reinterpret_cast<void*>(image), rva, body.data(), body.size(), error)) return rejected(error);
        if (body != expected) return rejected("native helper prologue differs rva=" + std::to_string(rva));
    }
    for (uintptr_t rva : {0x172d7c0,0x17183c0,0x1736320,0x1720790,0x1733bb0,0x1742fd0,0x1657270,0x1655090,0x1673900,
        0x1743740,0x16e6730,0x16de920,0x174aef0,0x1742860})
        if (!image_range(rva, 16, false, error)) return rejected(error);
    error.clear(); return true;
}
void* capture_entrypoint() { std::lock_guard lock(mutex); return image ? reinterpret_cast<void*>(image + entry_rva) : nullptr; }
bool begin(const std::string& uuid, uint64_t epoch, std::string& error, const std::string& world_name, bool bind_snapshot_identities) {
    std::lock_guard lock(mutex);
    if (!image || armed || !valid_uuid(uuid) || !net::snapshot_world_path::valid_folder(world_name)) { error = "begin requires ready resolver, valid UUID and no active capture"; LOG_ERROR("[BuildingCapture] {}", error); return false; }
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    uint8_t main_skip = 0, legacy_skip = 0; uint64_t main_offset = 0;
    if (!read(reinterpret_cast<void*>(image + 0x13be4c0), 0x20, main_skip, error) ||
        !read(reinterpret_cast<void*>(image + 0x13be4c0), 0x38, main_offset, error) ||
        !read(reinterpret_cast<void*>(image + 0x13be500), 0x20, legacy_skip, error)) {
        LOG_ERROR("[BuildingCapture] begin saved-property guard failed cause={}", error); return false;
    }
    if (main_skip != 0 || main_offset != 0x408 || legacy_skip != 1) {
        error = "saved-property flags/member offset differ main_skip=" + std::to_string(main_skip) + " legacy_skip=" + std::to_string(legacy_skip) + " member_offset=" + std::to_string(main_offset);
        LOG_ERROR("[BuildingCapture] {}", error); return false;
    }
    native::WorldStatus world;
    if (!native::read_world_status(world, error)) { LOG_ERROR("[BuildingCapture] begin read_world_status failed cause={}", error); return false; }
    if (!world.world_present || world.saves_in_progress != 0) {
        error = "begin requires active world and no native save: world=" + std::to_string(world.world_present) + " saves=" + std::to_string(world.saves_in_progress);
        LOG_ERROR("[BuildingCapture] {}", error); return false;
    }
#endif
    WorldWitness candidate_world;
    if (!read_world_witness(candidate_world, error)) { LOG_ERROR("[BuildingCapture] begin world witness rejected: {}", error); return false; }
    clear_private(); current = {}; emergency_failure = false; current.state = "armed"; current.capture_uuid = uuid; current.session_epoch = epoch;
    capture_snapshot_identities = bind_snapshot_identities;
    bound_world = candidate_world; current.world_witness_bound = true; current.world_witness_current = true;
    current.game_thread_id = GetCurrentThreadId(); filename = net::snapshot_world_path::capture(world_name, uuid); armed = true;
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    if (capture_snapshot_identities) {
        if (!npc_quest_target_runtime::arm_host(uuid,{candidate_world.game,candidate_world.mode,candidate_world.state,candidate_world.world,candidate_world.map},error)) { armed=false; return fail(error,"Quest target capture arm failed: "+error); }
        if (!npc_saved_identity_runtime::arm_host(uuid,{candidate_world.game,candidate_world.mode,candidate_world.state,candidate_world.world,candidate_world.map},error)) { armed=false; npc_quest_target_runtime::cancel(error); return fail(error,"NPC identity capture arm failed: "+error); }
        if (!bug_authority::arm_snapshot(uuid,{},true,error)) { armed=false; npc_saved_identity_runtime::cancel(error); npc_quest_target_runtime::cancel(error); return fail(error,"Bug capture arm failed: "+error); }
    }
#endif
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    if(!monster_saved_identity_runtime::arm_host(uuid,{candidate_world.game,candidate_world.mode,candidate_world.state,candidate_world.world,candidate_world.map},error)) {
        armed=false;npc_saved_identity_runtime::cancel(error);npc_quest_target_runtime::cancel(error);bug_authority::cancel_snapshot(error);
        return fail(error,"Monster identity capture arm failed: "+error);
    }
#endif
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    if(!boss_saved_identity_runtime::arm_host(uuid,{candidate_world.game,candidate_world.mode,candidate_world.state,candidate_world.world,candidate_world.map},error)) {
        armed=false;monster_saved_identity_runtime::cancel(error);boss_saved_identity_runtime::cancel(error);npc_saved_identity_runtime::cancel(error);npc_quest_target_runtime::cancel(error);bug_authority::cancel_snapshot(error);
        return fail(error,"Boss identity capture arm failed: "+error);
    }
#endif
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    if(!district_saved_identity_runtime::arm_host(uuid,{candidate_world.game,candidate_world.mode,candidate_world.state,candidate_world.world,candidate_world.map},error)) {
        armed=false;monster_saved_identity_runtime::cancel(error);boss_saved_identity_runtime::cancel(error);npc_saved_identity_runtime::cancel(error);npc_quest_target_runtime::cancel(error);bug_authority::cancel_snapshot(error);
        return fail(error,"District identity capture arm failed: "+error);
    }
#endif
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    if(!party_saved_identity_runtime::arm_host(uuid,{candidate_world.game,candidate_world.mode,candidate_world.state,candidate_world.world,candidate_world.map},error)){
        armed=false;district_saved_identity_runtime::cancel(error);monster_saved_identity_runtime::cancel(error);boss_saved_identity_runtime::cancel(error);
        npc_saved_identity_runtime::cancel(error);npc_quest_target_runtime::cancel(error);bug_authority::cancel_snapshot(error);
        return fail(error,"Party identity capture arm failed: "+error);
    }
#endif
    error.clear(); LOG_INFO("[BuildingCapture] Armed capture={} epoch={} game_thread={}", uuid, epoch, current.game_thread_id); return true;
}
Ticket capture_before(const void* array, void* writer, void* context) noexcept {
    try {
        std::lock_guard lock(mutex);
        if (!armed || current.state == "failed" || current.state == "confirmed") return 0;
        ++current.wrapper_calls;
        std::string error;
        if (current.file_confirmed) { fail(error, "save wrapper entered after final file receipt"); return 0; }
        WriterWitness owner;
        const auto scope = writer_scope(writer, owner, error);
        if (scope == WriterScope::Foreign) {
            ++current.foreign_writers_skipped;
            if (current.foreign_writers_skipped == 1) LOG_INFO("[BuildingCapture] Foreign/metadata save writer skipped capture={}", current.capture_uuid);
            return 0;
        }
        if (scope == WriterScope::Invalid) { fail(error, "writer scope: " + error); return 0; }
        if (!bind_writer_lifetime(owner, error)) { fail(error, "writer scope: " + error); return 0; }
        if (pending.size() >= maximum_inflight) { fail(error, "inflight wrapper limit actual=257 required_max=256"); return 0; }
        std::set<void*> contexts;
        for (const auto& value : groups) contexts.insert(value.first);
        for (const auto& value : pending) contexts.insert(value.second.context);
        contexts.insert(context);
        if (contexts.size() > maximum_contexts) { fail(error, "context count actual=" + std::to_string(contexts.size()) + " required_max=8"); return 0; }
        Slice before;
        if (!slice(array, writer, context, before, error)) { fail(error, "before: " + error); return 0; }
        before.file_owner = owner;
        const auto found = std::find(current.observed_thread_ids.begin(), current.observed_thread_ids.end(), before.thread);
        if (found == current.observed_thread_ids.end()) {
            if (current.observed_thread_ids.size() >= 8) { fail(error, "wrapper thread count actual=9 required_max=8"); return 0; }
            current.observed_thread_ids.push_back(before.thread);
            LOG_INFO("[BuildingCapture] Wrapper thread observed capture={} thread={} game_thread={}", current.capture_uuid, before.thread, current.game_thread_id);
        }
        const auto ticket = ++sequence;
        pending.emplace(ticket, std::move(before)); current.inflight_wrappers = pending.size(); current.contexts_seen = contexts.size(); current.state = "serializing";
        return ticket;
    } catch (const std::exception& exc) { caught("before exception", exc.what()); return 0; }
    catch (...) { caught("before", "non-standard exception"); return 0; }
}
void capture_after(Ticket ticket, const void* array, void* writer, void* context) noexcept {
    try {
        std::lock_guard lock(mutex);
        if (!ticket || !armed || current.state == "failed") return;
        const auto found = pending.find(ticket);
        std::string error;
        if (found == pending.end()) { fail(error, "after ticket absent"); return; }
        Slice after;
        WriterWitness owner;
        if (writer_scope(writer, owner, error) != WriterScope::Own) { fail(error, "after writer no longer owned: " + error); return; }
        if (!same_writer_lifetime(found->second.file_owner, owner, error)) { fail(error, error); return; }
        if (!bind_writer_lifetime(owner, error)) { fail(error, error); return; }
        if (!slice(array, writer, context, after, error)) { fail(error, "after: " + error); return; }
        after.file_owner = found->second.file_owner;
        if (after != found->second) { fail(error, "native array/context/owner/ID/UID/thread changed during original save"); return; }
        auto& group = groups[context];
        for (const auto& item : after.items) {
            const auto previous = group.find(item.suffix);
            if (previous != group.end() && previous->second != item) { fail(error, "save linkID maps to changed object id=" + std::to_string(item.id)); return; }
            if (std::any_of(group.begin(), group.end(), [&](const auto& value) { return value.second.uid == item.uid && value.first != item.suffix; })) {
                fail(error, "one local UID has distinct save linkIDs uid=" + std::to_string(item.uid)); return;
            }
            group[item.suffix] = item;
        }
        const auto identity = std::make_pair(context, array);
        if (!arrays.contains(identity)) {
            if (arrays.size() >= maximum_arrays) { fail(error, "saved array count exceeds required_max=4096"); return; }
            arrays.insert(identity); current.native_array_buildings += size_t(after.count); current.unique_main_arrays = arrays.size();
        }
        current.staged_buildings = 0;
        for (const auto& value : groups) current.staged_buildings += value.second.size();
        if (current.staged_buildings > maximum_buildings || current.native_array_buildings > maximum_buildings) { fail(error, "total building count exceeds required_max=4096"); return; }
        pending.erase(found); current.inflight_wrappers = pending.size(); ++current.wrapper_returns; ++current.arrays_verified;
        current.state = pending.empty() ? "staged" : "serializing";
    } catch (const std::exception& exc) { caught("after exception", exc.what()); }
    catch (...) { caught("after", "non-standard exception"); }
}
void original_failed(Ticket ticket, const std::string& cause) noexcept {
    try {
        std::lock_guard lock(mutex);
        if (!ticket || !armed || !pending.contains(ticket)) {
            LOG_ERROR("[BuildingCapture] Original save exception outside staged ticket={}: {}; original exception remains with caller", ticket, cause);
            return;
        }
        std::string error; fail(error, "original native save failed: " + cause);
    } catch (...) { caught("original failure reporting", cause.c_str()); }
}
SealResult seal(const std::string& uuid, const std::string& sha, uint64_t bytes, uint64_t epoch, Status& result, std::string& error) {
    std::lock_guard lock(mutex);
    auto rejected = [&](const std::string& cause) { fail(error, cause); result = current; armed = false; clear_private(); return SealResult::Failed; };
    if (emergency_failure) return rejected("observer failure reporting failed; native save unchanged");
    if (current.state == "failed") return rejected(current.error);
    if (GetCurrentThreadId() != current.game_thread_id) return rejected("seal game thread differs");
    WorldWitness observed_world;
    ++current.world_witness_checks;
    current.world_witness_current = false;
    if (!current.world_witness_bound || !read_world_witness(observed_world, error)) return rejected("seal world witness rejected: " + error);
    if (observed_world != bound_world) return rejected("seal native Game/Mode/GameState/World/Map receiver changed since begin");
    current.world_witness_current = true;
    if (current.state == "confirmed" && uuid == current.capture_uuid && epoch == current.session_epoch &&
        sha == current.snapshot_sha256 && bytes == current.snapshot_bytes && GetCurrentThreadId() == current.game_thread_id) {
        result = current; error.clear(); return SealResult::Confirmed;
    }
    if (!armed || uuid != current.capture_uuid || epoch != current.session_epoch || GetCurrentThreadId() != current.game_thread_id)
        return rejected("seal capture UUID/session epoch/game thread differs");
    if (!valid_sha(sha) || !bytes) return rejected("owned final file/SHA receipt invalid");
    if (!pending.empty()) { current.state = "awaiting_wrappers"; result = current; error = "expected wait: native wrapper returns pending actual=" + std::to_string(pending.size()); return SealResult::Pending; }
    if (groups.size() != 1) return rejected("world save-context count actual=" + std::to_string(groups.size()) + " required=1");
    if (current.native_array_buildings != current.staged_buildings) return rejected("native declared/unique building counts differ");
    if (current.snapshot_sha256.empty()) {
        current.snapshot_sha256 = sha; current.snapshot_bytes = bytes; current.file_confirmed = true; current.state = "sealing";
        for (const auto& value : groups.begin()->second) sealing.push_back(value.second);
    } else if (sha != current.snapshot_sha256 || bytes != current.snapshot_bytes) return rejected("seal SHA/bytes changed during readback");
    size_t completed = 0;
    while (seal_cursor < sealing.size() && completed < 64) {
        const auto& item = sealing[seal_cursor]; native::BuildingState building;
        ++current.readback_calls;
        if (!readback(item.uid, item.pointer, building, error)) return rejected("seal id=" + std::to_string(item.id) + " uid=" + std::to_string(item.uid) + " " + error);
        if (building.prop.uid != item.uid) return rejected("native building readback UID differs id=" + std::to_string(item.id));
        sealed_readbacks.push_back({item.id, item.uid, sha + ":world:building:" + item.suffix, std::move(building)});
        ++seal_cursor; ++completed; current.readbacks_confirmed = seal_cursor;
    }
    if (seal_cursor < sealing.size()) { result = current; error = "expected wait: native readbacks actual=" + std::to_string(seal_cursor) + " required=" + std::to_string(sealing.size()); return SealResult::Pending; }
    // One game-thread turn checks the complete map again after the incremental reads.
    for (size_t index = 0; index < sealing.size(); ++index) {
        const auto& item = sealing[index]; native::BuildingState building;
        ++current.readback_calls;
        if (!readback(item.uid, item.pointer, building, error)) return rejected("final seal id=" + std::to_string(item.id) + " uid=" + std::to_string(item.uid) + " " + error);
        if (building.prop.uid != item.uid) return rejected("final native building readback UID differs id=" + std::to_string(item.id));
        if (!item.id) {
            int32_t region = -1;
            if (!root_saved_building::slot(image, bound_world.map, item.pointer, item.child, region, error) || region != item.root_region)
                return rejected("final inline saved owner/child changed: " + error);
        }
        if (building.dungeon_instance_membership) ++current.dungeon_instance_buildings;
        else if (building.dungeon_membership) ++current.dungeon_buildings;
        else ++current.root_buildings;
        sealed_readbacks[index].building = std::move(building); ++current.final_readbacks_checked;
    }
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    dungeon_scene_identity_runtime::Census dungeon_scenes;
    std::vector<dungeon_scene_identity_runtime::Exterior> scene_exteriors;
    for(const auto& row:sealed_readbacks)scene_exteriors.push_back({row.local_uid,row.record_key});
    if(!dungeon_scene_identity_runtime::classify_capture(uuid,sha,{observed_world.game,observed_world.mode,observed_world.state,observed_world.world,observed_world.map},scene_exteriors,dungeon_scenes,error))
        return rejected("Dungeon scene native snapshot classification failed: "+error);
    if(capture_snapshot_identities&&!npc_quest_target_runtime::stage_scenes(dungeon_scenes,error))
        return rejected("Dungeon Quest Zone native snapshot classification failed: "+error);
    if(!party_saved_identity_runtime::stage_scenes(dungeon_scenes,error))
        return rejected("Dungeon Party native snapshot classification failed: "+error);
    if(!boss_saved_identity_runtime::stage_scenes(dungeon_scenes,error))
        return rejected("Dungeon Boss native snapshot classification failed: "+error);
    if(!monster_saved_identity_runtime::stage_scenes(std::move(dungeon_scenes),error))
        return rejected("Dungeon Monster native snapshot classification failed: "+error);
    if (capture_snapshot_identities) {
        if (!npc_quest_target_runtime::seal_host(uuid,sha,{observed_world.game,observed_world.mode,observed_world.state,observed_world.world,observed_world.map},error)) return rejected("Quest target snapshot seal failed: "+error);
        if (!npc_saved_identity_runtime::seal_host(uuid,sha,{observed_world.game,observed_world.mode,observed_world.state,observed_world.world,observed_world.map},error)) return rejected("NPC identity snapshot seal failed: "+error);
        if (!bug_authority::seal_snapshot(uuid,sha,error)) return rejected("Bug snapshot seal failed: "+error);
    }
#endif
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    if(!party_saved_identity_runtime::seal_host(uuid,sha,{observed_world.game,observed_world.mode,observed_world.state,observed_world.world,observed_world.map},error))
        return rejected("Party identity snapshot seal failed: "+error);
    if(!boss_saved_identity_runtime::seal_host(uuid,sha,{observed_world.game,observed_world.mode,observed_world.state,observed_world.world,observed_world.map},error))
        return rejected("Boss identity snapshot seal failed: "+error);
    if(!monster_saved_identity_runtime::seal_host(uuid,sha,{observed_world.game,observed_world.mode,observed_world.state,observed_world.world,observed_world.map},error))
        return rejected("Monster identity snapshot seal failed: "+error);
#endif
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    if(!district_saved_identity_runtime::seal_host(uuid,sha,{observed_world.game,observed_world.mode,observed_world.state,observed_world.world,observed_world.map},error))
        return rejected("District identity snapshot seal failed: "+error);
#endif
    current.buildings = std::move(sealed_readbacks);
    current.state = "confirmed"; current.bindings_confirmed = true; armed = false; clear_private(); result = current; error.clear();
    LOG_INFO("[BuildingCapture] FileSHA/registry/membership map confirmed capture={} sha={} buildings={} arrays={} root={} dungeon={} instances={}",
        uuid, sha, current.buildings.size(), current.unique_main_arrays, current.root_buildings, current.dungeon_buildings, current.dungeon_instance_buildings);
    return SealResult::Confirmed;
}
void cancel(const std::string& reason) {
#ifndef MT2MP_CAPTURE_IDENTITY_TEST
    party_saved_identity_runtime::cancel(reason);
    district_saved_identity_runtime::cancel(reason);
    monster_saved_identity_runtime::cancel(reason);
    boss_saved_identity_runtime::cancel(reason);
    if (capture_snapshot_identities) {
        npc_quest_target_runtime::cancel(reason);
        npc_saved_identity_runtime::cancel(reason);
        bug_authority::cancel_snapshot(reason);
    }
#endif
    std::lock_guard lock(mutex); armed = false; clear_private(); bound_world = {}; current.world_witness_bound = false; current.world_witness_current = false; current.state = "cancelled"; current.error = reason; current.buildings.clear(); current.bindings_confirmed = false; LOG_INFO("[BuildingCapture] Cancelled capture={} reason={}", current.capture_uuid, reason); }
bool stream_owner(void* writer,StreamOwner& result,bool& own,std::string& error) {
    std::lock_guard lock(mutex); result={};own=false;
    if(!armed||!current.world_witness_bound||current.state=="failed"||emergency_failure) {
        error="Zone writer requires armed healthy snapshot capture";return false;
    }
    WriterWitness witness;
    const auto scope=writer_scope(writer,witness,error);
    if(scope==WriterScope::Invalid)return false;
    if(scope==WriterScope::Foreign){error.clear();return true;}
    if(!bind_writer_lifetime(witness,error))return false;
    result={current.capture_uuid,reinterpret_cast<uintptr_t>(witness.stream),reinterpret_cast<uintptr_t>(witness.consumer),
        reinterpret_cast<uintptr_t>(witness.file),reinterpret_cast<uintptr_t>(witness.task)};
    own=true;error.clear();return true;
}
Status status() {
    std::lock_guard lock(mutex);
    if (emergency_failure) { current.state = "failed"; current.error = "observer failure reporting failed; native save unchanged"; current.bindings_confirmed = false; current.buildings.clear(); }
    return current;
}
#ifdef MT2MP_CAPTURE_IDENTITY_TEST
namespace testing {
void backend(uintptr_t base, Contains query, Typed type, Readback reader) { std::lock_guard lock(mutex); image = base; contains = query; typed = type; readback = reader; }
}
#endif
}

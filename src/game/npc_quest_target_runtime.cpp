#include "npc_quest_target_runtime.h"
#include "npc_zone_append.h"
#include "npc_quest_building_key.h"
#include "building_capture_identity.h"
#include "native_open_runtime.h"
#include "root_saved_building.h"
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
struct Save { q::SaveFrame frame; Owner owner; uint32_t thread = 0; };
struct Load { q::LoadFrame frame; uintptr_t property = 0, manager = 0; uint32_t thread = 0; q::ChildHeader child; bool child_pending = false; };
std::map<Ticket, Save> saves;
std::map<Ticket, Load> loads;
std::map<uint32_t, Ticket> load_stack;
std::map<uintptr_t, q::Array> observed;
std::map<int32_t, q::SavedPool> pools;
std::map<std::string, npc_zone_append::Receipt> zone_appends;
#ifdef MT2MP_QUEST_TARGET_RUNTIME_TEST
using TestRead = bool(*)(uintptr_t, void*, size_t, std::string&);
using TestLink = bool(*)(uintptr_t, uintptr_t, int32_t&, std::string&);
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
    error = failure; pools.clear(); return false;
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
bool same_world(const opened::CurrentWorld& a, const opened::CurrentWorld& b) {
    return a.generation == b.generation && a.world_epoch == b.world_epoch && a.game == b.game &&
        a.mode == b.mode && a.state == b.state && a.world == b.world && a.map == b.map;
}
void reset(const std::string& id, const std::string& digest) {
    uuid = id; sha = digest; failure.clear(); emergency = false; saves.clear(); loads.clear();
    load_stack.clear(); observed.clear(); pools.clear(); zone_appends.clear(); bound_context = 0; main_thread = GetCurrentThreadId();
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
    origin = world; pools = std::move(ready); observed.clear();
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
void cancel(const std::string& cause) { std::lock_guard lock(gate); host_active = false; client_active = false; failure = cause; saves.clear(); loads.clear(); load_stack.clear(); observed.clear(); pools.clear(); zone_appends.clear(); }
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
        !live_reader(frame->frame.reader, frame->frame.context, error)) { reject(error, "Quest original LoadStream result/lifetime differs: " + error); return; }
    q::Array value;
    if (!frame->frame.finish(memory, image, budget, value, error) || !retain(std::move(value), error)) { reject(error, error); return; }
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
}

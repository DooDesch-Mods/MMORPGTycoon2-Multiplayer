#include "district_saved_identity_runtime.h"
#include "live_host_state/memory_query_timing.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "building_capture_identity.h"
#include "root_saved_building.h"
#ifndef MT2MP_DISTRICT_SAVED_RUNTIME_TEST
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

namespace mt2mp::game::native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld&, std::string&);
}

namespace mt2mp::game::district_saved_identity_runtime {
namespace {
namespace q = district_saved_identity;
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
std::map<int32_t, q::SavedPool> pools;
#ifdef MT2MP_DISTRICT_SAVED_RUNTIME_TEST
using TestRead = bool(*)(uintptr_t, void*, size_t, std::string&);
using TestLink = bool(*)(uintptr_t, uintptr_t, int32_t&, std::string&);
TestRead test_read = nullptr;
TestLink test_link = nullptr;
#endif
struct Memory {
    bool copy(uintptr_t p, void* out, size_t bytes, std::string& error) { return read(p, out, bytes, error); }
    bool read(uintptr_t p, void* out, size_t bytes, std::string& error) {
#ifdef MT2MP_DISTRICT_SAVED_RUNTIME_TEST
        if (test_read) return test_read(p, out, bytes, error);
#endif
        return root_saved_building::bytes(reinterpret_cast<void*>(p), 0, out, bytes, error);
    }
} memory;
bool reject(std::string& error, const std::string& cause) {
    if (failure.empty()) { failure = cause; LOG_ERROR("[SavedDistrict] snapshot={} cause={}; original serialization unchanged", uuid, cause); }
    error = failure; pools.clear(); return false;
}
void caught(const char* cause) noexcept {
    emergency = true; OutputDebugStringA("ERROR SavedDistrict observer exception: ");
    OutputDebugStringA(cause); OutputDebugStringA("; target bindings blocked, original unchanged\n");
}
bool healthy(std::string& error) {
    if (emergency) return reject(error, "District identity observer exception; original diagnostic retained");
    if (!failure.empty()) { error = failure; return false; }
    return true;
}
bool same_world(const opened::CurrentWorld& a, const opened::CurrentWorld& b) {
    return a.generation == b.generation && a.world_epoch == b.world_epoch && a.game == b.game &&
        a.mode == b.mode && a.state == b.state && a.world == b.world && a.map == b.map;
}
void reset(const std::string& id, const std::string& digest) {
    uuid = id; sha = digest; failure.clear(); emergency = false; saves.clear(); loads.clear();
    load_stack.clear(); observed.clear(); pools.clear(); bound_context = 0; main_thread = GetCurrentThreadId();
}
bool context(uintptr_t actual, std::string& error) {
    if (!actual || (bound_context && bound_context != actual)) return reject(error, "District property save/load context differs");
    bound_context = actual; return true;
}
DWORD contains_call(uintptr_t ctx, uintptr_t object, int32_t& id, bool& present) {
    __try { present = reinterpret_cast<bool(*)(void*, const void*, int32_t*)>(image + 0x89e690)(
        reinterpret_cast<void*>(ctx), reinterpret_cast<void*>(object), &id); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool contains(uintptr_t ctx, uintptr_t object, int32_t& id, std::string& error) {
#ifdef MT2MP_DISTRICT_SAVED_RUNTIME_TEST
    if (test_link) return test_link(ctx, object, id, error);
#endif
    bool present = false; id = 0; const auto code = contains_call(ctx, object, id, present);
    if (code || (present ? id <= 0 : id != 0)) return q::fail(error, "District SaveContext ContainsObject differs exception=" + std::to_string(code));
    return true;
}
#ifndef MT2MP_DISTRICT_SAVED_RUNTIME_TEST
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
    return actual == ctx || q::fail(error, "District LoadContext is not active Mode context");
}
Load* top(std::string& error) {
    const auto thread = load_stack.find(GetCurrentThreadId());
    if (thread == load_stack.end()) { reject(error, "District event outside its owned property frame"); return nullptr; }
    const auto frame = loads.find(thread->second);
    if (frame == loads.end()) { reject(error, "District property frame missing"); return nullptr; }
    return &frame->second;
}
bool retain(q::Pool value, std::string& error) {
    if (observed.size() >= 4096 || !observed.emplace(value.manager, std::move(value)).second)
        return reject(error, "District property array duplicate or observation budget exceeded");
    return true;
}
bool seal_world(const opened::CurrentWorld& world, bool, std::string& error) {
    if (!healthy(error) || !saves.empty() || !loads.empty() || !load_stack.empty() || !bound_context || observed.empty())
        return reject(error, failure.empty() ? "District snapshot lacks complete property returns/context" : failure);
    const auto map = reinterpret_cast<uintptr_t>(world.map), root = map + 0x1c0;
    q::Bounds regions;
    if (!q::typed(memory, image, map, 0x1742860, error) || !q::typed(memory, image, map + 0x230, 0x16f04e0, error) ||
        !q::bounds(memory, map + 0x230, 4096, regions, error)) return reject(error, error);
    std::map<int32_t, q::SavedPool> ready;
    std::set<std::string> keys;
    for (int32_t i = 0; i < regions.count; ++i) {
        uintptr_t region = 0, manager = 0, selected = 0, owner = 0; int32_t id = -1;
        if (!q::at(memory, regions.data, static_cast<size_t>(i) * 8, region, error) ||
            !q::at(memory, region, 0x834, id, error) || !q::at(memory, region, 0x970, manager, error) ||
            !q::at(memory, manager, 0x60, owner, error)) return reject(error, error);
        auto read = [&](uintptr_t p, size_t offset, auto& out, std::string& cause) { return q::at(memory, p, offset, out, cause); };
        if (!region_saved_identity::resolve(image, map, root, id, region, selected, read, error) || owner != region)
            return reject(error, error.empty() ? "District saved Region/Manager owner differs" : error);
        const auto found = observed.find(manager);
        if (found == observed.end()) return reject(error, "saved root Region District property unobserved id=" + std::to_string(id));
        auto links = [&](uintptr_t object, int32_t& link, std::string& cause) {
            for (const auto& row : found->second.records) if (row.pointer == object) { link = row.link_id; return true; }
            return q::fail(cause, "District seal member absent from original serialization");
        };
        q::Pool actual;
        if (!q::observe(memory, image, manager, links, actual, error) || actual != found->second)
            return reject(error, error.empty() ? "District identities/order changed before snapshot seal" : error);
        for (const auto& row : actual.records) {
            uintptr_t district_region = 0, district_root=0;
            if (!q::at(memory, row.pointer, 0x178, district_region, error)||!q::at(memory,row.pointer,0x170,district_root,error)) return reject(error, error);
            const auto key = q::key(sha, id, row);
            if (district_region != region || district_root!=root || !snapshot_district_key::valid(key) || !keys.insert(key).second)
                return reject(error, "District saved owner/key duplicate or invalid");
        }
        if (!ready.emplace(id, q::SavedPool{id, std::move(actual)}).second) return reject(error, "District saved Region ID repeats");
    }
    if (ready.empty() || !q::unchanged(memory, map + 0x230, 4096, regions, error)) return reject(error, error.empty() ? "saved root Regions absent" : error);
    origin = world; pools = std::move(ready); observed.clear();
    LOG_INFO("[SavedDistrict] Native snapshot Districts bound uuid={} sha={} regions={}", uuid, sha, pools.size());
    error.clear(); return true;
}
bool current(opened::CurrentWorld& actual, std::string& error) {
    if (!healthy(error)) return false;
    if (GetCurrentThreadId() != main_thread || host_active || client_active || net::current_snapshot_id() != uuid || !net::current_world_epoch())
        return q::fail(error, "Saved Districts require current active snapshot on bound Mainthread");
    if (!origin.generation || !origin.world_epoch) return q::fail(error, "Saved Districts require completed snapshot load after capture");
    if (!native_open_runtime::read_current_world(actual, error)) return false;
    return same_world(origin, actual) || q::fail(error, "Saved District native world generation changed");
}
DWORD find_call(build_actions::private_native::host::FindUid find, uint64_t uid, uintptr_t& out) {
    __try { out = reinterpret_cast<uintptr_t>(find(uid)); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}

}
bool initialize(std::string& error) {
    std::lock_guard lock(gate);
    if (!native::ready()) return q::fail(error, "District identity exact native resolver unavailable");
    image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const std::array<unsigned char, 16> save{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x78};
    const std::array<unsigned char, 16> load{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x88};
    auto guard = [&](uintptr_t rva, const auto& bytes) {
        MEMORY_BASIC_INFORMATION page{}; std::vector<unsigned char> actual(bytes.size());
        if (!VirtualQuery(reinterpret_cast<void*>(image + rva), &page, sizeof page) || page.Type != MEM_IMAGE ||
            page.AllocationBase != reinterpret_cast<void*>(image) || page.Protect != PAGE_EXECUTE_READ ||
            !memory.read(image + rva, actual.data(), actual.size(), error) || !std::equal(actual.begin(), actual.end(), bytes.begin()))
            return q::fail(error, "District property original entry guard differs rva=" + std::to_string(rva));
        return true;
    };
    if (!guard(q::save_property_rva, save) || !guard(q::load_property_rva, load)) { image = 0; return false; }
    error.clear(); return true;
}
void* entrypoint(bool save) { return image ? reinterpret_cast<void*>(image + (save ? q::save_property_rva : q::load_property_rva)) : nullptr; }
bool confirm_hooks(std::string& error) { std::lock_guard lock(gate); if (!image) return q::fail(error, "District property resolver absent"); hooks = true; error.clear(); return true; }
bool arm_host(const std::string& id, const CaptureWorld& capture, std::string& error) {
    std::lock_guard lock(gate);
    if (!hooks || host_active || client_active || !saves.empty() || !loads.empty()) return q::fail(error, "District snapshot observer busy/uninstalled");
    if (!capture.game || !capture.mode || !capture.state || !capture.world || !capture.map)
        return q::fail(error, "District Host capture requires complete BuildingCapture world witness");
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
            return q::fail(error, "District Host loaded origin differs from BuildingCapture world witness");
        world = std::move(loaded);
    }
    reset(id, ""); origin = std::move(world);
    host_active = true; error.clear(); return true;
}
bool seal_host(const std::string& id, const std::string& digest, const CaptureWorld& capture, std::string& error) {
    std::lock_guard lock(gate);
    if (!host_active || id != uuid || GetCurrentThreadId() != main_thread) return reject(error, "District Host snapshot seal lifetime differs");
    if (capture.game != origin.game || capture.mode != origin.mode || capture.state != origin.state ||
        capture.world != origin.world || capture.map != origin.map)
        return reject(error, "District Host BuildingCapture world witness changed");
    if (origin.generation) {
        opened::CurrentWorld loaded;
        if (!native_open_runtime::read_current_world(loaded, error) || !same_world(loaded, origin))
            return reject(error, "District Host completed origin changed during capture: " + error);
    }
    sha = digest; const bool confirmed = seal_world(origin, !origin.generation, error); host_active = false; return confirmed;
}
bool arm_client(const std::string& id, const std::string& digest, std::string& error) {
    std::lock_guard lock(gate); const auto witness = opened::status();
    if (!hooks || host_active || client_active || !saves.empty() || !loads.empty() || witness.state != "pinned" ||
        !witness.held || witness.uuid != id || witness.sha256 != digest || !witness.generation)
        return q::fail(error, "District Client arm lacks idle pinned exact NativeOpen receipt");
    reset(id, digest); generation = witness.generation; client_active = true; error.clear(); return true;
}
bool seal_client(std::string& error) {
    std::lock_guard lock(gate);
    if (!client_active || GetCurrentThreadId() != main_thread) return reject(error, "District Client seal lifetime differs");
    opened::CurrentWorld world;
    if (!opened::read_completed_world(generation, world, error) || world.uuid != uuid || world.sha256 != sha)
        return reject(error, "District Client completed native snapshot differs: " + error);
    const bool confirmed = seal_world(world, false, error); client_active = false; return confirmed;
}
void cancel(const std::string& cause) { std::lock_guard lock(gate); host_active = false; client_active = false; failure = cause; saves.clear(); loads.clear(); load_stack.clear(); observed.clear(); pools.clear(); }
Ticket save_before(const void* property, void* manager, void* writer, void* ctx) noexcept try {
    if (!host_active) return 0;
    Owner owner; bool own = false; std::string error;
    const bool checked = building_capture_identity::stream_owner(writer, owner, own, error);
    std::lock_guard lock(gate);
    if (!host_active || !healthy(error)) return 0;
    if (!checked) { reject(error, "District owned snapshot writer: " + error); return 0; }
    if (!own) return 0;
    if (owner.capture_uuid != uuid || !context(reinterpret_cast<uintptr_t>(ctx), error)) { reject(error, "District writer UUID/context differs: " + error); return 0; }
    auto links = [&](uintptr_t object, int32_t& id, std::string& cause) { return contains(reinterpret_cast<uintptr_t>(ctx), object, id, cause); };
    Save value; value.owner = owner; value.thread = GetCurrentThreadId();
    if (!q::begin_save(memory, image, reinterpret_cast<uintptr_t>(property), reinterpret_cast<uintptr_t>(manager),
        reinterpret_cast<uintptr_t>(writer), reinterpret_cast<uintptr_t>(ctx), links, value.frame, error)) { reject(error, error); return 0; }
    if (saves.size() >= 256 || next_ticket == UINT64_MAX) { reject(error, "District save frame budget/sequence exceeded"); return 0; }
    const auto ticket = ++next_ticket; saves.emplace(ticket, std::move(value)); return ticket;
} catch (const std::exception& e) { caught(e.what()); return 0; } catch (...) { caught("non-standard save before exception"); return 0; }
void save_after(Ticket ticket, const void* property, void* manager, void* writer, void* ctx, bool result) noexcept try {
    if (!ticket) return;
    Owner owner; bool own = false; std::string error;
    const bool checked = building_capture_identity::stream_owner(writer, owner, own, error);
    std::lock_guard lock(gate); if (!healthy(error)) return;
    const auto found = saves.find(ticket);
    if (!checked || !own || found == saves.end()) { reject(error, "District save return/file owner absent: " + error); return; }
    const auto& saved = found->second;
    if (saved.thread != GetCurrentThreadId() || saved.owner.capture_uuid != owner.capture_uuid ||
        saved.owner.stream != owner.stream || saved.owner.consumer != owner.consumer || saved.owner.file != owner.file ||
        (saved.owner.task && saved.owner.task != owner.task)) { reject(error, "District save writer/thread lifetime changed"); return; }
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
    if (!live_reader(r, c, error) || !context(c, error)) { reject(error, "District load reader/context rejected: " + error); return 0; }
    Load value; value.property = reinterpret_cast<uintptr_t>(property); value.manager = reinterpret_cast<uintptr_t>(manager); value.thread = GetCurrentThreadId();
    if (load_stack.contains(value.thread) || loads.size() >= 256 || next_ticket == UINT64_MAX ||
        !q::begin_load(memory, image, value.property, value.manager, r, c, value.frame, error)) { reject(error, "District load frame rejected: " + error); return 0; }
    const auto ticket = ++next_ticket; load_stack.emplace(value.thread, ticket); loads.emplace(ticket, std::move(value)); return ticket;
} catch (const std::exception& e) { caught(e.what()); return 0; } catch (...) { caught("non-standard load before exception"); return 0; }
void children_before(uintptr_t caller, void* reader) noexcept try {
    if (!client_active || caller != q::load_children_return_rva) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (frame->child_pending || frame->frame.reader != reinterpret_cast<uintptr_t>(reader) ||
        !live_reader(frame->frame.reader, frame->frame.context, error) || !q::child_header(memory, frame->frame.reader, frame->child, error)) { reject(error, "District child header rejected: " + error); return; }
    frame->child_pending = true;
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard children before exception"); }
void children_after(uintptr_t caller, void* reader, int32_t count) noexcept try {
    if (!client_active || caller != q::load_children_return_rva) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (!frame->child_pending || !live_reader(frame->frame.reader, frame->frame.context, error) ||
        !frame->frame.children(caller, reinterpret_cast<uintptr_t>(reader), frame->child.label, frame->child.count, count, error)) { reject(error, "District BeginChildren return rejected: " + error); return; }
    frame->child_pending = false;
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard children after exception"); }
void registered(uintptr_t caller, void* ctx, int32_t id, void* object) noexcept try {
    if (!client_active || (caller != q::load_link_return_rva && caller != q::load_inline_return_rva)) return;
    std::lock_guard lock(gate); std::string error; if (!healthy(error)) return;
    auto* frame = top(error); if (!frame) return;
    if (!live_reader(frame->frame.reader, frame->frame.context, error)) { reject(error, error); return; }
    if (id > 0) {
#ifndef MT2MP_DISTRICT_SAVED_RUNTIME_TEST
        uintptr_t actual = 0; const auto code = lookup_call(reinterpret_cast<uintptr_t>(ctx), id, actual);
        if (code || actual != reinterpret_cast<uintptr_t>(object)) { reject(error, "District registered ID lookup differs exception=" + std::to_string(code)); return; }
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
        !live_reader(frame->frame.reader, frame->frame.context, error)) { reject(error, "District original LoadStream result/lifetime differs: " + error); return; }
    q::Pool value;
    if (!frame->frame.finish(memory, image, value, error) || !retain(std::move(value), error)) { reject(error, error); return; }
    load_stack.erase(frame->thread); loads.erase(found);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard load after exception"); }
void original_failed(Ticket ticket, const std::string& cause) noexcept try {
    if (!ticket) return; std::lock_guard lock(gate); std::string error; reject(error, "original District property failed: " + cause);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("original failure reporting exception"); }
bool current_snapshot_sha(std::string& out, std::string& error) {
    std::lock_guard lock(gate); out.clear(); opened::CurrentWorld world;
    if (!current(world, error)) return false;
    out = sha; error.clear(); return true;
}
bool read_current(std::vector<Current>& out, std::string& error, bool share_metadata) {
    live_host_state::apply_timing::PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);
    std::lock_guard lock(gate); out.clear(); opened::CurrentWorld world;
    if (!current(world, error)) return false;
    namespace host = build_actions::private_native::host;
    host::ProcessMemory process; host::LookupApi api;
    constexpr const char* exe_sha = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
    if (!host::resolve_lookup(process, image, exe_sha, api, error)) return false;
    auto lookup = [&](uint64_t uid, uintptr_t& pointer, std::string& cause) {
        uint32_t code = 0;
        code = find_call(api.find, uid, pointer);
        return !code || q::fail(cause, "District UID lookup exception=" + std::to_string(code));
    };
    const auto map = reinterpret_cast<uintptr_t>(world.map);
    std::vector<Current> result;
    for (const auto& [id, saved] : pools) {
        if (saved.pool.records.empty()) continue;
        uintptr_t region = 0;
        auto read = [&](uintptr_t p, size_t offset, auto& value, std::string& cause) { return q::at(memory, p, offset, value, cause); };
        if (!region_saved_identity::resolve(image, map, map + 0x1c0, id, 0, region, read, error)) return false;
        uintptr_t manager=0,owner=0;
        if(!q::at(memory,region,0x970,manager,error)||manager!=saved.pool.manager||!q::at(memory,manager,0x60,owner,error)||owner!=region)
            return q::fail(error,error.empty()?"District current reciprocal manager/Region differs":error);
        q::Pool census;std::vector<Current> rows;
        const auto no_link=[](uintptr_t,int32_t& link,std::string&){link=0;return true;};
        if(!q::observe(memory,image,manager,no_link,census,error))return false;
        for(const auto& row:census.records){uintptr_t actual_region=0,actual_root=0;
            if(!q::at(memory,row.pointer,0x170,actual_root,error)||!q::at(memory,row.pointer,0x178,actual_region,error))return false;
            if(actual_region!=region||actual_root!=map+0x1c0)return q::fail(error,"District current member root/Region differs");
        }
        if(!q::current_rows(sha,saved,census,lookup,rows,error))return false;
        result.insert(result.end(), rows.begin(), rows.end());
    }
    opened::CurrentWorld after;
    if (!current(after, error) || !same_world(world, after)) return q::fail(error, error.empty() ? "District world changed during read" : error);
    out = std::move(result); error.clear(); return true;
}
bool resolve(const std::string& key, Current& out, std::string& error) {
    out = {};
    if (!snapshot_district_key::valid(key)) return q::fail(error, "Saved District key invalid");
    std::vector<Current> rows;
    if (!read_current(rows, error)) return false;
    for (const auto& row : rows) if (row.key == key) { out = row; return true; }
    return q::fail(error, "Saved District key absent from current snapshot members");
}
bool bind_local(uintptr_t object, uint64_t uid, std::string& key, std::string& error) {
    key.clear(); std::vector<Current> rows;
    if (!object || !uid) return q::fail(error, "Saved District local binding is null");
    if (!read_current(rows, error)) return false;
    for (const auto& row : rows) if (row.row.pointer == object && row.row.uid == uid) { key = row.key; return true; }
    return q::fail(error, "District local member is absent from the active snapshot");
}
}

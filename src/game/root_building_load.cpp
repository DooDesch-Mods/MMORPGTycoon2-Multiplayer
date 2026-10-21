#include "root_building_load.h"
#include "root_saved_building.h"
#include "saved_building_path.h"
#include "snapshot_building_key.h"
#include "native_open_witness.h"
#include "mt2mp/core/logger.h"
#include <array>
#include <atomic>
#include <map>
#include <mutex>

namespace mt2mp::game::root_building_load {
namespace {
namespace opened = building_mixed_identity::opened_file;
using namespace root_saved_building;
constexpr std::array<uintptr_t, 3> entries{0xc0ab50, 0x96bf30, 0x89e770};
uintptr_t image = 0;
uint32_t hooks = 0, main_thread = 0;
uint64_t sequence = 0, generation = 0;
std::string sha, uuid, failure;
std::mutex mutex;
std::atomic<bool> armed = false, emergency = false;
void* bound_context = nullptr;
struct Row { void* pointer = nullptr; uint64_t uid = 0; int32_t id = 0, child = -1; };
struct Frame {
    void* array = nullptr; void* reader = nullptr; void* context = nullptr;
    uint32_t thread = 0; int32_t declared = -1; bool counted = false;
    std::map<void*, int32_t> registrations;
};
struct Saved { void* array = nullptr; std::vector<Row> rows; };
std::map<Ticket, Frame> pending;
std::map<uint32_t, std::vector<Ticket>> stacks;
std::vector<Saved> saved;
size_t rows_seen = 0;
bool reject(std::string& error, const std::string& cause) {
    if (failure.empty()) { failure = cause; LOG_ERROR("[RootBuildingLoad] snapshot={} cause={}; native load unchanged", uuid, cause); }
    error = failure; return false;
}
void caught(const char* cause) noexcept {
    emergency = true;
    OutputDebugStringA("ERROR RootBuildingLoad observer exception: ");
    OutputDebugStringA(cause);
    OutputDebugStringA("; bindings blocked, original unchanged\n");
}
bool live(void* reader, std::string& error) {
    const auto state = opened::status();
    if (state.generation != generation || state.uuid != uuid || state.sha256 != sha ||
        !opened::validate_reader(reader, generation, error)) return reject(error, "owned native load reader changed: " + error);
    return true;
}
DWORD cast_native(void* object, uintptr_t source, ptrdiff_t hint, void*& typed) {
    __try { typed = reinterpret_cast<void*(*)(void*, const void*, const void*, ptrdiff_t)>(image + 0x12654f0)(
        object, reinterpret_cast<void*>(image + source), reinterpret_cast<void*>(image + 0x1655090), hint); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool typed_native(void* object, std::string& error) {
    void* actual = nullptr; const auto code = cast_native(object, 0x1657270, -1, actual);
    if (code || actual != object) { error = "native Building RTTI differs exception=" + std::to_string(code); return false; }
    return true;
}
DWORD lookup_native_call(void* context, int32_t id, void*& actual) {
    __try { actual = reinterpret_cast<void*(*)(void*, int32_t)>(image + 0x89e0f0)(context, id); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool lookup_native(void* context, int32_t id, void*& actual, std::string& error) {
    const auto code = lookup_native_call(context, id, actual);
    if (code) { error = "native context lookup exception=" + std::to_string(code); return false; }
    return true;
}
DWORD registry_native(uint64_t uid, void*& actual) {
    __try {
        auto* object = reinterpret_cast<void*(*)(uint64_t)>(image + 0x47ce20)(uid);
        if (!object) { actual = nullptr; return 0; }
        return cast_native(object, 0x1673900, 0, actual);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool readback_native(uint64_t uid, void* expected, native::BuildingState& building, std::string& error) {
    void* actual = nullptr; const auto code = registry_native(uid, actual);
    if (code || actual != expected) { error = "loaded UID registry pointer differs uid=" + std::to_string(uid) + " exception=" + std::to_string(code); return false; }
    return native::read_building(uid, building, error);
}
using Typed = bool(*)(void*, std::string&);
using Lookup = bool(*)(void*, int32_t, void*&, std::string&);
using Readback = bool(*)(uint64_t, void*, native::BuildingState&, std::string&);
Typed typed = typed_native;
Lookup lookup = lookup_native;
Readback readback = readback_native;
Frame* top(std::string& error) {
    auto stack = stacks.find(GetCurrentThreadId());
    if (stack == stacks.end() || stack->second.empty()) { reject(error, "native Building child outside owned load array frame"); return nullptr; }
    return &pending.at(stack->second.back());
}
bool bounds(void* array, void*& data, int32_t& count, std::string& error) {
    int32_t capacity = 0;
    if (!type(image, array, 0x172d7c0, error) || !read(array, 8, data, error) ||
        !read(array, 0x10, count, error) || !read(array, 0x14, capacity, error)) return false;
    if (count < 0 || count > 4096 || capacity < count) { error = "native Building array count/capacity invalid required_max=4096"; return false; }
    return true;
}
}
bool initialize(std::string& error) {
    std::lock_guard lock(mutex);
    if (!native::ready()) return reject(error, "native exact-build resolver unavailable");
    image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const std::array<std::string_view, 3> guards{
        std::string_view("\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x78", 16),
        std::string_view("\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x28", 12),
        std::string_view("\x56\x53\x48\x83\xec\x28\x48\x8b\x01", 9)};
    for (size_t index = 0; index < entries.size(); ++index) {
        std::array<char, 16> actual{}; MEMORY_BASIC_INFORMATION memory{};
        auto* address = reinterpret_cast<void*>(image + entries[index]);
        if (!VirtualQuery(address, &memory, sizeof(memory)) || memory.Type != MEM_IMAGE || memory.AllocationBase != reinterpret_cast<void*>(image) ||
            memory.Protect != PAGE_EXECUTE_READ || !bytes(address, 0, actual.data(), guards[index].size(), error) ||
            std::memcmp(actual.data(), guards[index].data(), guards[index].size())) return reject(error, "root load entry guard differs rva=" + std::to_string(entries[index]) + " cause=" + error);
    }
    error.clear(); return true;
}
bool confirm_hooks(uint32_t mask, std::string& error) {
    std::lock_guard lock(mutex);
    if (!image || mask != 7 || !failure.empty()) return reject(error, "root load hook receipt incomplete required=7 actual=" + std::to_string(mask));
    hooks = mask; error.clear(); return true;
}
void* entrypoint(size_t index) { return image && index < entries.size() ? reinterpret_cast<void*>(image + entries[index]) : nullptr; }
bool begin(const std::string& digest, const std::string& capture, std::string& error) {
    std::lock_guard lock(mutex);
    if (!image || hooks != 7 || armed) return reject(error, "root load begin requires all hooks and no active load");
    const auto origin = opened::status();
    if (!origin.initialized || origin.hook_mask != opened::required_hooks || origin.state != "pinned" || !origin.held ||
        origin.uuid != capture || origin.sha256 != digest || origin.hash_bytes != origin.bytes || !origin.generation)
        return reject(error, "root load requires pinned exact NativeOpen file receipt");
    sha = digest; uuid = capture; generation = origin.generation; failure.clear(); emergency = false;
    pending.clear(); stacks.clear(); saved.clear(); bound_context = nullptr; rows_seen = 0;
    main_thread = GetCurrentThreadId(); armed = true; error.clear(); return true;
}
Ticket before(void* array, void* reader, void* context) noexcept try {
    if (!armed) return 0;
    std::lock_guard lock(mutex); std::string error;
    if (!failure.empty() || emergency || !live(reader, error)) return 0;
    void* mode = nullptr; void* owned_context = nullptr; void* data = nullptr; int32_t count = -1;
    if (!read(reinterpret_cast<void*>(image), 0x13df320, mode, error) || !type(image, mode, 0x16e6730, error) ||
        !read(mode, 0xd8, owned_context, error) || owned_context != context || !context || (bound_context && context != bound_context) ||
        !bounds(array, data, count, error) || count != 0) { reject(error, "native load array requires empty array and current Mode context: " + error); return 0; }
    Frame frame; frame.array = array; frame.reader = reader; frame.context = context; frame.thread = GetCurrentThreadId();
    if (!opened::array_record(reader, generation, opened::ArrayKind::Building, frame.declared, error) || frame.declared < 0 || frame.declared > 4096) {
        reject(error, "owned Building record count invalid: " + error); return 0;
    }
    if (pending.size() >= 256 || saved.size() >= 4096 || sequence == UINT64_MAX) { reject(error, "native load diagnostic frame/array limit exceeded"); return 0; }
    bound_context = context; const auto ticket = ++sequence;
    pending.emplace(ticket, std::move(frame)); stacks[GetCurrentThreadId()].push_back(ticket); return ticket;
} catch (const std::exception& e) { caught(e.what()); return 0; } catch (...) { caught("non-standard before exception"); return 0; }
void children(uintptr_t caller, void* reader, int32_t count) noexcept try {
    if (!armed || caller != 0xc0ab73) return;
    std::lock_guard lock(mutex); std::string error; if (!failure.empty() || emergency) return;
    auto* frame = top(error); if (!frame) return;
    if (frame->reader != reader || frame->counted || frame->declared != count || !live(reader, error)) {
        reject(error, "native Building BeginChildren count or reader differs: " + error); return;
    }
    frame->counted = true;
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard count exception"); }
void registered(uintptr_t caller, void* context, int32_t id, void* object) noexcept try {
    if (!armed || (caller != 0xc0ae44 && caller != 0xc0aef5)) return;
    std::lock_guard lock(mutex); std::string error; if (!failure.empty() || emergency) return;
    auto* frame = top(error); if (!frame) return;
    void* actual = nullptr; void* object_context = nullptr; const bool positive = caller == 0xc0ae44;
    if (context != frame->context || !frame->counted || (positive ? id <= 0 : id != 0) || !typed(object, error) ||
        !read(object, 8, object_context, error) || object_context != context || !live(frame->reader, error) ||
        (positive && (!lookup(context, id, actual, error) || actual != object)) || frame->registrations.size() >= size_t(frame->declared) ||
        !frame->registrations.emplace(object, id).second) reject(error, "native Building child registration invalid: " + error);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard registration exception"); }
void after(Ticket ticket, void* array, void* reader, void* context) noexcept try {
    if (!ticket || !armed) return;
    std::lock_guard lock(mutex); std::string error; if (!failure.empty() || emergency) return;
    const auto found = pending.find(ticket); auto* frame = top(error); if (!frame) return;
    if (found == pending.end() || frame != &found->second || frame->array != array || frame->reader != reader || frame->context != context ||
        frame->thread != GetCurrentThreadId() || !frame->counted || !live(reader, error)) { reject(error, "native load array return lifetime differs: " + error); return; }
    void* data = nullptr; int32_t count = -1;
    if (!bounds(array, data, count, error) || count != frame->declared || size_t(count) != frame->registrations.size()) {
        reject(error, "native Building child skip/count differs actual=" + std::to_string(count) + " required=" + std::to_string(frame->declared) + " cause=" + error); return;
    }
    if (rows_seen + size_t(count) > 4096 || std::any_of(saved.begin(), saved.end(), [&](const Saved& value) { return value.array == array; })) {
        reject(error, "duplicate loaded Building array or building budget exceeded"); return;
    }
    Saved item; item.array = array; std::set<void*> pointers;
    for (int32_t index = 0; index < count; ++index) {
        Row row; row.child = index;
        if (!read(data, size_t(index) * 8, row.pointer, error) || !typed(row.pointer, error) || !read(row.pointer, 0x18, row.uid, error) ||
            !row.uid || !pointers.insert(row.pointer).second || !frame->registrations.contains(row.pointer)) { reject(error, "loaded Building child order/type/UID differs: " + error); return; }
        row.id = frame->registrations.at(row.pointer); item.rows.push_back(row);
    }
    rows_seen += item.rows.size(); saved.push_back(std::move(item));
    auto& stack = stacks.at(GetCurrentThreadId()); stack.pop_back(); if (stack.empty()) stacks.erase(GetCurrentThreadId()); pending.erase(found);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard after exception"); }
void original_failed(Ticket ticket, const std::string& cause) noexcept try {
    if (!ticket) return;
    std::lock_guard lock(mutex); std::string error; reject(error, "native Building load original failed: " + cause);
} catch (const std::exception& e) { caught(e.what()); } catch (...) { caught("non-standard original failure reporting"); }
bool finalize(std::vector<normal_load_identity::Binding>& result, std::string& error) {
    std::lock_guard lock(mutex); result.clear();
    if (emergency) return reject(error, "native Building observer exception; original cause retained in debug output");
    if (!failure.empty()) { error = failure; return false; }
    if (!armed || GetCurrentThreadId() != main_thread || !pending.empty() || !bound_context || saved.empty())
        return reject(error, "native load finalization lacks complete array returns on game thread");
    opened::CurrentWorld world;
    if (!opened::read_completed_world(generation, world, error) || world.uuid != uuid || world.sha256 != sha)
        return reject(error, "native Building load completed world differs: " + error);
    std::set<std::string> keys; std::set<uint64_t> uids; std::set<void*> pointers;
    for (const auto& array : saved) {
        void* data = nullptr; int32_t count = 0;
        if (!bounds(array.array, data, count, error) || size_t(count) != array.rows.size()) return reject(error, "loaded Building array changed before finalization: " + error);
        for (const auto& row : array.rows) {
            void* pointer = nullptr; native::BuildingState building;
            if (!read(data, size_t(row.child) * 8, pointer, error) || pointer != row.pointer ||
                !readback(row.uid, pointer, building, error) || building.prop.uid != row.uid)
                return reject(error, "loaded Building final registry/readback failed uid=" + std::to_string(row.uid) + " cause=" + error);
            std::string suffix = std::to_string(row.id);
            if (!row.id) {
                if (!saved_building_path::read(image,world.map,pointer,row.child,suffix,error,array.array))
                    return reject(error,"loaded inline Building owner rejected: "+error);
            }
            const auto key = sha + ":world:building:" + suffix;
            if (!keys.insert(key).second || !uids.insert(row.uid).second || !pointers.insert(pointer).second)
                return reject(error, "loaded Building has duplicate saved key/UID/pointer");
            result.push_back({row.id, row.uid, key, std::move(building)});
        }
    }
    // Every root manager, including empty arrays, must have a native load receipt.
    auto* root_regions = static_cast<char*>(world.map) + 0x230;
    void* region_data = nullptr; int32_t region_count = 0, region_capacity = 0;
    if (!type(image, world.map, 0x1742860, error) || !type(image, root_regions, 0x16f04e0, error) ||
        !read(root_regions, 8, region_data, error) || !read(root_regions, 0x10, region_count, error) ||
        !read(root_regions, 0x14, region_capacity, error) || region_count < 0 || region_count > 4096 || region_capacity < region_count)
        return reject(error, "loaded saved root region coverage bounds invalid: " + error);
    for (int32_t index = 0; index < region_count; ++index) {
        void* region = nullptr; void* manager = nullptr;
        if (!read(region_data, size_t(index) * 8, region, error) || !type(image, region, 0x1787930, error) ||
            !read(region, 0x950, manager, error) || !type(image, manager, 0x17183c0, error))
            return reject(error, "loaded root manager coverage read failed: " + error);
        const auto array = reinterpret_cast<uintptr_t>(manager) + 0x408;
        if (std::none_of(saved.begin(), saved.end(), [&](const Saved& value) { return reinterpret_cast<uintptr_t>(value.array) == array; }))
            return reject(error, "loaded saved root Building array lacks native receipt region_child=" + std::to_string(index));
    }
    armed = false; error.clear();
    LOG_INFO("[RootBuildingLoad] Native file/array/registry bindings confirmed snapshot={} arrays={} buildings={}", uuid, saved.size(), result.size());
    return true;
}
void cancel(const std::string& cause) {
    std::lock_guard lock(mutex); armed = false; pending.clear(); stacks.clear(); saved.clear(); bound_context = nullptr;
    LOG_INFO("[RootBuildingLoad] Cancelled snapshot={} cause={}", uuid, cause);
}
#ifdef MT2MP_ROOT_LOAD_TEST
namespace testing {
void backend(uintptr_t base, Typed type_fn, Lookup lookup_fn, Readback reader) {
    std::lock_guard lock(mutex); image = base; hooks = 7; typed = type_fn; lookup = lookup_fn; readback = reader;
}
}
#endif
}

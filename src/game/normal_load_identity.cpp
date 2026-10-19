#include "bug_authority/runtime.h"
#include "mt2mp/game/normal_load_identity.h"
#include "mt2mp/core/logger.h"
#include "root_building_load.h"
#include "npc_quest_target_runtime.h"
#include "npc_saved_identity_runtime.h"
#include "party_saved_identity_runtime.h"
#include "monster_saved_identity_runtime.h"
#include "boss_saved_identity_runtime.h"
#include "dungeon_scene_identity_runtime.h"
#include "district_saved_identity_runtime.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <exception>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace mt2mp::game::normal_load_identity {
namespace {
constexpr uintptr_t add_object_rva = 0x89ea20;
constexpr uintptr_t lookup_id_rva = 0x89e0f0;
constexpr uintptr_t dynamic_cast_rva = 0x12654f0;
constexpr uintptr_t null_typeinfo = 0x1657270;
constexpr uintptr_t building_typeinfo = 0x1655090;
constexpr uintptr_t find_uid_rva = 0x47ce20;
constexpr size_t maximum_references = 4096;
constexpr size_t maximum_contexts = 64;
constexpr uint64_t timing_stride = 1024;
constexpr size_t maximum_negative_classes = 512;
constexpr size_t maximum_metadata_bytes = 131072;
struct ImageRange { uintptr_t start; size_t bytes; bool executable = false; };
struct Protection { void* start; size_t bytes; DWORD protect; };
struct MetadataBlock { uintptr_t address; std::vector<unsigned char> bytes; };
struct MetadataProof { std::vector<MetadataBlock> blocks; size_t bytes = 0; };
struct NegativeClass { uintptr_t typeinfo; std::vector<Protection> protections; MetadataProof metadata; };
std::vector<ImageRange> readonly_ranges;
std::unordered_map<uintptr_t, NegativeClass> negative_classes;
std::unordered_map<uintptr_t, std::string> failed_rtti_attempts;
uintptr_t class_vptr = 0, si_vptr = 0, vmi_vptr = 0, source_typeinfo = 0, target_typeinfo = 0;
bool cache_recovery_reported = false;
bool negative_cache_enabled = false;
struct Observation { void* pointer = nullptr; uint64_t uid = 0; };
using References = std::map<int32_t, Observation>;
std::map<void*, References> contexts;
std::mutex mutex;
uintptr_t image = 0;
Status current;
std::map<std::string,uint64_t> fallback_categories;
uint64_t fallback_overflow_count=0;
bool active = false;
struct ObserverSample {
    bool sampled;
    std::chrono::steady_clock::time_point started;
    explicit ObserverSample(uint64_t number) : sampled((number - 1) % timing_stride == 0) {
        if (sampled) started = std::chrono::steady_clock::now();
    }
    ~ObserverSample() {
        if (!sampled) return;
        const auto elapsed = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started).count());
        ++current.observer_sample_count;
        current.observer_sample_total_ns += elapsed;
        current.observer_sample_max_ns = std::max(current.observer_sample_max_ns, elapsed);
    }
};
using Registration = bool(*)(void*, int32_t, void*, uint64_t&, std::string&);
using ReadBuilding = bool(*)(uint64_t, native::BuildingState&, std::string&);

bool readable(const void* pointer, size_t bytes) {
    if (!pointer || !bytes) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(pointer, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto address = reinterpret_cast<uintptr_t>(pointer);
    const auto start = reinterpret_cast<uintptr_t>(memory.BaseAddress);
    return address >= start && bytes <= memory.RegionSize && address - start <= memory.RegionSize - bytes;
}

DWORD copy_metadata(const void* source, void* target, size_t bytes) {
    __try { std::memcpy(target, source, bytes); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

bool readonly_image(uintptr_t address, size_t bytes, std::vector<Protection>& proofs, std::string& cause) {
    const auto found = std::find_if(readonly_ranges.begin(), readonly_ranges.end(), [=](const ImageRange& range) {
        return address >= range.start && bytes <= range.bytes && address - range.start <= range.bytes - bytes;
    });
    if (found == readonly_ranges.end()) { cause = "RTTI metadata outside readonly exact EXE section"; return false; }
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory))) {
        cause = "RTTI metadata VirtualQuery failed WinError=" + std::to_string(GetLastError()); return false;
    }
    if (memory.State != MEM_COMMIT || (memory.Protect != PAGE_READONLY && memory.Protect != PAGE_EXECUTE_READ) ||
        address < reinterpret_cast<uintptr_t>(memory.BaseAddress) || bytes > memory.RegionSize ||
        address - reinterpret_cast<uintptr_t>(memory.BaseAddress) > memory.RegionSize - bytes) {
        cause = "RTTI metadata live page is not entirely readonly actual_protect=" + std::to_string(memory.Protect) +
                " actual_state=" + std::to_string(memory.State); return false;
    }
#ifndef MT2MP_IDENTITY_TEST
    if (memory.Type != MEM_IMAGE || memory.AllocationBase != reinterpret_cast<void*>(image)) {
        cause = "RTTI metadata is not owned by exact EXE image"; return false;
    }
#endif
    if (std::none_of(proofs.begin(), proofs.end(), [&](const Protection& value) { return value.start == memory.BaseAddress; })) {
        if (proofs.size() >= 8) { cause = "RTTI protection region budget exceeded required_max=8"; return false; }
        proofs.push_back({memory.BaseAddress, memory.RegionSize, memory.Protect});
    }
    return true;
}

bool read_metadata(uintptr_t address, void* target, size_t bytes, std::vector<Protection>& proofs, std::string& cause,
                   MetadataProof* snapshot = nullptr) {
    if (!readonly_image(address, bytes, proofs, cause)) return false;
    const auto exception = copy_metadata(reinterpret_cast<void*>(address), target, bytes);
    if (exception) { cause = "RTTI metadata read exception=" + std::to_string(exception); return false; }
    if (snapshot) {
        if (bytes > 16384 - snapshot->bytes) { cause = "RTTI metadata byte budget actual=" + std::to_string(snapshot->bytes + bytes) + " required_max=16384 per class"; return false; }
        const auto* source = static_cast<const unsigned char*>(target);
        if (!snapshot->blocks.empty() && snapshot->blocks.back().address + snapshot->blocks.back().bytes.size() == address)
            snapshot->blocks.back().bytes.insert(snapshot->blocks.back().bytes.end(), source, source + bytes);
        else snapshot->blocks.push_back({address, std::vector<unsigned char>(source, source + bytes)});
        snapshot->bytes += bytes;
    }
    return true;
}

DWORD compare_metadata(uintptr_t address, const void* expected, size_t bytes, bool& same) {
    __try { same = std::memcmp(reinterpret_cast<void*>(address), expected, bytes) == 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

bool metadata_unchanged(const MetadataProof& snapshot, std::string& cause) {
    for (const auto& block : snapshot.blocks) {
        bool same = false;
        const auto exception = compare_metadata(block.address, block.bytes.data(), block.bytes.size(), same);
        if (exception) { cause = "cached RTTI metadata read exception=" + std::to_string(exception); return false; }
        if (!same) { cause = "cached readonly RTTI hierarchy bytes changed"; return false; }
    }
    return true;
}

bool protections_unchanged(const std::vector<Protection>& proofs, std::string& cause) {
    for (const auto& proof : proofs) {
        MEMORY_BASIC_INFORMATION memory{};
        if (!VirtualQuery(proof.start, &memory, sizeof(memory))) {
            cause = "cached RTTI VirtualQuery failed WinError=" + std::to_string(GetLastError()); return false;
        }
        if (memory.State != MEM_COMMIT || memory.BaseAddress != proof.start || memory.RegionSize != proof.bytes || memory.Protect != proof.protect) {
            cause = "cached RTTI protection changed actual_protect=" + std::to_string(memory.Protect) +
                    " required_protect=" + std::to_string(proof.protect) + " actual_region_bytes=" + std::to_string(memory.RegionSize) +
                    " required_region_bytes=" + std::to_string(proof.bytes); return false;
        }
#ifndef MT2MP_IDENTITY_TEST
        if (memory.Type != MEM_IMAGE || memory.AllocationBase != reinterpret_cast<void*>(image)) {
            cause = "cached RTTI page no longer belongs to exact EXE image"; return false;
        }
#endif
    }
    return true;
}

struct Hierarchy {
    std::vector<uintptr_t> visited;
    std::array<uintptr_t, 33> ancestors{};
    std::vector<Protection> proofs;
    MetadataProof metadata;
    bool building = false, source = false;
};

bool inspect_hierarchy(uintptr_t node, size_t depth, Hierarchy& graph, std::string& cause) {
    if (depth > 32) { cause = "RTTI hierarchy depth exceeds required_max=32"; return false; }
    if (std::find(graph.ancestors.begin(), graph.ancestors.begin() + depth, node) != graph.ancestors.begin() + depth) {
        cause = "RTTI hierarchy contains a cycle"; return false;
    }
    if (std::find(graph.visited.begin(), graph.visited.end(), node) != graph.visited.end()) return true;
    if (graph.visited.size() >= 256) { cause = "RTTI hierarchy node budget exceeds required_max=256"; return false; }
    graph.visited.push_back(node);
    graph.ancestors[depth] = node;
    std::array<uintptr_t, 2> header{};
    if (node % alignof(uintptr_t) || !read_metadata(node, header.data(), sizeof(header), graph.proofs, cause, &graph.metadata)) {
        if (cause.empty()) cause = "RTTI typeinfo alignment differs";
        return false;
    }
    if (header[0] != class_vptr && header[0] != si_vptr && header[0] != vmi_vptr) {
        cause = "RTTI layout is not supported GCC class/si/vmi"; return false;
    }
    bool terminated = false;
    std::array<char, 512> name{};
    for (size_t index = 0; index < 512; ++index) {
        unsigned char byte = 0;
        if (header[1] > UINTPTR_MAX - index || !read_metadata(header[1] + index, &byte, 1, graph.proofs, cause, &graph.metadata)) return false;
        if (!byte) { terminated = true; break; }
        if (byte < 32 || byte > 126) { cause = "RTTI mangled name contains unsupported byte"; return false; }
        name[index] = static_cast<char>(byte);
    }
    if (!terminated) { cause = "RTTI name lacks terminator required_max=512"; return false; }
    graph.building |= node == target_typeinfo || std::strcmp(name.data(), "11mmoBuilding") == 0;
    graph.source |= node == source_typeinfo || std::strcmp(name.data(), "12vsNullObject") == 0;
    if (header[0] == class_vptr) return true;
    if (header[0] == si_vptr) {
        uintptr_t base = 0;
        return read_metadata(node + 16, &base, sizeof(base), graph.proofs, cause, &graph.metadata) && inspect_hierarchy(base, depth + 1, graph, cause);
    }
    std::array<uint32_t, 2> flags{};
    if (!read_metadata(node + 16, flags.data(), sizeof(flags), graph.proofs, cause, &graph.metadata)) return false;
    if ((flags[0] & ~3u) || flags[1] > 64) { cause = "RTTI vmi flags/base budget differs required_max_bases=64"; return false; }
    for (uint32_t index = 0; index < flags[1]; ++index) {
        std::array<uintptr_t, 2> base{};
        if (!read_metadata(node + 24 + index * 16, base.data(), sizeof(base), graph.proofs, cause, &graph.metadata)) return false;
        if ((base[1] & 255u) & ~uintptr_t(3)) { cause = "RTTI base flags are unsupported"; return false; }
        if (!inspect_hierarchy(base[0], depth + 1, graph, cause)) return false;
    }
    return true;
}

bool remember_rtti_failure(uintptr_t vptr, const std::string& cause) {
    if (failed_rtti_attempts.size() < maximum_negative_classes) {
        failed_rtti_attempts.emplace(vptr, cause);
        current.failed_rtti_memo_entries = failed_rtti_attempts.size();
        current.failed_rtti_memo_peak_entries = std::max(current.failed_rtti_memo_peak_entries, current.failed_rtti_memo_entries);
    } else ++current.failed_rtti_memo_overflows;
    return false;
}

bool negative_class(void* object, std::string& cause) {
    if (readonly_ranges.empty()) { cause = "readonly RTTI image resolver is unavailable"; return false; }
    uintptr_t vptr = 0;
    const auto exception = copy_metadata(object, &vptr, sizeof(vptr));
    if (exception) { cause = "object vptr read exception=" + std::to_string(exception); return false; }
    if (vptr < 16 || vptr % alignof(uintptr_t)) { cause = "object vptr alignment/header differs"; return false; }
    const auto failed = failed_rtti_attempts.find(vptr);
    if (failed != failed_rtti_attempts.end()) {
        ++current.failed_rtti_memo_hits;
        cause = failed->second;
        return false;
    }
    auto cached = negative_classes.find(vptr);
    if (cached != negative_classes.end()) {
        std::array<uintptr_t, 2> header{};
        bool matches = protections_unchanged(cached->second.protections, cause) && metadata_unchanged(cached->second.metadata, cause);
        if (matches) {
            const auto exception = copy_metadata(reinterpret_cast<void*>(vptr - 16), header.data(), sizeof(header));
            if (exception) { cause = "cached vtable header read exception=" + std::to_string(exception); matches = false; }
            else if (header[0] != 0 || header[1] != cached->second.typeinfo) { cause = "cached vtable header or typeinfo changed"; matches = false; }
        }
        if (matches) {
            ++current.negative_cache_hits;
            if (current.rtti_fallbacks && !cache_recovery_reported) {
                cache_recovery_reported = true;
                LOG_INFO("[LoadIdentity] Other negative RTTI classes remain available while failed classes use native checks count={}", current.rtti_fallbacks);
            }
            return true;
        }
        current.negative_cache_bytes -= cached->second.metadata.bytes;
        negative_classes.erase(cached);
        current.negative_cache_entries = negative_classes.size();
        ++current.negative_cache_invalidations;
        return remember_rtti_failure(vptr, cause);
    }
    ++current.negative_cache_misses;
    ++current.rtti_graph_attempts;
    Hierarchy graph;
    std::array<uintptr_t, 2> header{};
    if (!read_metadata(vptr - 16, header.data(), sizeof(header), graph.proofs, cause, &graph.metadata) ||
        !readonly_image(vptr, sizeof(uintptr_t), graph.proofs, cause)) return remember_rtti_failure(vptr, cause);
    if (header[0]) { cause = "secondary/construction vtable requires native classification"; return remember_rtti_failure(vptr, cause); }
    uintptr_t function = 0;
    if (!read_metadata(vptr, &function, sizeof(function), graph.proofs, cause, &graph.metadata)) return remember_rtti_failure(vptr, cause);
    if (std::none_of(readonly_ranges.begin(), readonly_ranges.end(), [=](const ImageRange& range) {
        return range.executable && function >= range.start && function - range.start < range.bytes;
    }) || !readonly_image(function, 1, graph.proofs, cause)) {
        if (cause.empty()) cause = "vtable function is outside readonly executable image code";
        return remember_rtti_failure(vptr, cause);
    }
    if (!inspect_hierarchy(header[1], 0, graph, cause)) return remember_rtti_failure(vptr, cause);
    if (graph.building) return false;
    if (!graph.source) { cause = "RTTI graph lacks vsNullObject source base"; return remember_rtti_failure(vptr, cause); }
    if (negative_classes.size() >= maximum_negative_classes) { cause = "negative RTTI cache capacity actual=" + std::to_string(negative_classes.size() + 1) + " required_max=512"; return remember_rtti_failure(vptr, cause); }
    if (graph.metadata.bytes > maximum_metadata_bytes - current.negative_cache_bytes) { cause = "negative RTTI cache metadata capacity actual=" + std::to_string(current.negative_cache_bytes + graph.metadata.bytes) + " required_max=131072"; return remember_rtti_failure(vptr, cause); }
    current.negative_cache_bytes += graph.metadata.bytes;
    negative_classes.emplace(vptr, NegativeClass{header[1], std::move(graph.proofs), std::move(graph.metadata)});
    current.negative_cache_entries = negative_classes.size();
    current.negative_cache_peak_entries = std::max(current.negative_cache_peak_entries, current.negative_cache_entries);
    return true;
}

void record_fallback(const std::string& cause) {
    ++current.rtti_fallbacks;
    current.rtti_fallback_reason = cause;
    std::string category;
    bool number=false;
    for(char byte:cause) {
        if(byte>='0'&&byte<='9') { if(!number) category.push_back('#'); number=true; }
        else { category.push_back(byte); number=false; }
    }
    auto found=fallback_categories.find(category);
    uint64_t count=0;
    if(found!=fallback_categories.end()) count=++found->second;
    else if(fallback_categories.size()<64) count=++fallback_categories.emplace(category,0).first->second;
    else { category="additional RTTI fallback categories"; count=++fallback_overflow_count; }
    if((count&(count-1))==0)
        LOG_WARN("[LoadIdentity] RTTI fast path skipped cause={} category={} category_count={} total={}; native registration check remains active",cause,category,count,current.rtti_fallbacks);
}

DWORD registration_native(void* context, int32_t id, void* object, uint64_t& uid,
                          bool& registered, bool& building) {
    __try {
        registered = reinterpret_cast<void*(*)(void*, int32_t)>(image + lookup_id_rva)(context, id) == object;
        if (!registered) return 0;
        auto* typed = reinterpret_cast<void*(*)(void*, const void*, const void*, ptrdiff_t)>(image + dynamic_cast_rva)(
            object, reinterpret_cast<void*>(image + null_typeinfo), reinterpret_cast<void*>(image + building_typeinfo), -1);
        building = typed != nullptr;
        if (typed) std::memcpy(&uid, static_cast<const unsigned char*>(typed) + 0x18, sizeof(uid));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

DWORD registered_building_native(uint64_t uid, void*& pointer) {
    __try {
        auto* object = reinterpret_cast<void*(*)(uint64_t)>(image + find_uid_rva)(uid);
        pointer = object ? reinterpret_cast<void*(*)(void*, const void*, const void*, ptrdiff_t)>(image + dynamic_cast_rva)(
            object, reinterpret_cast<void*>(image + 0x1673900), reinterpret_cast<void*>(image + building_typeinfo), 0) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

bool registration_checked(void* context, int32_t id, void* object, uint64_t& uid, std::string& error) {
    if (!readable(context, sizeof(void*)) || !readable(object, 0x20)) {
        error = "context or registered object is unreadable";
        return false;
    }
    bool registered = false, building = false;
    const DWORD exception = registration_native(context, id, object, uid, registered, building);
    if (exception) { error = "registration RTTI read exception=" + std::to_string(exception); return false; }
    if (!registered) { error = "native context lookup differs from original AddObject object"; return false; }
    if (building && !uid) { error = "typed building has invalid zero UID"; return false; }
    return true;
}

Registration registration = registration_checked;
ReadBuilding read_building = native::read_building;

bool fail(std::string& error, const std::string& message) {
    error = message;
    LOG_ERROR("[LoadIdentity] {}", message);
    return false;
}

void fail_capture(const std::string& message) {
    if (current.state != "failed") {
        current.state = "failed";
        current.error = message;
        LOG_ERROR("[LoadIdentity] Capture failed snapshot={} cause={}", current.snapshot_uuid, message);
    }
    active = false;
}

bool valid_sha(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

bool valid_uuid(const std::string& value) {
    if (value.size() != 36) return false;
    for (size_t i = 0; i < value.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (value[i] != '-') return false; }
        else if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
    }
    return true;
}
} // namespace

bool initialize(std::string& error) {
    std::lock_guard lock(mutex);
    if (!native::ready()) return fail(error, "initialize rejected: exact-build native resolver is not ready");
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!readable(reinterpret_cast<void*>(base), sizeof(IMAGE_DOS_HEADER)))
        return fail(error, "initialize rejected: executable header is unreadable");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096)
        return fail(error, "initialize rejected: executable DOS header differs");
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (!readable(pe, sizeof(*pe)) || pe->Signature != IMAGE_NT_SIGNATURE ||
        pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || pe->OptionalHeader.SizeOfImage != 30986240)
        return fail(error, "initialize rejected: exact-build PE contract differs");
    constexpr std::array<unsigned char, 12> add_prefix{0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x58};
    constexpr std::array<unsigned char, 10> cast_prefix{0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x70};
    constexpr std::array<unsigned char, 9> lookup_prefix{0x53,0x48,0x8b,0x09,0x48,0x83,0x79,0x18,0x00};
    constexpr std::array<unsigned char, 7> uid_prefix{0x53,0x45,0x31,0xc0,0x48,0x85,0xc9};
    if (!readable(reinterpret_cast<void*>(base + add_object_rva), add_prefix.size()) ||
        std::memcmp(reinterpret_cast<void*>(base + add_object_rva), add_prefix.data(), add_prefix.size()) != 0)
        return fail(error, "initialize rejected: AddObject prologue differs");
    if (!readable(reinterpret_cast<void*>(base + dynamic_cast_rva), cast_prefix.size()) ||
        std::memcmp(reinterpret_cast<void*>(base + dynamic_cast_rva), cast_prefix.data(), cast_prefix.size()) != 0)
        return fail(error, "initialize rejected: GCC dynamic_cast prologue differs");
    if (!readable(reinterpret_cast<void*>(base + lookup_id_rva), lookup_prefix.size()) ||
        std::memcmp(reinterpret_cast<void*>(base + lookup_id_rva), lookup_prefix.data(), lookup_prefix.size()) != 0)
        return fail(error, "initialize rejected: reference lookup prologue differs");
    if (!readable(reinterpret_cast<void*>(base + find_uid_rva), uid_prefix.size()) ||
        std::memcmp(reinterpret_cast<void*>(base + find_uid_rva), uid_prefix.data(), uid_prefix.size()) != 0)
        return fail(error, "initialize rejected: UID registry lookup prologue differs");
    image = base;
    negative_cache_enabled = false;
    readonly_ranges.clear();
    negative_classes.clear();
    failed_rtti_attempts.clear();
    current.failed_rtti_memo_entries = 0;
    current.negative_cache_bytes = 0;
    class_vptr = image + 0x17888c0;
    si_vptr = image + 0x17889f0;
    vmi_vptr = image + 0x1788a50;
    source_typeinfo = image + null_typeinfo;
    target_typeinfo = image + building_typeinfo;
    const auto* sections = IMAGE_FIRST_SECTION(pe);
    if (pe->FileHeader.NumberOfSections <= 96 && readable(sections, size_t(pe->FileHeader.NumberOfSections) * sizeof(*sections))) {
        for (size_t index = 0; index < pe->FileHeader.NumberOfSections; ++index) {
            const auto& section = sections[index];
            const size_t bytes = std::max(section.Misc.VirtualSize, section.SizeOfRawData);
            if ((section.Characteristics & IMAGE_SCN_MEM_READ) && !(section.Characteristics & IMAGE_SCN_MEM_WRITE) &&
                section.VirtualAddress < pe->OptionalHeader.SizeOfImage && bytes <= pe->OptionalHeader.SizeOfImage - section.VirtualAddress)
                readonly_ranges.push_back({image + section.VirtualAddress, bytes, bool(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)});
        }
    }
    if (readonly_ranges.empty()) LOG_WARN("[LoadIdentity] Readonly RTTI image sections unavailable; all registrations retain native checks");
    error.clear();
    LOG_INFO("[LoadIdentity] Exact-build reference observer ready");
    return true;
}

void* entrypoint() { std::lock_guard lock(mutex); return image ? reinterpret_cast<void*>(image + add_object_rva) : nullptr; }

bool begin(const std::string& sha, const std::string& uuid, std::string& error) {
    std::lock_guard lock(mutex);
    if (!image) return fail(error, "begin rejected: reference observer resolver is not ready");
    if (current.load_pending) return fail(error, "begin rejected: previous normal load capture is still active");
    if (!valid_sha(sha) || !valid_uuid(uuid)) return fail(error, "begin rejected: snapshot SHA256 or UUID is invalid");
#ifndef MT2MP_IDENTITY_TEST
    if (!root_building_load::begin(sha, uuid, error)) return fail(error, "native array load identity arm failed: " + error);
    if (!npc_quest_target_runtime::arm_client(uuid,sha,error)) { root_building_load::cancel(error); return fail(error,"Quest target load arm failed: "+error); }
    if (!npc_saved_identity_runtime::arm_client(uuid,sha,error)) { root_building_load::cancel(error); npc_quest_target_runtime::cancel(error); return fail(error,"NPC identity load arm failed: "+error); }
    if(!monster_saved_identity_runtime::arm_client(uuid,sha,error)){root_building_load::cancel(error);npc_quest_target_runtime::cancel(error);npc_saved_identity_runtime::cancel(error);return fail(error,"Monster identity load arm failed: "+error);}
    if(!boss_saved_identity_runtime::arm_client(uuid,sha,error)){root_building_load::cancel(error);npc_quest_target_runtime::cancel(error);npc_saved_identity_runtime::cancel(error);monster_saved_identity_runtime::cancel(error);boss_saved_identity_runtime::cancel(error);return fail(error,"Boss identity load arm failed: "+error);}
    if(!district_saved_identity_runtime::arm_client(uuid,sha,error)){root_building_load::cancel(error);npc_quest_target_runtime::cancel(error);npc_saved_identity_runtime::cancel(error);monster_saved_identity_runtime::cancel(error);boss_saved_identity_runtime::cancel(error);return fail(error,"District identity load arm failed: "+error);}
    if(!party_saved_identity_runtime::arm_client(uuid,sha,error)){
        root_building_load::cancel(error);npc_quest_target_runtime::cancel(error);npc_saved_identity_runtime::cancel(error);
        monster_saved_identity_runtime::cancel(error);boss_saved_identity_runtime::cancel(error);district_saved_identity_runtime::cancel(error);
        return fail(error,"Party identity load arm failed: "+error);
    }
    if (!bug_authority::arm_snapshot(uuid,sha,false,error)) { root_building_load::cancel(error); npc_quest_target_runtime::cancel(error); npc_saved_identity_runtime::cancel(error); monster_saved_identity_runtime::cancel(error);boss_saved_identity_runtime::cancel(error); district_saved_identity_runtime::cancel(error); party_saved_identity_runtime::cancel(error); return fail(error,"Bug load arm failed: "+error); }
#endif
    contexts.clear();
    negative_classes.clear();
    failed_rtti_attempts.clear();
    cache_recovery_reported = false;
    current = {};
    current.negative_cache_enabled = negative_cache_enabled;
    current.classification_policy = negative_cache_enabled ? "experimental_checked_rtti_cache" : "native_checked";
    fallback_categories.clear(); fallback_overflow_count=0;
    current.snapshot_sha256 = sha;
    current.snapshot_uuid = uuid;
    current.state = "armed";
    current.load_pending = true;
    active = true;
    error.clear();
    LOG_INFO("[LoadIdentity] Normal load capture armed snapshot={} sha256={}", uuid, sha);
    if (!negative_cache_enabled) LOG_INFO("[LoadIdentity] Optional optimization skipped cause=live metadata validation overhead; all registrations use native checked lookup/RTTI");
    return true;
}

void observe_registered(void* context, int32_t id, void* object) noexcept {
    try {
        std::lock_guard lock(mutex);
        if (!active) return;
        ++current.registrations;
        ObserverSample timing(current.registrations);
        if (id <= 0) { fail_capture("registration ID invalid actual=" + std::to_string(id) + " required>0"); return; }
        if (!object || !context) { fail_capture("registration rejected: context and object must be non-null"); return; }
        auto found_context = contexts.find(context);
        const bool retained_slot = found_context != contexts.end() && found_context->second.contains(id);
        std::string cache_cause;
        const bool negative = negative_cache_enabled && !retained_slot && negative_class(object, cache_cause);
        if (!negative && !cache_cause.empty()) record_fallback(cache_cause);
        uint64_t uid = 0;
        std::string cause;
        if (!negative) {
            ++current.registration_checks;
            if (!registration(context, id, object, uid, cause)) { fail_capture("registration id=" + std::to_string(id) + ": " + cause); return; }
        }
        if (current.state == "armed") {
            current.state = "observing";
            LOG_INFO("[LoadIdentity] First normal load registration observed snapshot={} coverage=typed_buildings_only", current.snapshot_uuid);
        }
        if (!uid) {
            ++current.nonbuilding_registrations;
            if (found_context != contexts.end()) {
                if (found_context->second.erase(id)) {
                    --current.reference_count;
                    ++current.building_drops;
                    if (current.building_drops == 1 || current.building_drops % 1024 == 0)
                        LOG_INFO("[LoadIdentity] Native building slot replaced by non-building id={} drops={} retained_buildings={}", id, current.building_drops, current.reference_count);
                }
                if (found_context->second.empty()) contexts.erase(found_context);
                current.context_count = contexts.size();
            }
            return;
        }
        ++current.building_registrations;
        if (found_context != contexts.end()) {
            auto found = found_context->second.find(id);
            if (found != found_context->second.end()) {
                // Native AddObject replaces an existing ID slot at RVA 0x89eb0f.
                if (found->second.pointer != object || found->second.uid != uid) {
                    found->second = Observation{object, uid};
                    ++current.building_replacements;
                    if (current.building_replacements == 1 || current.building_replacements % 1024 == 0)
                        LOG_INFO("[LoadIdentity] Native building replacement accepted id={} replacements={}", id, current.building_replacements);
                }
                return;
            }
        }
        if (current.reference_count >= maximum_references) {
            fail_capture("retained building capacity exhausted actual=" + std::to_string(current.reference_count + 1) + " required_max=4096; diagnostic map stopped, native load unchanged");
            return;
        }
        if (found_context == contexts.end() && contexts.size() >= maximum_contexts) {
            fail_capture("retained building context capacity exhausted actual=" + std::to_string(contexts.size() + 1) + " required_max=64; diagnostic map stopped, native load unchanged");
            return;
        }
        contexts[context].emplace(id, Observation{object, uid});
        ++current.reference_count;
        current.context_count = contexts.size();
        current.peak_reference_count = std::max(current.peak_reference_count, current.reference_count);
        current.peak_context_count = std::max(current.peak_context_count, current.context_count);
    } catch (const std::exception& exc) {
        std::lock_guard lock(mutex);
        fail_capture(std::string("observer exception: ") + exc.what());
    } catch (...) {
        std::lock_guard lock(mutex);
        fail_capture("observer failed with a non-standard exception");
    }
}

bool finalize(Status& result, std::string& error) {
    std::lock_guard lock(mutex);
    negative_classes.clear();
    current.negative_cache_entries = 0;
    failed_rtti_attempts.clear();
    current.failed_rtti_memo_entries = 0;
    current.negative_cache_bytes = 0;
    if (current.state == "failed") { current.load_pending = false; result = current; return fail(error, current.error); }
    if (current.state == "confirmed") { result = current; error.clear(); return true; }
    if (!active) return fail(error, "finalize rejected: no active normal load capture");
    active = false;
    current.load_pending = false;
#ifndef MT2MP_IDENTITY_TEST
    std::vector<Binding> native_arrays;
    if (!root_building_load::finalize(native_arrays, error)) {
        fail_capture("native array load identity incomplete: " + error); result = current; return false;
    }
    dungeon_scene_identity_runtime::Census dungeon_scenes;
    std::vector<dungeon_scene_identity_runtime::Exterior> scene_exteriors;
    for(const auto& row:native_arrays)scene_exteriors.push_back({row.local_uid,row.record_key});
    if(!dungeon_scene_identity_runtime::classify_loaded(current.snapshot_uuid,current.snapshot_sha256,scene_exteriors,dungeon_scenes,error)||
       !npc_quest_target_runtime::stage_scenes(dungeon_scenes,error)||
       !party_saved_identity_runtime::stage_scenes(dungeon_scenes,error)||
       !boss_saved_identity_runtime::stage_scenes(dungeon_scenes,error)||
       !monster_saved_identity_runtime::stage_scenes(std::move(dungeon_scenes),error)){
        fail_capture("Dungeon scene native load classification incomplete: "+error);result=current;return false;
    }
    if (!npc_quest_target_runtime::seal_client(error)) {
        fail_capture("Quest target native load identity incomplete: "+error);result=current;return false;
    }
    if(!district_saved_identity_runtime::seal_client(error)){fail_capture("District native load identity incomplete: "+error);result=current;return false;}
    if(!party_saved_identity_runtime::seal_client(error)){fail_capture("Party native load identity incomplete: "+error);result=current;return false;}
    if(!boss_saved_identity_runtime::seal_client(error)){fail_capture("Boss native load identity incomplete: "+error);result=current;return false;}
    if(!monster_saved_identity_runtime::seal_client(error)){fail_capture("Monster native load identity incomplete: "+error);result=current;return false;}
    if (!npc_saved_identity_runtime::seal_client(error)) { fail_capture("NPC native load identity incomplete: "+error);result=current;return false; }
    if (!bug_authority::seal_snapshot(current.snapshot_uuid,current.snapshot_sha256,error)) { fail_capture("Bug native load identity incomplete: "+error);result=current;return false; }
    current.buildings = std::move(native_arrays);
    current.state = "confirmed"; error.clear(); result = current;
    LOG_INFO("[LoadIdentity] Complete native array building map confirmed snapshot={} buildings={}", current.snapshot_uuid, current.buildings.size());
    return true;
#else
    size_t matching_contexts = 0;
    std::vector<Binding> accepted;
    std::string rejected_cause;
    for (const auto& [context, references] : contexts) {
        std::vector<Binding> bindings;
        size_t rejected = 0;
        for (const auto& [id, observed] : references) {
            if (!observed.uid) continue;
            native::BuildingState building;
            std::string cause;
            bool pointer_matches = true;
#ifndef MT2MP_IDENTITY_TEST
            void* registered_pointer = nullptr;
            const DWORD exception = registered_building_native(observed.uid, registered_pointer);
            pointer_matches = !exception && registered_pointer == observed.pointer;
            if (!pointer_matches) cause = "UID registry pointer changed or lookup exception=" + std::to_string(exception);
#endif
            if (!pointer_matches || !read_building(observed.uid, building, cause)) {
                ++rejected;
                ++current.rejected_buildings;
                if (rejected_cause.empty()) rejected_cause = "id=" + std::to_string(id) + " uid=" + std::to_string(observed.uid) + " " + cause;
                continue;
            }
            bindings.push_back(Binding{id, observed.uid, {}, std::move(building)});
        }
        if (!bindings.empty()) {
            if (rejected) { fail_capture("active building context contains failed readbacks: " + rejected_cause); result = current; return fail(error, current.error); }
            ++matching_contexts;
            accepted = std::move(bindings);
        }
    }
    if (current.rejected_buildings) LOG_WARN("[LoadIdentity] Non-world building observations excluded count={} first_cause={}", current.rejected_buildings, rejected_cause);
    if (matching_contexts != 1) {
        fail_capture("world building context count actual=" + std::to_string(matching_contexts) + " required=1; " + rejected_cause);
        result = current;
        return fail(error, current.error);
    }
    current.buildings = std::move(accepted);
    current.state = "confirmed";
    error.clear();
    result = current;
    LOG_INFO("[LoadIdentity] Typed building map confirmed snapshot={} callbacks={} checks={} nonbuilding_callbacks={} retained_buildings={} retained_contexts={} active_buildings={} samples={} sampled_ns={} sample_max_ns={} stride={}",
             current.snapshot_uuid, current.registrations, current.registration_checks, current.nonbuilding_registrations,
             current.reference_count, current.context_count, current.buildings.size(), current.observer_sample_count,
             current.observer_sample_total_ns, current.observer_sample_max_ns, current.observer_sample_stride);
    return true;
#endif
}

Status status() { std::lock_guard lock(mutex); return current; }
void cancel(const std::string& reason) {
#ifndef MT2MP_IDENTITY_TEST
    root_building_load::cancel(reason);
    npc_quest_target_runtime::cancel(reason);
    npc_saved_identity_runtime::cancel(reason);
    party_saved_identity_runtime::cancel(reason);
    district_saved_identity_runtime::cancel(reason);
    monster_saved_identity_runtime::cancel(reason);
    boss_saved_identity_runtime::cancel(reason);
#endif
    std::lock_guard lock(mutex);
    active = false;
    current.load_pending = false;
    current.state = "cancelled";
    current.error = reason;
    current.buildings.clear();
    contexts.clear();
    negative_classes.clear();
    current.negative_cache_entries = 0;
    failed_rtti_attempts.clear();
    current.failed_rtti_memo_entries = 0;
    current.negative_cache_bytes = 0;
    LOG_INFO("[LoadIdentity] Capture cancelled snapshot={} reason={}", current.snapshot_uuid, reason);
}

void shutdown() {
    cancel("reference observer shutdown");
    std::lock_guard lock(mutex);
    image = 0;
    negative_cache_enabled = false;
    readonly_ranges.clear();
    class_vptr = si_vptr = vmi_vptr = source_typeinfo = target_typeinfo = 0;
}

bool resolve_record(const std::string& sha, int32_t id, uint32_t ordinal, Binding& result, std::string& error) {
    std::lock_guard lock(mutex);
    if (current.state != "confirmed" || sha != current.snapshot_sha256)
        return fail(error, "record resolution rejected: capture is not confirmed for this snapshot SHA");
    const auto found = std::find_if(current.buildings.begin(), current.buildings.end(), [id](const Binding& value) { return value.link_id == id; });
    if (found == current.buildings.end()) return fail(error, "record resolution rejected: linked building missing id=" + std::to_string(id));
    native::BuildingState readback;
    if (!read_building(found->local_uid, readback, error)) return false;
    result = *found;
    result.building = std::move(readback);
    result.record_key = sha + ":" + std::to_string(ordinal);
    error.clear();
    return true;
}

#ifdef MT2MP_IDENTITY_TEST
namespace testing {
void backend(Registration registered, ReadBuilding read) {
    std::lock_guard lock(mutex);
    registration = registered;
    read_building = read;
    image = 1;
    negative_cache_enabled = false;
}
void metadata_image(void* base, size_t bytes, uintptr_t class_pointer, uintptr_t si_pointer,
                    uintptr_t vmi_pointer, uintptr_t source, uintptr_t building) {
    std::lock_guard lock(mutex);
    image = reinterpret_cast<uintptr_t>(base);
    negative_cache_enabled = true;
    readonly_ranges = {{image, bytes, true}};
    class_vptr = class_pointer;
    si_vptr = si_pointer;
    vmi_vptr = vmi_pointer;
    source_typeinfo = source;
    target_typeinfo = building;
    negative_classes.clear();
    current.negative_cache_entries = 0;
    failed_rtti_attempts.clear();
    current.failed_rtti_memo_entries = 0;
    current.negative_cache_bytes = 0;
}
}
#endif
} // namespace mt2mp::game::normal_load_identity

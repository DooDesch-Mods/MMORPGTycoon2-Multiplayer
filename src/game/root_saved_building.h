#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "live_host_state/memory_query_timing.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::root_saved_building {
inline DWORD copy(const void* source, void* target, size_t size) {
    __try { std::memcpy(target, source, size); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
inline bool bytes(const void* object, size_t offset, void* value, size_t size, std::string& error) {
    const auto address = reinterpret_cast<uintptr_t>(object);
    MEMORY_BASIC_INFORMATION memory{};
    if (!object || address > UINTPTR_MAX - offset || !mt2mp::game::live_host_state::apply_timing::virtual_query(reinterpret_cast<void*>(address + offset), &memory, sizeof(memory),mt2mp::game::live_host_state::apply_timing::Part::QuerySavedFields) ||
        memory.State != MEM_COMMIT || (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || size > memory.RegionSize ||
        address + offset - reinterpret_cast<uintptr_t>(memory.BaseAddress) > memory.RegionSize - size) {
        error = "root saved field unreadable offset=" + std::to_string(offset); return false;
    }
    const auto exception = copy(reinterpret_cast<void*>(address + offset), value, size);
    if (exception) { error = "root saved field exception=" + std::to_string(exception); return false; }
    return true;
}
template<class T> bool read(const void* object, size_t offset, T& value, std::string& error) { return bytes(object, offset, &value, sizeof(value), error); }
inline bool type(uintptr_t image, const void* object, uintptr_t expected, std::string& error) {
    uintptr_t actual = 0;
    if (!read(object, 0, actual, error)) return false;
    if (actual != image + expected) { error = "root saved vtable differs expected_rva=" + std::to_string(expected); return false; }
    return true;
}
// Identity uses saved Region.id and the native BuildingArray child position.
// Map.m_ownedRegions is a subset of saved Map.region; unowned regions are also saved.
inline bool region_id(uintptr_t image, void* map, void* region, void* manager, void* shard, int32_t& id, std::string& error) {
    if (!type(image, map, 0x1742860, error)) return false;
    auto* root = static_cast<char*>(map) + 0x1c0;
    int32_t shard_type = -1;
    if (shard != root || !type(image, shard, 0x174ad20, error) || !read(shard, 0x48, shard_type, error) || shard_type != 0) {
        error = "inline building owner is not supported saved root shard"; return false;
    }
    void* region_shard = nullptr; void* owner = nullptr; void* reverse_region = nullptr; void* reverse_shard = nullptr;
    if (!type(image, region, 0x1787930, error) || !type(image, manager, 0x17183c0, error) ||
        !read(region, 0x70, region_shard, error) || !read(region, 0x950, owner, error) ||
        !read(manager, 0x450, reverse_shard, error) || !read(manager, 0x458, reverse_region, error)) return false;
    if (region_shard != shard || owner != manager || reverse_shard != shard || reverse_region != region) {
        error = "inline root region/manager/shard ownership differs"; return false;
    }
    auto* saved = static_cast<char*>(map) + 0x230;
    void* data = nullptr; void* owned = nullptr; int32_t count = 0, capacity = 0, owned_count = 0, owned_capacity = 0;
    if (!type(image, saved, 0x16f04e0, error) || !read(saved, 8, data, error) || !read(saved, 0x10, count, error) ||
        !read(saved, 0x14, capacity, error) || !read(map, 0x358, owned, error) || !read(map, 0x360, owned_count, error) ||
        !read(map, 0x364, owned_capacity, error)) return false;
    if (count < 0 || count > 4096 || capacity < count || owned_count < 0 || owned_count > 4096 || owned_capacity < owned_count) {
        error = "inline root region array bounds invalid required_max=4096"; return false;
    }
    std::set<void*> owned_regions;
    for (int32_t index = 0; index < owned_count; ++index) {
        void* candidate = nullptr; void* candidate_shard = nullptr;
        if (!read(owned, size_t(index) * 8, candidate, error) || !type(image, candidate, 0x1787930, error) ||
            !read(candidate, 0x70, candidate_shard, error)) return false;
        if (candidate_shard != shard || !owned_regions.insert(candidate).second) { error = "inline root owned region shard or uniqueness differs"; return false; }
    }
    std::set<int32_t> ids; std::set<void*> saved_regions; bool found = false;
    for (int32_t index = 0; index < count; ++index) {
        void* candidate = nullptr; void* candidate_shard = nullptr; int32_t candidate_id = -1;
        if (!read(data, size_t(index) * 8, candidate, error) || !type(image, candidate, 0x1787930, error) ||
            !read(candidate, 0x70, candidate_shard, error) || !read(candidate, 0x834, candidate_id, error)) return false;
        const auto context = " saved_index=" + std::to_string(index) + " region_id=" + std::to_string(candidate_id) +
            " saved_count=" + std::to_string(count) + " owned_count=" + std::to_string(owned_count);
        if (candidate_shard != shard) { error = "inline saved root region shard differs required=root" + context; return false; }
        if (candidate_id < 0) { error = "inline saved root region ID invalid required_min=0" + context; return false; }
        if (!saved_regions.insert(candidate).second) { error = "inline saved root region pointer repeated required_unique=1" + context; return false; }
        if (!ids.insert(candidate_id).second) { error = "inline saved root region ID repeated required_unique=1" + context; return false; }
        if (candidate == region) { found = true; id = candidate_id; }
    }
    for (int32_t index = 0; index < owned_count; ++index) {
        void* candidate = nullptr;
        if (!read(owned, size_t(index) * 8, candidate, error)) return false;
        if (!saved_regions.contains(candidate)) {
            error = "inline root owned region absent from saved Map.region required_membership=1 owned_index=" + std::to_string(index) +
                " saved_count=" + std::to_string(count) + " owned_count=" + std::to_string(owned_count); return false;
        }
    }
    if (!found) { error = "inline root region absent from saved Map.region"; return false; }
    return true;
}
inline bool slot(uintptr_t image, void* map, void* object, int32_t child, int32_t& id, std::string& error, const void* expected_array = nullptr) {
    void* shard = nullptr; void* region = nullptr; void* manager = nullptr; void* data = nullptr; void* actual = nullptr;
    int32_t count = 0, capacity = 0;
    if (child < 0 || !read(object, 0x170, shard, error) || !read(object, 0x178, region, error) ||
        !read(region, 0x950, manager, error) || !region_id(image, map, region, manager, shard, id, error)) return false;
    auto* array = static_cast<char*>(manager) + 0x408;
    if (expected_array && array != expected_array) { error = "inline Building array differs from saved owner"; return false; }
    if (!type(image, array, 0x172d7c0, error) || !read(array, 8, data, error) || !read(array, 0x10, count, error) ||
        !read(array, 0x14, capacity, error)) return false;
    if (count < 0 || count > 4096 || capacity < count || child >= count) { error = "inline building child outside saved array"; return false; }
    if (!read(data, size_t(child) * 8, actual, error)) return false;
    if (actual != object) { error = "inline building saved child pointer changed"; return false; }
    return true;
}
}

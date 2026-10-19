#pragma once
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mt2mp::game::dungeon_party_census {
inline constexpr uintptr_t region_ap = 0x1787930;
inline constexpr uintptr_t manager_ap = 0x17018d0;
inline constexpr uintptr_t parties_ap = 0x16f0180;
inline constexpr uintptr_t pending_ap = 0x1704b60;
inline constexpr uintptr_t party_ap = 0x174a370;
struct Array {
    uintptr_t object = 0, data = 0;
    int32_t count = 0, capacity = 0;
    std::vector<uintptr_t> members;
    bool operator==(const Array&) const = default;
};
struct Member {
    uintptr_t pointer = 0;
    uint64_t uid = 0;
    uintptr_t shard = 0, region = 0, manager_region = 0;
    bool operator==(const Member&) const = default;
};
struct Pool {
    uintptr_t region = 0, shard = 0, manager = 0;
    Array parties, removals, releases;
    uint8_t updating = 0;
    std::vector<Member> members;
    bool operator==(const Pool&) const = default;
};
inline bool fail(std::string& error, const char* cause) {
    error = cause;
    return false;
}
template<class Memory, class T>
bool at(Memory& memory, uintptr_t object, size_t offset, T& out, std::string& error) {
    if (!object || offset > UINTPTR_MAX - object || sizeof(T) > UINTPTR_MAX - object - offset)
        return fail(error, "Party census native field address overflows");
    return memory.read(object + offset, &out, sizeof out, error);
}
template<class Memory>
bool array(Memory& memory, uintptr_t image, uintptr_t object, uintptr_t expected_ap,
           Array& out, std::string& error) {
    out = {};
    uintptr_t ap = 0;
    Array next;
    next.object = object;
    if (!at(memory, object, 0, ap, error) || !at(memory, object, 8, next.data, error) ||
        !at(memory, object, 0x10, next.count, error) || !at(memory, object, 0x14, next.capacity, error))
        return false;
    if (image > UINTPTR_MAX - expected_ap || ap != image + expected_ap ||
        next.count < 0 || next.capacity < next.count || (next.capacity && !next.data) ||
        (next.count && (!next.data || size_t(next.count) > (UINTPTR_MAX - next.data) / sizeof(uintptr_t))))
        return fail(error, "Party census native array class/count/capacity/span differs");
    std::set<uintptr_t> seen;
    for (int32_t i = 0; i < next.count; ++i) {
        uintptr_t pointer = 0;
        if (!at(memory, next.data, size_t(i) * sizeof(uintptr_t), pointer, error)) return false;
        if (!pointer || !seen.insert(pointer).second)
            return fail(error, "Party census native array member is null or duplicated");
        next.members.push_back(pointer);
    }
    out = std::move(next);
    return true;
}
template<class Memory, class Find>
bool read(Memory& memory, Find& find, uintptr_t image, uintptr_t shard, uintptr_t region,
          Pool& out, std::string& error) {
    out = {};
    error.clear();
    if (!image || image > UINTPTR_MAX - region_ap || !shard || !region)
        return fail(error, "Party census requires exact current image/Shard/Region");
    Pool next;
    next.region = region;
    next.shard = shard;
    uintptr_t ap = 0, actual = 0;
    if (!at(memory, region, 0, ap, error) || !at(memory, region, 0x70, actual, error)) return false;
    if (ap != image + region_ap || actual != shard)
        return fail(error, "Party census Region class/Shard differs");
    if (!at(memory, region, 0x9d0, next.manager, error)) return false;
    if (!next.manager) return fail(error, "Party census Region has no current manager");
    if (!at(memory, next.manager, 0, ap, error) ||
        !at(memory, next.manager, 0xa8, actual, error) ||
        !at(memory, next.manager, 0xb0, next.updating, error)) return false;
    if (ap != image + manager_ap || actual != region || next.updating > 1)
        return fail(error, "Party census manager class/reciprocal Region/update flag differs");
    if (next.manager > UINTPTR_MAX - 0x80)
        return fail(error, "Party census manager array address overflows");
    if (!array(memory, image, next.manager + 0x18, parties_ap, next.parties, error) ||
        !array(memory, image, next.manager + 0x58, pending_ap, next.removals, error) ||
        !array(memory, image, next.manager + 0x80, pending_ap, next.releases, error)) return false;
    std::set<uint64_t> uids;
    for (uintptr_t pointer : next.parties.members) {
        Member member{pointer};
        uintptr_t registered = 0;
        if (!at(memory, pointer, 0, ap, error) || !at(memory, pointer, 0x18, member.uid, error) ||
            !at(memory, pointer, 0x170, member.shard, error) ||
            !at(memory, pointer, 0x178, member.region, error) ||
            !at(memory, pointer, 0x620, member.manager_region, error)) return false;
        if (ap != image + party_ap || !member.uid || !uids.insert(member.uid).second)
            return fail(error, "Party census actual class/full UID differs or UID is duplicated");
        if (!find(member.uid, registered, error)) return false;
        if (registered != pointer) return fail(error, "Party census full UID registry differs from actual member");
        next.members.push_back(member);
    }
    // Region transfer can leave one Party in the old manager until ReleaseParty
    // drains. Preserve its current owner fields rather than inventing membership.
    for (const Array* previous : {&next.parties, &next.removals, &next.releases}) {
        Array current;
        const uintptr_t expected = previous == &next.parties ? parties_ap : pending_ap;
        if (!array(memory, image, previous->object, expected, current, error)) return false;
        if (current != *previous) return fail(error, "Party census native array changed during read");
    }
    for (const Member& previous : next.members) {
        Member current{previous.pointer};
        if (!at(memory, current.pointer, 0, ap, error) ||
            !at(memory, current.pointer, 0x18, current.uid, error) ||
            !at(memory, current.pointer, 0x170, current.shard, error) ||
            !at(memory, current.pointer, 0x178, current.region, error) ||
            !at(memory, current.pointer, 0x620, current.manager_region, error)) return false;
        if (ap != image + party_ap || current != previous)
            return fail(error, "Party census member changed during read");
    }
    uintptr_t current_manager = 0, owner = 0;
    uint8_t updating = 0;
    if (!at(memory, region, 0x9d0, current_manager, error) || !at(memory, next.manager, 0xa8, owner, error) ||
        !at(memory, next.manager, 0xb0, updating, error)) return false;
    if (current_manager != next.manager || owner != region || updating != next.updating)
        return fail(error, "Party census reciprocal manager changed during read");
    out = std::move(next);
    error.clear();
    return true;
}
inline bool settled(const Pool& pool) {
    return !pool.updating && pool.removals.members.empty() && pool.releases.members.empty();
}
}

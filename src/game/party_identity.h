#pragma once
#include "snapshot_party_key.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::party_identity {
inline bool uuid(std::string_view text) {
    if (text.size() != 36) return false;
    for (size_t i = 0; i < text.size(); ++i) {
        const auto c = text[i];
        if ((i == 8 || i == 13 || i == 18 || i == 23) ? c != '-' :
            !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
inline bool valid_key(std::string_view text) {
    if (snapshot_party_key::valid(text)) return true;
    if (!text.starts_with("action:")) return false;
    text.remove_prefix(7);
    return text.size() > 43 && uuid(text.substr(0, 36)) && text.substr(36, 7) == ":party:" &&
        snapshot_building_key::number(text.substr(43), INT32_MAX, false);
}
inline std::string birth(const std::string& operation, uint32_t ordinal) {
    return "action:" + operation + ":party:" + std::to_string(ordinal);
}
struct Scope {
    std::string snapshot;
    uint64_t epoch = 0, generation = 0;
    uintptr_t root = 0;
    bool operator==(const Scope&) const = default;
};
struct Entry {
    Scope scope;
    uintptr_t pointer = 0;
    uint64_t uid = 0;
    bool retired = false;
    bool operator==(const Entry&) const = default;
};
inline bool valid(const Scope& scope) {
    return uuid(scope.snapshot) && scope.epoch && scope.generation && scope.root;
}
inline bool fail(std::string& error, const char* cause) { error = cause; return false; }

// A Party moves between managers. Its identity belongs to the completed world,
// while fresh native census rows carry its current membership and Region links.
class Registry {
    std::map<std::string, Entry> entries_;
public:
    bool restore(const Scope& current, const std::vector<std::pair<std::string, Entry>>& rows, std::string& error) {
        if (!valid(current)) return fail(error, "Party restore requires an actual completed world");
        std::map<std::string, Entry> next;
        std::set<uintptr_t> pointers;
        std::set<uint64_t> uids;
        for (const auto& [key, row] : rows) {
            if (!valid_key(key) || row.scope != current || !row.pointer || !row.uid || row.retired ||
                !pointers.insert(row.pointer).second || !uids.insert(row.uid).second || !next.emplace(key, row).second)
                return fail(error, "Party restore has a duplicate or invalid actual full-UID binding");
        }
        for (const auto& [key, row] : entries_) if (row.scope == current) {
            const auto found = next.find(key);
            if (row.retired) {
                if (found != next.end()) return fail(error, "Party restore would revive an existing lifetime");
                next.emplace(key, row);
            } else if (found == next.end() || found->second != row) {
                return fail(error, "Party restore changes a confirmed current native binding");
            }
        }
        entries_.swap(next);
        error.clear();
        return true;
    }
    bool append(const std::string& key, const Entry& row, std::string& error) {
        if (!valid_key(key) || !key.starts_with("action:") || !valid(row.scope) ||
            !row.pointer || !row.uid || row.retired || entries_.contains(key))
            return fail(error, "Party original birth key or actual current world differs");
        for (const auto& [old_key, old] : entries_) if (old.scope == row.scope &&
            (old.uid == row.uid || (!old.retired && old.pointer == row.pointer)))
            return fail(error, "Party original birth aliases an existing full-UID lifetime");
        entries_.emplace(key, row);
        error.clear();
        return true;
    }
    bool identify(const Scope& current, uintptr_t object, uint64_t uid, std::string& key, std::string& error) const {
        key.clear();
        if (!valid(current) || !object || !uid) return fail(error, "Party identify requires actual current world/object/full UID");
        for (const auto& [candidate, row] : entries_) if (row.scope == current && row.pointer == object && row.uid == uid) {
            if (row.retired) return fail(error, "Party original lifetime is retired");
            key = candidate;
            error.clear();
            return true;
        }
        return fail(error, "Party lacks an actual saved or original-birth canonical identity");
    }
    bool lookup(const Scope& current, const std::string& key, Entry& out, std::string& error) const {
        out = {};
        const auto found = entries_.find(key);
        if (!valid(current) || found == entries_.end() || found->second.scope != current || found->second.retired)
            return fail(error, "Party key is absent, retired or outside the completed world");
        out = found->second;
        error.clear();
        return true;
    }
    bool retire(const std::string& key, const Entry& expected, std::string& error) {
        const auto found = entries_.find(key);
        if (found == entries_.end() || found->second != expected || expected.retired)
            return fail(error, "Party retirement lacks its exact current original binding");
        found->second.retired = true;
        error.clear();
        return true;
    }
    bool enumerate(const Scope& current, std::vector<std::pair<std::string, Entry>>& out, std::string& error) const {
        out.clear();
        if (!valid(current)) return fail(error, "Party enumeration requires actual current world");
        for (const auto& row : entries_) if (row.second.scope == current) out.push_back(row);
        error.clear();
        return true;
    }
    bool capture_key(const Scope& current, uintptr_t object, uint64_t uid, const std::string& saved,
                     std::string& out, std::string& error) const {
        out.clear();
        const bool initial = uuid(current.snapshot) && current.root && !current.epoch && !current.generation;
        if ((!valid(current) && !initial) || !object || !uid || !snapshot_party_key::valid(saved))
            return fail(error, "Party saved capture lacks its actual current world/record");
        for (const auto& [key, row] : entries_) if (row.scope == current && (row.pointer == object || row.uid == uid)) {
            if (row.pointer != object || row.uid != uid || row.retired)
                return fail(error, "Party saved capture changed a current native lifetime");
            out = key;
            error.clear();
            return true;
        }
        out = saved;
        error.clear();
        return true;
    }
};
inline Registry registry;
}

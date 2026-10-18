#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace mt2mp::net::presence {
using Values = std::array<std::string, 4>;
inline constexpr std::array<const char*, 4> keys{"status", "steam_player_group", "steam_player_group_size", "subscribercount"};

inline bool copy_value(const char* actual, const char* key, std::string& value, std::string& error) {
    if (!actual) { error = "Steam presence getter returned null key=" + std::string(key); return false; }
    // Steam owns a terminated string, not a readable 256-byte range. Stop each byte read at its terminator.
    const volatile char* cursor = actual;
    size_t length = 0;
    while (length < 256 && cursor[length] != '\0') ++length;
    if (length == 256) { error = "Steam presence value exceeds SDK limit key=" + std::string(key); return false; }
    value.assign(actual, length);
    error.clear(); return true;
}

inline std::string decorate_count(const std::string& prefix, const std::string& count) {
    if (prefix.empty() || count.empty() || count.size() > 64 || count[0] < '0' || count[0] > '9') return {};
    for (unsigned char ch : count) if (ch < 32 || ch == 127 || ch == '|') return {};
    return prefix + count;
}

inline Values generic() {
    return {"Multiplayer-Mod", {}, {}, "Multiplayer-Mod | "};
}

inline Values describe(uint64_t lobby, unsigned members, unsigned limit, bool playing, bool preparing) {
    if (!lobby || !members || members > limit || limit > 4) return generic();
    const auto phase = playing ? "Building together" : preparing ? "Loading co-op" : "Co-op lobby";
    return {std::string("Multiplayer-Mod | ") + phase + " - " + std::to_string(members) + "/" +
        std::to_string(limit) + " players", "mt2mp:" + std::to_string(lobby), std::to_string(members),
        "Multiplayer-Mod | Co-op " + std::to_string(members) + "/" + std::to_string(limit) + " | "};
}

// Only restore values still owned by this publisher, preserving later writes from another mod or the game.
class OwnedKeys {
public:
    template<class Api> bool apply(const Values& wanted, Api& api, std::string& error) {
        if (wanted[0].empty()) return restore(api, error);
        for (size_t i = 0; i < keys.size(); ++i) {
            std::string actual;
            if (!api.get(keys[i], actual, error)) return false;
            // The original game refreshes its subscriber count. Keep that newest value for restoration.
            if (owned_[i] && actual != last_[i]) { original_[i] = actual; owned_[i] = false; }
            if (!owned_[i]) original_[i] = actual;
            const auto next = i == 3 ? decorate_count(wanted[i], original_[i]) : wanted[i];
            if (next.empty()) { if (!restore_one(i, api, error)) return false; continue; }
            if (actual == next) continue;
            if (!api.set(keys[i], next, error)) return false;
            owned_[i] = true;
            last_[i] = next;
            if (!api.get(keys[i], actual, error)) return false;
            if (actual != next) { error = "Steam presence readback differs key=" + std::string(keys[i]); return false; }
        }
        error.clear(); return true;
    }

    template<class Api> bool restore(Api& api, std::string& error) {
        std::string failures;
        for (size_t i = 0; i < keys.size(); ++i) {
            if (!owned_[i]) continue;
            std::string detail;
            if (!restore_one(i, api, detail)) { if (!failures.empty()) failures += "; "; failures += detail; }
        }
        error = failures; return failures.empty();
    }
    bool active() const { for (bool value : owned_) if (value) return true; return false; }
private:
    template<class Api> bool restore_one(size_t i, Api& api, std::string& error) {
        if (!owned_[i]) return true;
        std::string actual;
        if (!api.get(keys[i], actual, error)) return false;
        if (actual == last_[i]) {
            if (!api.set(keys[i], original_[i], error) || !api.get(keys[i], actual, error)) return false;
            if (actual != original_[i]) { error = "Steam presence restore readback differs key=" + std::string(keys[i]); return false; }
        }
        owned_[i] = false; return true;
    }
    Values original_{}, last_{};
    std::array<bool, 4> owned_{};
};
}

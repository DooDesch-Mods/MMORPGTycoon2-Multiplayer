#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace mt2mp::game {

struct PlayerInfo {
    uint64_t steam_id = 0;
    std::string display_name;
    uint32_t color = 0;             // ARGB color for cursor
    std::string current_shard;
    float cursor_world_x = 0;
    float cursor_world_y = 0;
    float cursor_world_z = 0;
    uint8_t cursor_type = 0;
    bool connected = false;

    // Generate a deterministic color from Steam ID
    static uint32_t color_from_id(uint64_t id);
};

class PlayerRegistry {
public:
    static PlayerRegistry& instance();

    void add(const PlayerInfo& info);
    void remove(uint64_t steam_id);
    PlayerInfo* get(uint64_t steam_id);
    const std::unordered_map<uint64_t, PlayerInfo>& all() const;

    void update_cursor(uint64_t steam_id, float x, float y, float z,
                       uint8_t type, const std::string& shard);

private:
    PlayerRegistry() = default;
    std::unordered_map<uint64_t, PlayerInfo> players_;
};

} // namespace mt2mp::game

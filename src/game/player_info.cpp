#include "mt2mp/game/player_info.h"

namespace mt2mp::game {

uint32_t PlayerInfo::color_from_id(uint64_t id) {
    // Generate a bright, distinguishable color from Steam ID
    uint32_t hash = static_cast<uint32_t>(id ^ (id >> 32));
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = (hash >> 16) ^ hash;

    // Ensure brightness by keeping each channel above 100
    uint8_t r = 100 + (hash & 0xFF) % 156;
    uint8_t g = 100 + ((hash >> 8) & 0xFF) % 156;
    uint8_t b = 100 + ((hash >> 16) & 0xFF) % 156;

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

PlayerRegistry& PlayerRegistry::instance() {
    static PlayerRegistry reg;
    return reg;
}

void PlayerRegistry::add(const PlayerInfo& info) {
    players_[info.steam_id] = info;
    players_[info.steam_id].color = PlayerInfo::color_from_id(info.steam_id);
}

void PlayerRegistry::remove(uint64_t steam_id) {
    players_.erase(steam_id);
}

PlayerInfo* PlayerRegistry::get(uint64_t steam_id) {
    auto it = players_.find(steam_id);
    return it != players_.end() ? &it->second : nullptr;
}

const std::unordered_map<uint64_t, PlayerInfo>& PlayerRegistry::all() const {
    return players_;
}

void PlayerRegistry::update_cursor(uint64_t steam_id, float x, float y, float z,
                                    uint8_t type, const std::string& shard) {
    auto it = players_.find(steam_id);
    if (it != players_.end()) {
        it->second.cursor_world_x = x;
        it->second.cursor_world_y = y;
        it->second.cursor_world_z = z;
        it->second.cursor_type = type;
        it->second.current_shard = shard;
    }
}

} // namespace mt2mp::game

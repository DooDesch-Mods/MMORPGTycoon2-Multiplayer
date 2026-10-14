#include "mt2mp/sync/sync_cursor.h"
#include "mt2mp/net/net_manager.h"
#include "mt2mp/net/packet_serializer.h"
#include "mt2mp/hooks/hook_input.h"
#include "mt2mp/hooks/hook_shard.h"
#include "mt2mp/game/player_info.h"
#include "mt2mp/core/config.h"

namespace mt2mp::sync {

CursorSync& CursorSync::instance() {
    static CursorSync cs;
    return cs;
}

void CursorSync::send_update() {
    auto& mouse = hooks::get_mouse_state();
    auto local_id = net::SteamNetworking::instance().local_steam_id();

    // TODO: Convert screen coords to world coords using camera matrices
    // For now send screen coords as placeholders
    float wx = static_cast<float>(mouse.screen_x);
    float wy = 0.0f;
    float wz = static_cast<float>(mouse.screen_y);

    auto packet = net::PacketSerializer::make_cursor_update(
        local_id, 0, wx, wy, wz, 0);

    net::NetManager::instance().broadcast_unreliable(packet);
}

void CursorSync::on_remote_cursor(uint64_t steam_id, uint32_t shard_id,
                                   float x, float y, float z, uint8_t type) {
    game::PlayerRegistry::instance().update_cursor(
        steam_id, x, y, z, type, hooks::get_current_shard());
}

void CursorSync::interpolate(float dt) {
    // TODO: Smooth interpolation of remote cursor positions
    // For now, positions are updated directly in on_remote_cursor
}

} // namespace mt2mp::sync

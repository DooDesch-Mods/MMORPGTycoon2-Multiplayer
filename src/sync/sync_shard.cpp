#include "mt2mp/sync/sync_shard.h"
#include "mt2mp/game/player_info.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::sync {

ShardSync& ShardSync::instance() {
    static ShardSync ss;
    return ss;
}

void ShardSync::on_local_shard_change(const char* shard_name) {
    LOG_INFO("Local shard change: {}", shard_name ? shard_name : "null");
    // TODO: Broadcast shard change to peers
}

void ShardSync::on_remote_shard_change(uint64_t steam_id, const char* shard_name) {
    auto* player = game::PlayerRegistry::instance().get(steam_id);
    if (player) {
        player->current_shard = shard_name ? shard_name : "";
    }
}

} // namespace mt2mp::sync

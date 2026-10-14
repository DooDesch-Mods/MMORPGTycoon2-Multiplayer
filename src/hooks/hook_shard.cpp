#include "mt2mp/hooks/hook_shard.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"

#include <string>

namespace mt2mp::hooks {

static void* orig_set_shard = nullptr;
static std::string g_current_shard;
static ShardChangeCallback g_on_shard_change;

// ShardManager::SetShard likely: void __fastcall SetShard(void* this, const char* name)
using SetShardFn = void(__fastcall*)(void* this_ptr, const char* name);
static void __fastcall detour_set_shard(void* this_ptr, const char* name) {
    g_current_shard = name ? name : "";
    LOG_INFO("ShardManager::SetShard -> \"{}\"", g_current_shard);

    if (g_on_shard_change) g_on_shard_change(name);

    reinterpret_cast<SetShardFn>(orig_set_shard)(this_ptr, name);
}

void set_shard_change_callback(ShardChangeCallback cb) { g_on_shard_change = std::move(cb); }
const char* get_current_shard() { return g_current_shard.c_str(); }

bool install_shard_hooks() {
    return create_hook_by_string(
        "= SHARD:",
        reinterpret_cast<void*>(&detour_set_shard),
        &orig_set_shard,
        "ShardManager::SetShard"
    );
}

} // namespace mt2mp::hooks

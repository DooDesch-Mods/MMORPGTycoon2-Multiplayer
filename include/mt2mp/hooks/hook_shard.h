#pragma once

#include <functional>
#include <string>

namespace mt2mp::hooks {

using ShardChangeCallback = std::function<void(const char* shard_name)>;
void set_shard_change_callback(ShardChangeCallback cb);
const char* get_current_shard();

bool install_shard_hooks();

} // namespace mt2mp::hooks

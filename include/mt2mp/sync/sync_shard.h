#pragma once

#include <cstdint>
#include <string>

namespace mt2mp::sync {

class ShardSync {
public:
    static ShardSync& instance();

    void on_local_shard_change(const char* shard_name);
    void on_remote_shard_change(uint64_t steam_id, const char* shard_name);

private:
    ShardSync() = default;
};

} // namespace mt2mp::sync

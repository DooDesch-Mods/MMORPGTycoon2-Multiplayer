#pragma once

#include <cstdint>

namespace mt2mp::sync {

class CursorSync {
public:
    static CursorSync& instance();

    // Send local cursor position
    void send_update();

    // Receive remote cursor update
    void on_remote_cursor(uint64_t steam_id, uint32_t shard_id,
                          float x, float y, float z, uint8_t type);

    // Interpolate remote cursors (call each frame)
    void interpolate(float dt);

private:
    CursorSync() = default;
};

} // namespace mt2mp::sync

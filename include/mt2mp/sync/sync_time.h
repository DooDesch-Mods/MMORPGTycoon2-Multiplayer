#pragma once

#include <cstdint>

namespace mt2mp::sync {

class TimeSync {
public:
    static TimeSync& instance();

    void send_time_state();
    void on_remote_time_sync(uint32_t day, float fraction, uint8_t speed);
    void on_remote_pause();
    void on_remote_unpause();
    void on_remote_speed_change(uint8_t speed);

private:
    TimeSync() = default;
};

} // namespace mt2mp::sync

#pragma once

#include <functional>

namespace mt2mp::hooks {

enum class TimeEvent { Pause, Unpause, SpeedChange };
using TimeCallback = std::function<void(TimeEvent event)>;
void set_time_callback(TimeCallback cb);

bool install_clock_hooks();

} // namespace mt2mp::hooks

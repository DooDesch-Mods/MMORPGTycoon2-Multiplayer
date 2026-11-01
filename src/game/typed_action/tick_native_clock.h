#pragma once
#include "tick_execution.h"
#include "../time_runtime.h"
namespace mt2mp::action_session::tick_commands {
// Obtain Native with the existing time_runtime::production_backend initializer.
// Its original clock APIs are shared; Client presentation adoption is not used.
ClockPort ordered_native_clock(game::time_runtime::Native);
}

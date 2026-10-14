#include "mt2mp/hooks/hook_clock.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::hooks {

static void* orig_clock = nullptr;
static TimeCallback g_on_time_event;

static uint64_t __fastcall detour_clock(void* rcx, void* rdx, void* r8, void* r9) {
    LOG_INFO("Clock function called");
    if (g_on_time_event) g_on_time_event(TimeEvent::SpeedChange);
    return reinterpret_cast<uint64_t(__fastcall*)(void*, void*, void*, void*)>(orig_clock)(rcx, rdx, r8, r9);
}

void set_time_callback(TimeCallback cb) { g_on_time_event = std::move(cb); }

bool install_clock_hooks() {
    return create_hook_by_string(
        ">> PAUSE",
        reinterpret_cast<void*>(&detour_clock),
        &orig_clock,
        "Clock::Pause"
    );
}

} // namespace mt2mp::hooks

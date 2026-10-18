#pragma once

#include <functional>
#include <cstdint>

namespace mt2mp::hooks {

using RenderCallback = std::function<void()>;
struct RenderPresentSnapshot {
    uint64_t completed = 0;
    uint64_t sampled_monotonic_ns = 0;
    uint64_t last_completed_monotonic_ns = 0;
    uint64_t window_identity = 0;
    uint64_t overlay_calls = 0;
    uint64_t overlay_total_ns = 0;
    uint64_t swap_returns = 0;
    uint64_t swap_total_ns = 0;
    uint64_t swap_failed = 0;
};

void set_render_callback(RenderCallback cb);
RenderPresentSnapshot render_present_snapshot();

bool install_renderer_hooks();

} // namespace mt2mp::hooks

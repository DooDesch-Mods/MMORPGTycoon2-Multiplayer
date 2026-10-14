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
};

void set_render_callback(RenderCallback cb);
RenderPresentSnapshot render_present_snapshot();

bool install_renderer_hooks();

} // namespace mt2mp::hooks

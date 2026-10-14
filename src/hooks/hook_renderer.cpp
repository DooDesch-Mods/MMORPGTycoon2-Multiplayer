#include "mt2mp/hooks/hook_renderer.h"
#include "mt2mp/hooks/hook_window.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"

#include <SDL3/SDL.h>
#include <windows.h>
#include <chrono>
#include <mutex>

using SDL_GL_SwapWindow_t = bool(*)(SDL_Window* window);

namespace mt2mp::hooks {

static SDL_GL_SwapWindow_t orig_swap_window = nullptr;
static RenderCallback g_render_callback;
static bool g_window_captured = false;
static std::mutex g_present_mutex;
static RenderPresentSnapshot g_presents;
static SDL_Window* g_present_window = nullptr;

static uint64_t monotonic_ns() {
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

static bool detour_swap_window(SDL_Window* window) {
    // Capture the game window pointer from SwapWindow (always called with the right window)
    if (!g_window_captured && window) {
        // Set it in the window hook module so overlay can find it
        extern void set_game_window_ptr(SDL_Window* w);
        set_game_window_ptr(window);
        g_window_captured = true;
        LOG_INFO("Game window captured from SwapWindow: 0x{:X}",
            reinterpret_cast<uintptr_t>(window));
    }

    // Call our overlay render before the swap
    if (g_render_callback) {
        g_render_callback();
    }

    const bool returned = orig_swap_window(window);
    if (returned) {
        std::lock_guard lock(g_present_mutex);
        ++g_presents.completed;
        g_presents.last_completed_monotonic_ns = monotonic_ns();
        if (g_present_window != window) {
            g_present_window = window;
            ++g_presents.window_identity;
        }
    }
    return returned;
}

RenderPresentSnapshot render_present_snapshot() {
    std::lock_guard lock(g_present_mutex);
    auto result = g_presents;
    result.sampled_monotonic_ns = monotonic_ns();
    return result;
}

void set_render_callback(RenderCallback cb) {
    g_render_callback = std::move(cb);
}

bool install_renderer_hooks() {
    return create_hook_by_import(
        "SDL3.dll",
        "SDL_GL_SwapWindow",
        reinterpret_cast<void*>(&detour_swap_window),
        reinterpret_cast<void**>(&orig_swap_window)
    );
}

} // namespace mt2mp::hooks

#include "mt2mp/hooks/hook_shell.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/hooks/hook_window.h"
#include "mt2mp/overlay/lobby_overlay.h"
#include "mt2mp/core/logger.h"

#include <windows.h>
#include <SDL3/SDL.h>
#include <cstring>

namespace mt2mp::hooks {

using ShellExecuteAFn = HINSTANCE(WINAPI*)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);
static ShellExecuteAFn orig_shell = nullptr;

static HINSTANCE WINAPI detour_shell(HWND hwnd, LPCSTR op, LPCSTR file,
                                      LPCSTR params, LPCSTR dir, INT show) {
    // Log EVERY ShellExecuteA call for debugging
    LOG_INFO("ShellExecuteA: op=\"{}\" file=\"{}\" params=\"{}\"",
        op ? op : "(null)", file ? file : "(null)", params ? params : "(null)");

    if (file) {
        // Intercept any URL that looks like the Discord link
        if (strstr(file, "discord") || strstr(file, "mmorpgtycoon") ||
            strstr(file, "http") || strstr(file, "mt2mp")) {
            LOG_INFO("Hijacked URL -> opening Multiplayer lobby");
            overlay::LobbyOverlay::instance().set_visible(true);

            // Set overlay active flag (same as F10 does in hook_input.cpp)
            extern bool g_overlay_active_flag;
            g_overlay_active_flag = true;

            SDL_ShowCursor();
            SDL_StartTextInput(hooks::get_game_window());
            return reinterpret_cast<HINSTANCE>(42);
        }
    }

    return orig_shell(hwnd, op, file, params, dir, show);
}

bool install_shell_hooks() {
    return create_hook_by_import(
        "SHELL32.dll", "ShellExecuteA",
        reinterpret_cast<void*>(&detour_shell),
        reinterpret_cast<void**>(&orig_shell));
}

} // namespace mt2mp::hooks

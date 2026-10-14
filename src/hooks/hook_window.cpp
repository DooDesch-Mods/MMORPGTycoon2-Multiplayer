#include "mt2mp/hooks/hook_window.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"

#include <cstring>
#include <cstdlib>
#include <SDL3/SDL.h>

struct SDL_Window; // opaque

// SDL function signatures
using CreateWindowFn = SDL_Window*(*)(const char*, int, int, SDL_WindowFlags);
using SetWindowTitleFn = bool(*)(SDL_Window*, const char*);
using SetWindowFullscreenFn = bool(*)(SDL_Window*, bool);
using SetWindowSizeFn = bool(*)(SDL_Window*, int, int);

namespace mt2mp::hooks {

static CreateWindowFn orig_createWindow = nullptr;
static SetWindowTitleFn orig_setWindowTitle = nullptr;
static SetWindowFullscreenFn orig_setWindowFullscreen = nullptr;
static SetWindowSizeFn orig_setWindowSize = nullptr;

static SDL_Window* g_game_window = nullptr;
static GameTimeInfo g_time_info;
static TitleChangeCallback g_on_title;
static WindowCreatedCallback g_on_window;

// Parse game time from window title
// Typical format: "MMORPG Tycoon 2 - GameName - Day 42 [>>]" or similar
static void parse_title(const char* title, GameTimeInfo& info) {
    info.raw_title = title ? title : "";
    info.game_day = -1;
    info.speed = -1;

    if (!title) return;

    // Look for "Day NNN" pattern
    const char* day_str = strstr(title, "Day ");
    if (!day_str) day_str = strstr(title, "day ");
    if (day_str) {
        info.game_day = atoi(day_str + 4);
    }

    // Look for speed indicators
    if (strstr(title, "||") || strstr(title, "PAUSE") || strstr(title, "pause")) {
        info.speed = 0;
    } else if (strstr(title, ">>>") || strstr(title, "FASTEST") || strstr(title, "3x")) {
        info.speed = 3;
    } else if (strstr(title, ">>") || strstr(title, "FAST") || strstr(title, "2x")) {
        info.speed = 2;
    } else if (strstr(title, ">") || info.game_day >= 0) {
        info.speed = 1; // normal speed if we see day info
    }
}

// --- Detours ---

static SDL_Window* detour_createWindow(const char* title, int w, int h, SDL_WindowFlags flags) {
    std::string local_title=title?title:"";
    const auto& cfg=Config::instance();
    if(cfg.local_test) {
        local_title="MMORPG Tycoon 2 ["+(cfg.is_host?std::string("Host"):"Client "+std::to_string(cfg.peer_id-1))+"]";
        flags&=~SDL_WINDOW_FULLSCREEN; w=1280; h=720;
    }
    SDL_Window* window = orig_createWindow(local_title.c_str(), w, h, flags);

    if (window) {
        g_game_window = window;
        LOG_INFO("SDL_CreateWindow \"{}\" {}x{} -> 0x{:X}",
            title ? title : "(null)", w, h, reinterpret_cast<uintptr_t>(window));

        if (g_on_window) g_on_window(window);
    }

    return window;
}

static bool detour_setWindowTitle(SDL_Window* window, const char* title) {
    const auto& cfg=Config::instance();
    std::string decorated=title?title:"";
    if(cfg.local_test) decorated+=" ["+(cfg.is_host?std::string("Host"):"Client "+std::to_string(cfg.peer_id-1))+"]";
    const bool result = orig_setWindowTitle(window, cfg.local_test?decorated.c_str():title);

    if (window == g_game_window && title) {
        GameTimeInfo old_info = g_time_info;
        parse_title(title, g_time_info);

        // Only log when something meaningful changes
        if (g_time_info.game_day != old_info.game_day || g_time_info.speed != old_info.speed) {
            LOG_INFO("Title: \"{}\" -> day={}, speed={}", title, g_time_info.game_day, g_time_info.speed);
        }

        if (g_on_title) g_on_title(g_time_info);
    }
    return result;
}
static bool detour_setWindowFullscreen(SDL_Window* window,bool fullscreen) {
    const bool local=Config::instance().local_test;
    if(local&&fullscreen) LOG_INFO("Local test fullscreen request redirected to windowed mode");
    const bool result=orig_setWindowFullscreen(window,local?false:fullscreen);
    if(!result) LOG_ERROR("SDL_SetWindowFullscreen failed: {}",SDL_GetError());
    return result;
}
static bool detour_setWindowSize(SDL_Window* window,int width,int height) {
    if(Config::instance().local_test) { width=1280; height=720; }
    const bool result=orig_setWindowSize(window,width,height);
    if(!result) LOG_ERROR("SDL_SetWindowSize failed dimensions={}x{}: {}",width,height,SDL_GetError());
    return result;
}

// Called from hook_renderer.cpp to set window ptr captured from SwapWindow
void set_game_window_ptr(SDL_Window* w) {
    if (!g_game_window && w) {
        g_game_window = w;
        LOG_INFO("Game window set: 0x{:X}", reinterpret_cast<uintptr_t>(w));
        const auto& cfg=Config::instance();
        if(cfg.local_test) {
            const std::string title="MMORPG Tycoon 2 ["+(cfg.is_host?std::string("Host"):"Client "+std::to_string(cfg.peer_id-1))+"]";
            LOG_INFO("Local test existing window setup attempted");
            if(!orig_setWindowFullscreen(w,false)) LOG_ERROR("Local test existing window fullscreen setup failed: {}",SDL_GetError());
            if(!orig_setWindowSize(w,1280,720)) LOG_ERROR("Local test existing window size setup failed: {}",SDL_GetError());
            if(!orig_setWindowTitle(w,title.c_str())) LOG_ERROR("Local test existing window title setup failed: {}",SDL_GetError());
            int width=0,height=0;
            if(!SDL_GetWindowSizeInPixels(w,&width,&height)) LOG_ERROR("Local test existing window size readback failed: {}",SDL_GetError());
            else LOG_INFO("Local test existing window readback dimensions={}x{} fullscreen={}",width,height,bool(SDL_GetWindowFlags(w)&SDL_WINDOW_FULLSCREEN));
        }
        if (g_on_window) g_on_window(w);
    }
}

// --- Public API ---

void set_title_change_callback(TitleChangeCallback cb) { g_on_title = std::move(cb); }
void set_window_created_callback(WindowCreatedCallback cb) { g_on_window = std::move(cb); }

SDL_Window* get_game_window() { return g_game_window; }
const GameTimeInfo& get_current_time_info() { return g_time_info; }

bool install_window_hooks() {
    bool ok = true;
    ok &= create_hook_by_import("SDL3.dll", "SDL_CreateWindow",
        reinterpret_cast<void*>(&detour_createWindow), reinterpret_cast<void**>(&orig_createWindow));
    ok &= create_hook_by_import("SDL3.dll", "SDL_SetWindowTitle",
        reinterpret_cast<void*>(&detour_setWindowTitle), reinterpret_cast<void**>(&orig_setWindowTitle));
    ok &= create_hook_by_import("SDL3.dll", "SDL_SetWindowFullscreen",
        reinterpret_cast<void*>(&detour_setWindowFullscreen), reinterpret_cast<void**>(&orig_setWindowFullscreen));
    ok &= create_hook_by_import("SDL3.dll", "SDL_SetWindowSize",
        reinterpret_cast<void*>(&detour_setWindowSize), reinterpret_cast<void**>(&orig_setWindowSize));
    return ok;
}

} // namespace mt2mp::hooks

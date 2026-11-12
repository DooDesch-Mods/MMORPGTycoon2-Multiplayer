#include "mt2mp/hooks/hook_window.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"

#include <string>
#include <SDL3/SDL.h>

namespace mt2mp::hooks {

static SDL_Window* g_game_window = nullptr;

void set_game_window_ptr(SDL_Window* w) {
    if (!g_game_window && w) {
        g_game_window = w;
        LOG_INFO("Game window set: 0x{:X}", reinterpret_cast<uintptr_t>(w));
        const auto& cfg=Config::instance();
        if(cfg.local_test) {
            const std::string title="MMORPG Tycoon 2 ["+(cfg.is_host?std::string("Host"):"Client "+std::to_string(cfg.peer_id-1))+"]";
            LOG_INFO("Local test existing window setup attempted");
            if(!SDL_SetWindowFullscreen(w,false)) LOG_ERROR("Local test existing window fullscreen setup failed: {}",SDL_GetError());
            if(!SDL_SetWindowSize(w,1280,720)) LOG_ERROR("Local test existing window size setup failed: {}",SDL_GetError());
            if(!SDL_SetWindowTitle(w,title.c_str())) LOG_ERROR("Local test existing window title setup failed: {}",SDL_GetError());
            int width=0,height=0;
            if(!SDL_GetWindowSizeInPixels(w,&width,&height)) LOG_ERROR("Local test existing window size readback failed: {}",SDL_GetError());
            else LOG_INFO("Local test existing window readback dimensions={}x{} fullscreen={}",width,height,bool(SDL_GetWindowFlags(w)&SDL_WINDOW_FULLSCREEN));
        }
    }
}

SDL_Window* get_game_window() { return g_game_window; }

} // namespace mt2mp::hooks

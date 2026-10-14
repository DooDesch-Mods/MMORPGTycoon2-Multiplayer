#include "mt2mp/hooks/hook_game.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::hooks {

static void* orig_set_mode = nullptr;
static int g_current_mode = 0;
static GameModeCallback g_on_mode_change;

// Safe passthrough: preserve return value (rax) by returning what original returns.
// On x64, up to 4 int/ptr args in rcx,rdx,r8,r9. Float args in xmm0-3.
// We forward all 4 integer regs and return whatever original returns.
static uint64_t __fastcall detour_set_mode(void* rcx, void* rdx, void* r8, void* r9) {
    LOG_INFO("Game::SetMode called");
    return reinterpret_cast<uint64_t(__fastcall*)(void*, void*, void*, void*)>(orig_set_mode)(rcx, rdx, r8, r9);
}

void set_game_mode_callback(GameModeCallback cb) { g_on_mode_change = std::move(cb); }
int get_current_game_mode() { return g_current_mode; }
bool is_in_game() { return g_current_mode == 2; }

bool install_game_hooks() {
    return create_hook_by_string(
        "set mode:",
        reinterpret_cast<void*>(&detour_set_mode),
        &orig_set_mode,
        "Game::SetMode"
    );
}

} // namespace mt2mp::hooks

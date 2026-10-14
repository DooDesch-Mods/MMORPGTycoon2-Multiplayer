#pragma once

#include <functional>

namespace mt2mp::hooks {

using GameModeCallback = std::function<void(int new_mode)>;
void set_game_mode_callback(GameModeCallback cb);
int get_current_game_mode(); // 0=Init, 1=Title, 2=InGame
bool is_in_game();

bool install_game_hooks();

} // namespace mt2mp::hooks

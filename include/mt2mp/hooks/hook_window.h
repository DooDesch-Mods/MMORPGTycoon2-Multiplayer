#pragma once

struct SDL_Window;

namespace mt2mp::hooks {

// The Framework render path reports the game window once; a local test run also forces it to a
// titled 1280x720 window.
void set_game_window_ptr(SDL_Window* window);
SDL_Window* get_game_window();

} // namespace mt2mp::hooks

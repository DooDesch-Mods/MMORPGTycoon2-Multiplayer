#pragma once

#include <functional>
#include <string>
#include <cstdint>

struct SDL_Window;

namespace mt2mp::hooks {

struct GameTimeInfo {
    std::string raw_title;
    int game_day = -1;
    int speed = -1; // 0=pause, 1=normal, 2=fast, 3=fastest
};

using TitleChangeCallback = std::function<void(const GameTimeInfo& info)>;
using WindowCreatedCallback = std::function<void(SDL_Window* window)>;

void set_title_change_callback(TitleChangeCallback cb);
void set_window_created_callback(WindowCreatedCallback cb);

SDL_Window* get_game_window();
const GameTimeInfo& get_current_time_info();

bool install_window_hooks();

} // namespace mt2mp::hooks

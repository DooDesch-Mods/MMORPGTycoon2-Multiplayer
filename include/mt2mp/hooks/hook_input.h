#pragma once

#include <cstdint>

namespace mt2mp::hooks {

struct MouseState {
    int32_t screen_x = 0;
    int32_t screen_y = 0;
    uint32_t buttons = 0;
};

using InputEventObserver = void(*)(void* event);
void set_input_event_observer(InputEventObserver observer);
#ifdef MT2MP_FRAMEWORK_PLUGIN
// Only an accepted SDK copy can reach this passive diagnostic forwarding path.
void forward_framework_input_event(void* event);
#endif

const MouseState& get_mouse_state();

} // namespace mt2mp::hooks

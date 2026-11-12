#include "mt2mp/hooks/hook_input.h"
#include <SDL3/SDL.h>

namespace mt2mp::hooks {
static InputEventObserver input_observer=nullptr;
static MouseState mouse;

void set_input_event_observer(InputEventObserver observer) { input_observer = observer; }

void forward_framework_input_event(void* raw) {
    const auto* event = static_cast<const SDL_Event*>(raw);
    if (!event) return;
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        mouse.screen_x = static_cast<int32_t>(event->motion.x);
        mouse.screen_y = static_cast<int32_t>(event->motion.y);
        mouse.buttons = event->motion.state;
    }
    if (input_observer) input_observer(raw);
}

const MouseState& get_mouse_state() { return mouse; }
}

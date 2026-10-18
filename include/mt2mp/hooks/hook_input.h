#pragma once

#include <functional>
#include <cstdint>
#include <string>

namespace mt2mp::hooks {

struct MouseState {
    int32_t screen_x = 0;
    int32_t screen_y = 0;
    uint32_t buttons = 0;
};

struct ChatInputState {
    bool overlay_active = false;
    bool text_input_requested = false;
    uint64_t f9_consumed = 0;
    uint64_t escape_consumed = 0;
    uint64_t enter_captured = 0;
    uint64_t text_captured = 0;
    uint64_t key_forwarded = 0;
    uint64_t probe_forwarded = 0;
    bool text_pending = false;
};

using InputFilterCallback = std::function<bool(void* event)>; // return true to consume
void set_input_filter_callback(InputFilterCallback cb);
using InputEventObserver = void(*)(void* event);
void set_input_event_observer(InputEventObserver observer);
#ifdef MT2MP_FRAMEWORK_PLUGIN
// Only an accepted SDK copy can reach this passive diagnostic forwarding path.
void forward_framework_input_event(void* event);
#endif

const MouseState& get_mouse_state();
ChatInputState read_chat_input_state();
bool queue_chat_ui_text(const std::string& text, std::string& error);

bool install_input_hooks();

} // namespace mt2mp::hooks

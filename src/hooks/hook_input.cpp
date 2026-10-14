#include "mt2mp/hooks/hook_input.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/hooks/hook_window.h"
#include "mt2mp/overlay/gl_overlay.h"
#include "mt2mp/overlay/lobby_overlay.h"
#include "mt2mp/overlay/chat_overlay.h"
#include "mt2mp/core/logger.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <atomic>
#include <memory>
#include <mutex>

namespace mt2mp::hooks {
using PollEventFn = bool(*)(SDL_Event*);
using HideCursorFn = bool(*)();
using RelativeMouseFn = bool(*)(SDL_Window*, bool);
static PollEventFn original_poll = nullptr;
static HideCursorFn original_hide = nullptr;
static RelativeMouseFn original_relative = nullptr;
static InputFilterCallback input_filter;
static InputEventObserver input_observer=nullptr;
static MouseState mouse;
static std::atomic<bool> overlay_active{false};
static bool restore_relative = false;
static std::atomic<bool> text_input_active{false};
static bool suppress_escape_up = false;
static std::atomic<uint64_t> chat_f9_consumed{0};
static std::atomic<uint64_t> chat_escape_consumed{0};
static std::atomic<uint64_t> chat_enter_captured{0};
static std::atomic<uint64_t> chat_text_captured{0};
static std::atomic<uint64_t> chat_key_forwarded{0};
static std::atomic<uint64_t> chat_probe_forwarded{0};
static std::mutex chat_text_mutex;
static std::unique_ptr<std::string> pending_chat_text;
bool g_overlay_active_flag = false;

static void update_overlay_state() {
    const bool next = overlay::LobbyOverlay::instance().is_visible() || overlay::ChatOverlay::instance().is_visible();
    const bool wants_text = overlay::LobbyOverlay::instance().is_visible() || overlay::ChatOverlay::instance().is_typing();
    if (next == overlay_active && wants_text == text_input_active) return;
    SDL_Window* window = get_game_window();
    if (next && !overlay_active) {
        restore_relative = window && SDL_GetWindowRelativeMouseMode(window);
        if (window && original_relative && !original_relative(window, false)) LOG_WARN("Disable relative mouse failed: {}", SDL_GetError());
        if (!SDL_ShowCursor()) LOG_WARN("Show cursor failed: {}", SDL_GetError());
    }
    if (window && wants_text != text_input_active) {
        if (wants_text && !SDL_StartTextInput(window)) LOG_WARN("Start text input failed: {}", SDL_GetError());
        if (!wants_text && !SDL_StopTextInput(window)) LOG_WARN("Stop text input failed: {}", SDL_GetError());
    }
    text_input_active = wants_text;
    if (!next && overlay_active) {
        if (window && restore_relative && original_relative && !original_relative(window, true)) LOG_WARN("Restore relative mouse failed: {}", SDL_GetError());
    }
    overlay_active = next;
    LOG_INFO("Overlay {}", next ? "opened" : "closed");
}
static bool hide_cursor() {
    if (overlay::LobbyOverlay::instance().is_visible() || overlay::ChatOverlay::instance().is_visible()) return SDL_ShowCursor();
    return original_hide();
}
static bool set_relative(SDL_Window* window, bool enabled) {
    if (overlay::LobbyOverlay::instance().is_visible() || overlay::ChatOverlay::instance().is_visible()) {
        restore_relative = enabled;
        return original_relative(window, false);
    }
    return original_relative(window, enabled);
}
static bool poll_event(SDL_Event* event) {
    update_overlay_state();
    while (original_poll(event)) {
        if (!event) return true;
        if (event->type == SDL_EVENT_MOUSE_MOTION) {
            mouse.screen_x = event->motion.x;
            mouse.screen_y = event->motion.y;
            mouse.buttons = event->motion.state;
        }
        if ((event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) && event->key.scancode == SDL_SCANCODE_F10) {
            if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
                overlay::LobbyOverlay::instance().toggle();
                update_overlay_state();
            }
            continue;
        }
        if ((event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) && event->key.scancode == SDL_SCANCODE_F9) {
            if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
                overlay::ChatOverlay::instance().toggle();
                ++chat_f9_consumed;
                update_overlay_state();
            }
            continue;
        }
        if (event->type == SDL_EVENT_KEY_UP && event->key.scancode == SDL_SCANCODE_ESCAPE && suppress_escape_up) {
            suppress_escape_up = false; continue;
        }
        if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_ESCAPE &&
            overlay::ChatOverlay::instance().is_typing()) {
            overlay::ChatOverlay::instance().toggle();
            ++chat_escape_consumed;
            suppress_escape_up = true;
            update_overlay_state();
            continue;
        }
        if (overlay_active) {
            const bool imgui_processed=overlay::GLOverlay::instance().process_event(event);
            if (ImGui::GetCurrentContext()) {
                const auto& io = ImGui::GetIO();
                const bool mouse_event = event->type >= SDL_EVENT_MOUSE_MOTION && event->type <= SDL_EVENT_MOUSE_WHEEL;
                const bool key_event = event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP || event->type == SDL_EVENT_TEXT_EDITING || event->type == SDL_EVENT_TEXT_INPUT;
                if ((mouse_event && io.WantCaptureMouse) ||
                    (key_event && ((overlay::LobbyOverlay::instance().is_visible() && io.WantCaptureKeyboard) ||
                                   overlay::ChatOverlay::instance().is_typing()))) {
                    if (overlay::ChatOverlay::instance().is_typing()) {
                        if (event->type == SDL_EVENT_TEXT_INPUT && imgui_processed) {
                            ++chat_text_captured;
                            std::lock_guard lock(chat_text_mutex);
                            if (pending_chat_text && event->text.text == pending_chat_text->c_str()) pending_chat_text.reset();
                        }
                        if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_RETURN) ++chat_enter_captured;
                    }
                    continue;
                }
            }
        }
        if (input_filter && input_filter(event)) continue;
        if (input_observer) input_observer(event);
        if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
            ++chat_key_forwarded;
            if (event->type == SDL_EVENT_KEY_UP && event->key.scancode == SDL_SCANCODE_UNKNOWN) ++chat_probe_forwarded;
        }
        return true;
    }
    return false;
}
void set_input_filter_callback(InputFilterCallback callback) { input_filter = std::move(callback); }
void set_input_event_observer(InputEventObserver observer) { input_observer = observer; }
const MouseState& get_mouse_state() { return mouse; }
ChatInputState read_chat_input_state() {
    std::lock_guard lock(chat_text_mutex);
    return {overlay_active.load(), text_input_active.load(), chat_f9_consumed.load(),
        chat_escape_consumed.load(), chat_enter_captured.load(), chat_text_captured.load(),
        chat_key_forwarded.load(), chat_probe_forwarded.load(), pending_chat_text != nullptr};
}
bool queue_chat_ui_text(const std::string& text, std::string& error) {
    std::lock_guard lock(chat_text_mutex);
    if (pending_chat_text) { error="previous chat text event has no confirmed ImGui copy"; return false; }
    SDL_Window* window=get_game_window();
    if (!window) { error="chat UI game window unavailable"; return false; }
    pending_chat_text=std::make_unique<std::string>(text);
    SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;
    event.text.windowID=SDL_GetWindowID(window);event.text.text=pending_chat_text->c_str();
    if (!SDL_PushEvent(&event)) {
        error="SDL chat text queue failed: "+std::string(SDL_GetError());
        pending_chat_text.reset();
        return false;
    }
    return true;
}
bool install_input_hooks() {
    bool ok = create_hook_by_import("SDL3.dll", "SDL_PollEvent", reinterpret_cast<void*>(&poll_event), reinterpret_cast<void**>(&original_poll));
    ok &= create_hook_by_import("SDL3.dll", "SDL_HideCursor", reinterpret_cast<void*>(&hide_cursor), reinterpret_cast<void**>(&original_hide));
    ok &= create_hook_by_import("SDL3.dll", "SDL_SetWindowRelativeMouseMode", reinterpret_cast<void*>(&set_relative), reinterpret_cast<void**>(&original_relative));
    return ok;
}
}

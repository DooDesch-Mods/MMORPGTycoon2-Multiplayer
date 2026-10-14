#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mt2mp::game::hud_time {
struct Config {
    uintptr_t image = 0;
    uint32_t game_thread = 0;
    std::string executable_sha256;
};
struct Services {
    void* user = nullptr;
    uint32_t (*thread_id)(void*) = nullptr;
    bool (*read)(void*, uintptr_t, void*, size_t, std::string&) = nullptr;
};
struct ItemState {
    float scale = 0;
    int32_t native_state = 0;
    bool active = false;
    bool button_cached_active = false;
    bool button_enabled = false;
    bool actor_owner_present = false;
};
struct State {
    bool complete = false;
    bool clock_panel_visible = false;
    bool pause_window_present = false;
    bool pause_window_visible = false;
    bool native_pause_indicator = false;
    bool pause_option_enabled = false;
    float native_selected_scale = 0;
    float native_pause_scale = 0;
    int32_t flat_day = 0;
    int32_t seconds = 0;
    uint32_t panel_children = 0;
    uint32_t time_items = 0;
    std::array<ItemState, 4> items{};
};
struct ActivationBinding {
    bool matched = false, visible = false, enabled = false;
    uintptr_t world = 0, panel = 0, slot = 0, button = 0, item = 0, type = 0;
    uintptr_t item_proxy = 0, type_proxy = 0;
    int32_t native_item_state = -1;
    float scale = 0, native_selected_scale = 0;
    bool operator==(const ActivationBinding&) const = default;
};
class Reader {
public:
    bool initialize(const Config&, const Services&, std::string&);
    bool read(State&, std::string&);
    bool classify_activation(uintptr_t type, uintptr_t item, ActivationBinding&, std::string&);
    bool button_binding(float scale, ActivationBinding&, std::string&);
private:
    friend class Writer;
    friend class Calendar;
    struct Witness {
        std::array<uintptr_t, 8> roots{};
        std::array<uintptr_t, 4> items{}, types{}, slots{}, buttons{}, proxies{}, type_proxies{};
        bool operator==(const Witness&) const;
    };
    bool read_bound(State&, Witness&, std::string&);
    bool once(State&, Witness&, std::string&);
    static void activation_binding(const State&, const Witness&, size_t, ActivationBinding&);
    bool bytes(uintptr_t, void*, size_t, std::string&) const;
    template<class T> bool at(uintptr_t p, size_t offset, T& value, std::string& e) const {
        if (!p || offset > UINTPTR_MAX-p) { e="hud_time_address_invalid"; return false; }
        return bytes(p+offset, &value, sizeof(value), e);
    }
    bool vtable(uintptr_t, uintptr_t, const char*, std::string&) const;
    bool boolean(uintptr_t, size_t, bool&, const char*, std::string&) const;
    bool name(uintptr_t, size_t, const char*, std::string&) const;
    Config config_{};
    Services io_{};
    bool ready_ = false;
    bool attempted_ = false;
    bool busy_ = false;
};
}

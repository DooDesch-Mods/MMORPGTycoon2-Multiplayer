#pragma once

#include <atomic>

namespace mt2mp::overlay {

class ChatFocus {
public:
    void toggle() {
        const bool next = !visible_.load();
        visible_ = next;
        typing_ = next;
        if (next) focus_next_ = true;
    }
    void close() { visible_ = false; typing_ = false; }
    bool visible() const { return visible_.load(); }
    bool typing() const { return visible() && typing_.load(); }
    void set_input_active(bool active) { typing_ = visible() && active; }
    void request_input_focus() { if (visible()) { focus_next_ = true; typing_ = true; } }
    bool take_focus_request() { return visible() && focus_next_.exchange(false); }

private:
    std::atomic<bool> visible_{false};
    std::atomic<bool> typing_{false};
    std::atomic<bool> focus_next_{false};
};

} // namespace mt2mp::overlay

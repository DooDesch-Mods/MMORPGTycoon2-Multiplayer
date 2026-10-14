#pragma once

#include "mt2mp/overlay/chat_focus.h"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace mt2mp::overlay {

struct ChatUiSnapshot {
    bool visible = false;
    bool typing = false;
    bool rendered_visible = false;
    bool input_active = false;
    uint64_t rendered_frames = 0;
    std::string draft;
};

class ChatOverlay {
public:
    static ChatOverlay& instance();

    void render();
    void toggle();
    bool is_visible() const;
    bool is_typing() const;
    ChatUiSnapshot ui_snapshot() const;

private:
    ChatOverlay() = default;
    char input_buf_[256] = {};
    ChatFocus focus_;
    std::string input_error_;
    size_t last_count_ = 0;
    mutable std::mutex ui_mutex_;
    ChatUiSnapshot observed_;
    void publish_ui(bool rendered_visible, bool input_active);
};

} // namespace mt2mp::overlay

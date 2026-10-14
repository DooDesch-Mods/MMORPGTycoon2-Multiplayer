#pragma once

namespace mt2mp::overlay {

class GLOverlay {
public:
    static GLOverlay& instance();

    bool init();    // Called once from renderer hook
    void shutdown();
    void render();  // Called each frame from SwapWindow hook

    bool wants_input() const; // True when overlay UI is focused
    bool process_event(void* sdl_event); // Returns true if consumed

private:
    GLOverlay() = default;
    bool initialized_ = false;
};

} // namespace mt2mp::overlay

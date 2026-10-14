#pragma once

namespace mt2mp::sync {

// Master sync controller - orchestrates all sync subsystems
class SyncController {
public:
    static SyncController& instance();

    bool init();
    void shutdown();

    // Per-frame update
    void update(float dt);

    // Session management
    void on_session_start(bool as_host);
    void on_session_end();

    bool is_active() const;

private:
    SyncController() = default;
    bool active_ = false;
    bool is_host_ = false;
    float cursor_timer_ = 0.0f;
    float heartbeat_timer_ = 0.0f;
    float time_sync_timer_ = 0.0f;
    float checksum_timer_ = 0.0f;
};

} // namespace mt2mp::sync

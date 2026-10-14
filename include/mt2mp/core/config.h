#pragma once

#include <string>
#include <cstdint>

namespace mt2mp {

// Version
constexpr const char* MOD_VERSION = "0.1.0";
constexpr const char* MOD_NAME = "MT2_Multiplayer";

// Network
constexpr int NET_CHANNEL_RELIABLE = 0;
constexpr int NET_CHANNEL_UNRELIABLE = 1;
constexpr int CURSOR_UPDATE_HZ = 30;
constexpr int TIME_SYNC_INTERVAL_SEC = 5;
constexpr int DESYNC_CHECK_INTERVAL_SEC = 60;
constexpr int SAVE_CHUNK_SIZE = 64 * 1024; // 64 KB
constexpr int HEARTBEAT_INTERVAL_MS = 2000;
constexpr int HEARTBEAT_TIMEOUT_MS = 10000;

// Region locking
constexpr int REGION_LOCK_TIMEOUT_SEC = 30;

// Overlay
constexpr uint16_t OVERLAY_TOGGLE_KEY = 0; // F10 scancode, set at runtime
constexpr float CURSOR_INTERP_SPEED = 15.0f;

// Paths (populated at runtime)
struct Config {
    std::string game_dir;
    std::string user_data_dir;
    std::string log_file;
    std::string original_steam_dll;
    bool is_host = false;
    bool overlay_visible = false;
    bool chat_visible = false;
    bool local_test = false;
    uint64_t peer_id = 0;
    uint64_t session_id = 0;
    std::string session_token;

    // Sidecar relay
    bool relay_enabled = true;
    std::string relay_host = "127.0.0.1";
    uint16_t relay_port = 47010;
    uint16_t sidecar_control_port = 47001;

    static Config& instance();
private:
    Config() = default;
};

} // namespace mt2mp

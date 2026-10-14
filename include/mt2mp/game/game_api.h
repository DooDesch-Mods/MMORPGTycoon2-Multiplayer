#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace mt2mp::game {

// ── Game Session State ───────────────────────────────────────
enum class SessionState {
    Idle,           // In main menu, not connected
    InLobby,        // In lobby, waiting for host to start
    Loading,        // Save is being transferred/loaded
    InGame,         // Actively playing
    Syncing,        // Receiving state update from host
};

// ── Game Action Types (detected via autosave diffs) ──────────
enum class ActionCategory {
    Unknown,
    BuildingPlace,
    BuildingDestroy,
    TerrainEdit,
    MonsterPlace,
    NPCPlace,
    ZoneCreate,
    PathBuild,
    DungeonEdit,
    SceneryPlace,
    RegionEdit,
    NetworkBuild,
    GameSpeedChange,
    Save,
    Load,
};

// ── Game Info (shared via Lobby Data) ────────────────────────
struct GameInfo {
    std::string game_name;       // "MP Test"
    std::string game_state;      // "menu", "loading", "ingame"
    uint32_t save_crc = 0;       // CRC32 of current state
    uint32_t save_size = 0;      // uncompressed state size
    int member_count = 0;        // lobby members
    bool is_host = false;
    uint64_t host_id = 0;
};

// ── Action Log Entry ─────────────────────────────────────────
struct ActionLogEntry {
    std::chrono::steady_clock::time_point timestamp;
    ActionCategory category;
    std::string description;
    uint32_t old_crc = 0;
    uint32_t new_crc = 0;
    uint32_t state_size = 0;
};

// ── The Game API ─────────────────────────────────────────────
class GameAPI {
public:
    static GameAPI& instance();

    // Lifecycle
    void init();
    void tick(float dt); // called every frame

    // Session
    SessionState session_state() const { return state_; }
    const char* session_state_string() const;
    const GameInfo& game_info() const { return info_; }

    // Action log (last N actions)
    const std::vector<ActionLogEntry>& action_log() const { return actions_; }
    void log_action(ActionCategory cat, const std::string& desc,
                    uint32_t old_crc, uint32_t new_crc, uint32_t size);

    // State change detection
    using StateChangeCallback = std::function<void(uint32_t old_crc, uint32_t new_crc)>;
    void set_state_change_callback(StateChangeCallback cb);

    // Trigger save transfer to lobby members
    void broadcast_save();

    // Set session state
    void set_state(SessionState s);

private:
    GameAPI() = default;

    SessionState state_ = SessionState::Idle;
    GameInfo info_;
    std::vector<ActionLogEntry> actions_;
    StateChangeCallback on_state_change_;

    // Timing
    float lobby_sync_timer_ = 0;
    float action_check_timer_ = 0;
    uint32_t last_known_crc_ = 0;

    static constexpr int MAX_ACTION_LOG = 50;
};

} // namespace mt2mp::game

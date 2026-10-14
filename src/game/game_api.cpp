#include "mt2mp/game/game_api.h"
#include "mt2mp/game/save_differ.h"
#include "mt2mp/game/save_diff.h"
#include "mt2mp/game/player_info.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/net/save_transfer.h"
#include "mt2mp/overlay/notification_overlay.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"

namespace mt2mp::game {

GameAPI& GameAPI::instance() {
    static GameAPI api;
    return api;
}

void GameAPI::init() {
    LOG_INFO("[API] Game API initialized");

    // Wire save state change detection (use add_ to not overwrite other callbacks)
    SaveStateTracker::instance().add_change_callback(
        [this](uint32_t old_crc, uint32_t new_crc, size_t size) {
            ActionCategory cat = ActionCategory::Unknown;
            std::string desc;

            if (old_crc == 0) {
                cat = ActionCategory::Load;
                desc = "Game loaded";
            } else if (size < 100000 && old_crc != 0) {
                cat = ActionCategory::Save;
                desc = "Autosave (metadata)";
            } else {
                // Full state change = player performed an action
                size_t prev_size = SaveStateTracker::instance().current_state_size();
                int64_t size_delta = static_cast<int64_t>(size) - static_cast<int64_t>(prev_size);

                if (std::abs(size_delta) > 10000) {
                    cat = ActionCategory::BuildingPlace;
                    desc = std::string("Major edit (") + (size_delta > 0 ? "+" : "") +
                           std::to_string(size_delta / 1024) + "KB)";
                } else if (std::abs(size_delta) > 500) {
                    cat = ActionCategory::TerrainEdit;
                    desc = std::string("Medium edit (") + (size_delta > 0 ? "+" : "") +
                           std::to_string(size_delta) + "B)";
                } else {
                    cat = ActionCategory::GameSpeedChange;
                    desc = "Minor edit (config/time)";
                }
            }
            log_action(cat, desc, old_crc, new_crc, static_cast<uint32_t>(size));

            // Compute binary diff between previous and current state
            const auto& prev = SaveStateTracker::instance().previous_state();
            const auto& curr = SaveStateTracker::instance().current_state();
            if (!prev.empty() && !curr.empty() && old_crc != 0) {
                auto diff = DiffEngine::compute(prev, curr, old_crc, new_crc);
                // Log diff stats for analysis
                float pct = diff.wire_size() * 100.0f / std::max<size_t>(1, curr.size());
                LOG_INFO("[API:DIFF] {} changed blocks, {} bytes diff ({:.1f}% of full {}KB state)",
                    diff.blocks.size(), diff.wire_size(), pct, curr.size() / 1024);
            }

            if (on_state_change_) {
                on_state_change_(old_crc, new_crc);
            }

            // Auto-broadcast if host and in lobby with members
            auto& mm = net::SteamMatchmaking::instance();
            auto& net_inst = net::SteamNetworking::instance();
            if (mm.in_lobby() && mm.is_host()) {
                int members = net_inst.get_lobby_member_count(mm.current_lobby_id());
                if (members > 1 && state_ == SessionState::InGame) {
                    LOG_INFO("[API] State changed while hosting - will broadcast on next autosave");
                    // Update lobby data with new CRC
                    char crc_str[16];
                    snprintf(crc_str, sizeof(crc_str), "0x%08X", new_crc);
                    mm.set_lobby_data("save_crc", crc_str);
                }
            }
        });

    state_ = SessionState::Idle;
}

void GameAPI::tick(float dt) {
    auto& mm = net::SteamMatchmaking::instance();
    auto& net_inst = net::SteamNetworking::instance();
    auto& tracker = SaveStateTracker::instance();

    // Update session state based on lobby membership
    if (mm.in_lobby()) {
        if (state_ == SessionState::Idle) {
            set_state(SessionState::InLobby);
        }

        // Update game info
        info_.game_name = mm.get_lobby_data("game_name");
        info_.game_state = mm.get_lobby_data("game_state");
        info_.is_host = mm.is_host();
        info_.host_id = net_inst.get_lobby_owner(mm.current_lobby_id());
        info_.member_count = net_inst.get_lobby_member_count(mm.current_lobby_id());
        info_.save_crc = tracker.current_hash();
        info_.save_size = static_cast<uint32_t>(tracker.current_state_size());

        // Detect state transitions
        if (info_.game_state == "ingame" && state_ == SessionState::InLobby) {
            if (info_.is_host) {
                set_state(SessionState::InGame);
            } else {
                // Joiner: check if we're receiving a save
                if (net::SaveTransfer::instance().is_receiving()) {
                    set_state(SessionState::Loading);
                }
            }
        }

        if (net::SaveTransfer::instance().is_receiving()) {
            if (state_ != SessionState::Loading) {
                set_state(SessionState::Loading);
            }
        } else if (state_ == SessionState::Loading) {
            // Transfer complete
            set_state(SessionState::InLobby);
        }

    } else {
        if (state_ != SessionState::Idle) {
            set_state(SessionState::Idle);
        }
    }

    // Periodic logging (every 30 seconds)
    action_check_timer_ += dt;
    if (action_check_timer_ >= 30.0f) {
        action_check_timer_ = 0;
        LOG_INFO("[API] Status: state={} game='{}' crc=0x{:08X} size={}KB members={} actions={}",
            session_state_string(),
            info_.game_name,
            info_.save_crc,
            info_.save_size / 1024,
            info_.member_count,
            actions_.size());
    }
}

void GameAPI::set_state(SessionState s) {
    if (state_ == s) return;
    LOG_INFO("[API] Session: {} -> {}", session_state_string(),
        [&]() { auto old = state_; state_ = s; auto r = session_state_string(); state_ = old; return r; }());
    state_ = s;
}

const char* GameAPI::session_state_string() const {
    switch (state_) {
        case SessionState::Idle:     return "Idle";
        case SessionState::InLobby:  return "InLobby";
        case SessionState::Loading:  return "Loading";
        case SessionState::InGame:   return "InGame";
        case SessionState::Syncing:  return "Syncing";
        default: return "?";
    }
}

void GameAPI::log_action(ActionCategory cat, const std::string& desc,
                          uint32_t old_crc, uint32_t new_crc, uint32_t size) {
    static const char* cat_names[] = {
        "Unknown", "BuildingPlace", "BuildingDestroy", "TerrainEdit",
        "MonsterPlace", "NPCPlace", "ZoneCreate", "PathBuild",
        "DungeonEdit", "SceneryPlace", "RegionEdit", "NetworkBuild",
        "GameSpeedChange", "Save", "Load"
    };
    int cat_idx = static_cast<int>(cat);
    const char* cat_name = (cat_idx >= 0 && cat_idx < 15) ? cat_names[cat_idx] : "?";

    LOG_INFO("[API:ACTION] {} | {} | CRC 0x{:08X}->0x{:08X} | {}KB",
        cat_name, desc, old_crc, new_crc, size / 1024);

    ActionLogEntry entry;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.category = cat;
    entry.description = desc;
    entry.old_crc = old_crc;
    entry.new_crc = new_crc;
    entry.state_size = size;

    actions_.push_back(entry);
    if (actions_.size() > MAX_ACTION_LOG) {
        actions_.erase(actions_.begin());
    }
}

void GameAPI::broadcast_save() {
    auto& mm = net::SteamMatchmaking::instance();
    auto& tracker = SaveStateTracker::instance();

    if (!mm.in_lobby() || !mm.is_host()) {
        LOG_WARN("[API] Cannot broadcast save: not host or not in lobby");
        return;
    }

    const auto& state = tracker.current_state();
    if (state.empty()) {
        LOG_WARN("[API] Cannot broadcast save: no state captured");
        return;
    }

    LOG_INFO("[API] Broadcasting save '{}' ({} bytes) to lobby",
        info_.game_name, state.size());

    // The PhysFS hook already triggers save transfer when a .vrz is loaded
    // This method is for manual trigger
}

void GameAPI::set_state_change_callback(StateChangeCallback cb) {
    on_state_change_ = std::move(cb);
}

} // namespace mt2mp::game

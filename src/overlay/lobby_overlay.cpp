#include "mt2mp/overlay/lobby_overlay.h"
#include "mt2mp/overlay/notification_overlay.h"
#include "mt2mp/net/net_manager.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/net/packet_serializer.h"
#include "mt2mp/game/player_info.h"
#include "mt2mp/game/save_differ.h"
#include "mt2mp/net/save_transfer.h"
#include "mt2mp/game/game_api.h"
#include "mt2mp/sync/sync_controller.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"
#include "mt2mp/net/local_session.h"

#include <imgui.h>
#include <SDL3/SDL.h>
#include <windows.h>
#include <cstring>

// Theme colors
static ImVec4 COL_OK      = ImVec4(0.46f, 0.77f, 0.50f, 1.0f);
static ImVec4 COL_WARN    = ImVec4(0.89f, 0.74f, 0.42f, 1.0f);
static ImVec4 COL_ERR     = ImVec4(0.84f, 0.41f, 0.41f, 1.0f);
static ImVec4 COL_INFO    = ImVec4(0.46f, 0.63f, 0.85f, 1.0f);
static ImVec4 COL_MUTED   = ImVec4(0.57f, 0.59f, 0.63f, 1.0f);
static ImVec4 COL_BRASS   = ImVec4(0.78f, 0.65f, 0.40f, 1.0f);
static ImVec4 COL_PRIMARY = ImVec4(0.30f, 0.45f, 0.72f, 1.0f);

namespace mt2mp::overlay {

// Session state (simpler than before)
enum class SessionMode { None, Hosting, Joined };
static SessionMode s_mode = SessionMode::None;

LobbyOverlay& LobbyOverlay::instance() {
    static LobbyOverlay inst;
    return inst;
}

bool LobbyOverlay::is_visible() const { return visible_; }
void LobbyOverlay::toggle() { visible_ = !visible_; }
void LobbyOverlay::set_visible(bool v) { visible_ = v; }

// Helper: big centered button
static bool BigButton(const char* label, ImVec2 size = ImVec2(0, 38)) {
    if (size.x <= 0) size.x = ImGui::GetContentRegionAvail().x;
    return ImGui::Button(label, size);
}

// Helper: status dot
static void StatusDot(ImVec4 color, const char* text) {
    ImGui::TextColored(color, "●");
    ImGui::SameLine();
    ImGui::TextColored(color, "%s", text);
}

void LobbyOverlay::render() {
    if (!visible_) return;

    ImGui::SetNextWindowSize(ImVec2(520, 460), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 80), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("MT2 Multiplayer###mt2mp", &visible_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (Config::instance().local_test) {
        const auto& cfg = Config::instance();
        ImGui::Text("Lokaler Test - %s %llu", cfg.is_host ? "Host" : "Peer", cfg.peer_id);
        auto state = net::local_session_status();
        ImGui::TextColored(net::local_session_connected() ? COL_OK : COL_WARN, "%s", state.c_str());
        ImGui::Text("Verbundene Gegenstellen: %zu", net::local_session_peers());
        ImGui::Separator();
        ImGui::TextWrapped("Welt-Synchronisation: Engine-Nachweis ausstehend");
        ImGui::TextWrapped("Eigener Datenpfad: %s", cfg.user_data_dir.c_str());
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Steam-Koop: Engine-Nachweis ausstehend");
    ImGui::End();
}

void LobbyOverlay::render_main_menu() {
    // Welcome screen - clean and simple
    ImGui::Spacing();

    ImGui::TextColored(COL_BRASS, "Welcome to Multiplayer");
    ImGui::Spacing();
    ImGui::TextColored(COL_MUTED, "Play MMORPG Tycoon 2 together with friends.");
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    // Two big action buttons
    float half_w = (ImGui::GetContentRegionAvail().x - 12) * 0.5f;

    // HOST button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.35f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.44f, 0.65f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.52f, 0.75f, 1.0f));
    if (ImGui::Button("Host Game", ImVec2(half_w, 60))) {
        if (s_mode != SessionMode::Hosting) {
            net::NetManager::instance().create_lobby(4);
            s_mode = SessionMode::Hosting;
            LOG_INFO("UI: Host Game clicked, mode=Hosting");
            LOG_INFO("UI: local_id={}, peers={}, registry_size={}",
                net::SteamNetworking::instance().local_steam_id(),
                net::SteamNetworking::instance().peers().size(),
                game::PlayerRegistry::instance().all().size());
        }
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 12);

    // JOIN button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.30f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.40f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.34f, 0.48f, 0.65f, 1.0f));
    if (ImGui::Button("Join Game", ImVec2(half_w, 60))) {
        s_mode = SessionMode::Joined;
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Spacing();

    // Brief descriptions
    ImGui::Columns(2, "##desc", false);
    ImGui::TextColored(COL_MUTED, "Create a lobby and invite");
    ImGui::TextColored(COL_MUTED, "friends to your world.");
    ImGui::NextColumn();
    ImGui::TextColored(COL_MUTED, "Connect to a friend's");
    ImGui::TextColored(COL_MUTED, "hosted session.");
    ImGui::Columns(1);

    // Footer
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
    ImGui::Separator();
    ImGui::TextColored(COL_MUTED, "MT2 Multiplayer v0.1.0");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
    ImGui::TextColored(COL_MUTED, "F10 = Close");
}

void LobbyOverlay::render_session_view() {
    auto local_id = net::SteamNetworking::instance().local_steam_id();
    int lobby_members = 0;
    uint64_t lv = net::SteamMatchmaking::instance().current_lobby_id();
    if (net::SteamMatchmaking::instance().in_lobby()) {
        lobby_members = net::SteamNetworking::instance().get_lobby_member_count(lv);
    }
    auto peer_count = (lobby_members > 1) ? static_cast<size_t>(lobby_members - 1) : size_t(0);
    bool is_hosting = (s_mode == SessionMode::Hosting);
    bool lobby_created = net::SteamMatchmaking::instance().in_lobby();

    // ── Top bar: session info ────────────────────────────────
    if (is_hosting) {
        ImGui::TextColored(COL_BRASS, "Your Lobby");
    } else {
        ImGui::TextColored(COL_BRASS, "Join Session");
    }
    // Right-aligned status (enough space for longest text)
    {
        const char* status_text = "";
        ImVec4 status_color = COL_MUTED;

        if (peer_count > 0) {
            static char pbuf[32];
            snprintf(pbuf, sizeof(pbuf), "%zu Player(s) connected", peer_count + 1);
            status_text = pbuf;
            status_color = COL_OK;
        } else if (is_hosting && !lobby_created) {
            status_text = "Creating lobby...";
            status_color = COL_WARN;
        } else if (is_hosting && lobby_created) {
            status_text = "Lobby open";
            status_color = COL_INFO;
        } else if (s_mode == SessionMode::Joined) {
            auto cs = net::NetManager::instance().connection_state();
            if (cs == net::ConnectionState::Connecting || cs == net::ConnectionState::Handshaking) {
                status_text = "Connecting...";
                status_color = COL_WARN;
            } else {
                status_text = "Ready to connect";
                status_color = COL_MUTED;
            }
        }

        auto text_size = ImGui::CalcTextSize(status_text);
        float status_w = text_size.x + 20; // dot + spacing
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - status_w);
        StatusDot(status_color, status_text);
    }

    ImGui::Separator();
    ImGui::Spacing();

    // If joiner successfully joined a lobby, show the session view
    if (s_mode == SessionMode::Joined && net::SteamMatchmaking::instance().in_lobby()) {
        // Joiner is now in lobby -- show player list just like host
    }

    // ── Join: show lobby ID input ──────────────────────────────
    if (s_mode == SessionMode::Joined && !net::SteamMatchmaking::instance().in_lobby()) {
        ImGui::TextColored(COL_MUTED, "Enter the Lobby ID to join:");
        ImGui::Spacing();

        static char lobby_buf[32] = "";
        ImGui::SetNextItemWidth(240);
        ImGui::InputText("##lobby_id", lobby_buf, sizeof(lobby_buf));
        ImGui::SameLine();
        if (ImGui::Button("Join Lobby", ImVec2(120, 0))) {
            uint64_t lobby_id = strtoull(lobby_buf, nullptr, 10);
            if (lobby_id > 0) {
                net::SteamMatchmaking::instance().join_lobby(lobby_id, nullptr);
                LOG_INFO("UI: Joining lobby {}", lobby_id);
                NotificationOverlay::instance().show("Joining lobby...", NotificationType::Info);
            }
        }
        ImGui::Spacing();
        ImGui::TextColored(COL_MUTED, "Or accept a Steam invite from the host.");
        ImGui::Spacing();
        ImGui::Spacing();
    }

    // ── Lobby / Player list ──────────────────────────────────
    ImGui::TextColored(COL_BRASS, "Players");
    ImGui::Spacing();

    ImGui::BeginChild("##player_list", ImVec2(0, -90), true);

    // Local player always shown with Steam name
    std::string local_name = net::SteamNetworking::instance().get_persona_name(local_id);
    ImGui::TextColored(COL_OK, "●");
    ImGui::SameLine();
    ImGui::Text("%s (You)", local_name.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
    ImGui::TextColored(COL_BRASS, is_hosting ? "Host" : "Joined");

    // Remote players from registry
    const auto& players = game::PlayerRegistry::instance().all();
    int remote_count = 0;
    for (const auto& [id, p] : players) {
        if (id == local_id) continue;
        remote_count++;

        ImVec4 col(((p.color >> 16) & 0xFF) / 255.0f,
                   ((p.color >> 8) & 0xFF) / 255.0f,
                   (p.color & 0xFF) / 255.0f, 1.0f);

        std::string pname = p.display_name.empty()
            ? net::SteamNetworking::instance().get_persona_name(id) : p.display_name;
        ImGui::TextColored(COL_OK, "●");
        ImGui::SameLine();
        ImGui::TextColored(col, "%s", pname.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
        ImGui::TextColored(COL_OK, "Online");
    }

    // Also show peers that aren't in the registry yet
    for (auto peer_id : net::SteamNetworking::instance().peers()) {
        if (peer_id == local_id) continue;
        if (game::PlayerRegistry::instance().get(peer_id)) continue; // already shown
        remote_count++;

        std::string peer_name = net::SteamNetworking::instance().get_persona_name(peer_id);
        ImGui::TextColored(COL_WARN, "●");
        ImGui::SameLine();
        ImGui::Text("%s", peer_name.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
        ImGui::TextColored(COL_WARN, "Connecting");
    }

    if (remote_count == 0 && peer_count == 0) {
        ImGui::Spacing();
        ImGui::TextColored(COL_MUTED, "Waiting for players to join...");
    }

    // Show host's game state if in lobby
    if (net::SteamMatchmaking::instance().in_lobby()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Read host's game info from lobby data
        std::string host_game = net::SteamMatchmaking::instance().get_lobby_data("game_name");
        std::string host_state = net::SteamMatchmaking::instance().get_lobby_data("game_state");

        if (!host_game.empty()) {
            if (host_state == "ingame") {
                ImGui::TextColored(COL_OK, "Host is playing: %s", host_game.c_str());
                if (!is_hosting) {
                    ImGui::Spacing();
                    ImGui::TextColored(COL_MUTED, "Make sure you have the save '%s'", host_game.c_str());
                    ImGui::TextColored(COL_MUTED, "Then load it from the main menu.");
                }
            } else if (host_state == "loading") {
                ImGui::TextColored(COL_WARN, "Host is loading: %s", host_game.c_str());
            } else {
                ImGui::TextColored(COL_MUTED, "Host's game: %s", host_game.c_str());
            }
        } else if (is_hosting) {
            ImGui::TextColored(COL_MUTED, "Load a save game to start playing.");
        }

        // Transfer progress
        auto& transfer = net::SaveTransfer::instance();
        if (transfer.is_sending()) {
            ImGui::Spacing();
            ImGui::TextColored(COL_WARN, "Sending save... %.0f%%", transfer.send_progress() * 100);
            ImGui::ProgressBar(transfer.send_progress());
        }
        if (transfer.is_receiving()) {
            ImGui::Spacing();
            ImGui::TextColored(COL_INFO, "Receiving save... %.0f%%", transfer.receive_progress() * 100);
            ImGui::ProgressBar(transfer.receive_progress());
        }

        // Game API status
        auto& api = game::GameAPI::instance();
        auto& info = api.game_info();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(COL_BRASS, "Session");
        ImGui::TextColored(COL_MUTED, "State:");
        ImGui::SameLine();
        ImGui::Text("%s", api.session_state_string());
        if (info.save_crc != 0) {
            ImGui::SameLine(200);
            ImGui::TextColored(COL_MUTED, "CRC: 0x%08X", info.save_crc);
            ImGui::SameLine();
            ImGui::TextColored(COL_MUTED, "(%uKB)", info.save_size / 1024);
        }

        // Recent actions
        const auto& actions = api.action_log();
        if (!actions.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(COL_BRASS, "Recent Actions");
            int show = std::min(static_cast<int>(actions.size()), 5);
            for (int i = static_cast<int>(actions.size()) - 1;
                 i >= static_cast<int>(actions.size()) - show; --i) {
                auto& a = actions[i];
                ImGui::TextColored(COL_MUTED, "  %s", a.description.c_str());
            }
        }
    }

    ImGui::EndChild();

    // ── Bottom actions ───────────────────────────────────────
    ImGui::Spacing();

    if (is_hosting) {
        float btn_w = (ImGui::GetContentRegionAvail().x - 24) / 3.0f;

        // Invite Friends via Steam Overlay
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.35f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.44f, 0.65f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.52f, 0.75f, 1.0f));
        if (ImGui::Button("Invite Friends", ImVec2(btn_w, 34))) {
            // Open Steam invite dialog via flat API
            static void* s_friends_iface = nullptr;
            static auto s_invite_fn = reinterpret_cast<void(*)(void*, uint64_t)>(nullptr);

            if (!s_friends_iface) {
                HMODULE steam = GetModuleHandleA("steam_api64_original.dll");
                if (steam) {
                    // Get ISteamFriends via SteamInternal
                    using FindFn = void*(*)(int32_t, const char*);
                    using GetHUserFn = int32_t(*)();
                    auto find_fn = reinterpret_cast<FindFn>(GetProcAddress(steam, "SteamInternal_FindOrCreateUserInterface"));
                    auto get_huser = reinterpret_cast<GetHUserFn>(GetProcAddress(steam, "SteamAPI_GetHSteamUser"));
                    if (find_fn && get_huser) {
                        int32_t huser = get_huser();
                        s_friends_iface = find_fn(huser, "SteamFriends018");
                        if (!s_friends_iface) s_friends_iface = find_fn(huser, "SteamFriends017");
                        if (!s_friends_iface) s_friends_iface = find_fn(huser, "SteamFriends016");
                    }
                    s_invite_fn = reinterpret_cast<void(*)(void*, uint64_t)>(
                        GetProcAddress(steam, "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog"));
                    LOG_INFO("Friends iface: 0x{:X}, invite_fn: 0x{:X}",
                        reinterpret_cast<uintptr_t>(s_friends_iface),
                        reinterpret_cast<uintptr_t>(s_invite_fn));
                }
            }

            // Copy Lobby ID to clipboard
            uint64_t lid = net::SteamMatchmaking::instance().current_lobby_id();
            char id_str[32];
            snprintf(id_str, sizeof(id_str), "%llu", lid);
            SDL_SetClipboardText(id_str);

            // Check if overlay is available
            HMODULE steam = GetModuleHandleA("steam_api64_original.dll");
            bool overlay_ok = false;
            if (steam) {
                using FnUtilsAccessor = void*(*)();
                using FnIsOverlay = bool(*)(void*);
                auto get_utils = reinterpret_cast<FnUtilsAccessor>(
                    GetProcAddress(steam, "SteamAPI_SteamUtils_v010"));
                auto is_overlay = reinterpret_cast<FnIsOverlay>(
                    GetProcAddress(steam, "SteamAPI_ISteamUtils_IsOverlayEnabled"));
                if (get_utils && is_overlay) {
                    void* utils = get_utils();
                    if (utils) overlay_ok = is_overlay(utils);
                }
                LOG_INFO("Steam Overlay enabled: {}", overlay_ok);
            }

            // Try opening Steam invite
            if (s_friends_iface && overlay_ok) {
                uint64_t lobby = net::SteamMatchmaking::instance().current_lobby_id();
                if (lobby != 0 && s_invite_fn) {
                    s_invite_fn(s_friends_iface, lobby);
                    LOG_INFO("Invite dialog requested for lobby {}", lobby);
                } else {
                    // No lobby or no invite fn -- try friends list
                    using FnOverlay = void(*)(void*, const char*);
                    auto overlay_fn = reinterpret_cast<FnOverlay>(
                        GetProcAddress(steam, "SteamAPI_ISteamFriends_ActivateGameOverlay"));
                    if (overlay_fn) {
                        overlay_fn(s_friends_iface, "friends");
                        LOG_INFO("Opened friends list (no lobby active)");
                    }
                }
                NotificationOverlay::instance().show(
                    "Steam ID copied! Opening invite...",
                    NotificationType::Success, 4.0f);
            } else {
                NotificationOverlay::instance().show(
                    overlay_ok ? "Steam ID copied! Send to friends."
                               : "Steam Overlay not available. ID copied!",
                    overlay_ok ? NotificationType::Success : NotificationType::Warning, 5.0f);
            }
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 12);

        // Copy Steam ID to clipboard
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.30f, 0.42f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.38f, 0.52f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.45f, 0.60f, 1.0f));
        if (ImGui::Button("Copy Lobby ID", ImVec2(btn_w, 34))) {
            uint64_t lid = net::SteamMatchmaking::instance().current_lobby_id();
            char id_str[32];
            snprintf(id_str, sizeof(id_str), "%llu", lid);
            SDL_SetClipboardText(id_str);
            LOG_INFO("Copied Lobby ID {} to clipboard", lid);
            NotificationOverlay::instance().show("Steam ID copied to clipboard!", NotificationType::Success, 3.0f);
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 12);

        // Leave
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.28f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.34f, 0.34f, 1.0f));
        if (ImGui::Button("Leave", ImVec2(btn_w, 34))) {
            net::NetManager::instance().leave_lobby();
            s_mode = SessionMode::None;
        }
        ImGui::PopStyleColor(3);
    } else {
        // Join mode: Disconnect + Back buttons
        float btn_w = (ImGui::GetContentRegionAvail().x - 12) * 0.5f;

        // Disconnect (if connected)
        if (peer_count > 0) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.20f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.28f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.34f, 0.34f, 1.0f));
            if (ImGui::Button("Disconnect", ImVec2(btn_w, 34))) {
                // Clear peers
                auto peer_copy = net::SteamNetworking::instance().peers();
                for (auto pid : peer_copy) {
                    net::SteamNetworking::instance().remove_peer(pid);
                    game::PlayerRegistry::instance().remove(pid);
                }
                net::NetManager::instance().leave_lobby();
                s_mode = SessionMode::None;
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine(0, 12);
        }

        // Back button (just returns to main menu, doesn't disconnect)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.28f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.36f, 0.45f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.44f, 0.55f, 1.0f));
        float back_w = (peer_count > 0) ? btn_w : ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("Back", ImVec2(back_w, 34))) {
            s_mode = SessionMode::None;
            // Don't leave lobby or disconnect -- just go back to menu
        }
        ImGui::PopStyleColor(3);
    }
}

// Stubs for old interface
void LobbyOverlay::render_create_tab() {}
void LobbyOverlay::render_browse_tab() {}
void LobbyOverlay::render_session_tab() {}

} // namespace mt2mp::overlay

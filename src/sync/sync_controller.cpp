#include "mt2mp/sync/sync_controller.h"
#include "mt2mp/sync/sync_cursor.h"
#include "mt2mp/net/net_manager.h"
#include "mt2mp/net/packet_serializer.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/game/player_info.h"
#include "mt2mp/game/save_differ.h"
#include "mt2mp/hooks/hook_input.h"
#include "mt2mp/overlay/notification_overlay.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"

#include <cstring>

namespace mt2mp::sync {

SyncController& SyncController::instance() {
    static SyncController ctrl;
    return ctrl;
}

bool SyncController::init() {
    auto& nm = net::NetManager::instance();

    // ── Handshake ────────────────────────────────────────────
    nm.register_handler(net::PacketType::HANDSHAKE_REQUEST,
        [](uint64_t sender, const net::Packet& p) {
            LOG_NET_INFO("Handshake request from {}", sender);
            auto response = net::PacketSerializer::make_handshake_response(true);
            net::NetManager::instance().send_reliable(sender, response);

            if (!SyncController::instance().is_active()) {
                SyncController::instance().on_session_start(Config::instance().is_host);
            }
            LOG_NET_INFO("Handshake accepted, response sent to {}", sender);
        });

    nm.register_handler(net::PacketType::HANDSHAKE_RESPONSE,
        [](uint64_t sender, const net::Packet& p) {
            LOG_NET_INFO("Handshake response from {} - connected!", sender);
            overlay::NotificationOverlay::instance().show(
                "Connected!", overlay::NotificationType::Success);

            if (!SyncController::instance().is_active()) {
                SyncController::instance().on_session_start(Config::instance().is_host);
            }

            // Host sends state to joiner
            if (Config::instance().is_host) {
                auto& tracker = game::SaveStateTracker::instance();
                const auto& state = tracker.current_state();
                if (!state.empty()) {
                    LOG_SYNC_INFO("Sending state to joiner ({} bytes)", state.size());
                    constexpr size_t CHUNK = 32768;
                    uint32_t idx = 0;
                    for (size_t off = 0; off < state.size(); off += CHUNK) {
                        size_t len = std::min(CHUNK, state.size() - off);
                        auto pkt = net::PacketSerializer::make_save_chunk(
                            idx++, state.data() + off, static_cast<uint32_t>(len));
                        net::NetManager::instance().send_reliable(sender, pkt);
                    }
                    net::NetManager::instance().send_reliable(sender,
                        net::PacketSerializer::make_save_complete(idx));
                }
            }
        });

    // ── Save Transfer ────────────────────────────────────────
    nm.register_handler(net::PacketType::SAVE_CHUNK,
        [](uint64_t sender, const net::Packet& p) {
            if (p.payload.size() < 4) return;
            uint32_t index;
            std::memcpy(&index, p.payload.data(), 4);
            uint32_t size = static_cast<uint32_t>(p.payload.size()) - 4;

            static std::vector<uint8_t> rx_buf;
            if (index == 0) rx_buf.clear();
            rx_buf.insert(rx_buf.end(), p.payload.data() + 4, p.payload.data() + 4 + size);
            LOG_SYNC_INFO("Save chunk {} ({} bytes, total: {})", index, size, rx_buf.size());
        });

    nm.register_handler(net::PacketType::SAVE_COMPLETE,
        [](uint64_t sender, const net::Packet& p) {
            LOG_SYNC_INFO("Save transfer complete from {}", sender);
            overlay::NotificationOverlay::instance().show(
                "World state received!", overlay::NotificationType::Success, 5.0f);
        });

    // ── Cursor ───────────────────────────────────────────────
    nm.register_handler(net::PacketType::CURSOR_UPDATE,
        [](uint64_t sender, const net::Packet& p) {
            auto cp = net::PacketSerializer::read_cursor(p);
            game::PlayerRegistry::instance().update_cursor(
                sender, cp.world_x, cp.world_y, cp.world_z, cp.cursor_type, "");
            // Don't log every cursor update (30Hz would flood)
        });

    // ── Heartbeat ────────────────────────────────────────────
    nm.register_handler(net::PacketType::HEARTBEAT,
        [](uint64_t sender, const net::Packet&) {
            LOG_SYNC_DEBUG("Heartbeat from {}", sender);
        });

    // ── Checksum ─────────────────────────────────────────────
    nm.register_handler(net::PacketType::CHECKSUM,
        [](uint64_t sender, const net::Packet& p) {
            auto cs = net::PacketSerializer::read_checksum(p);
            auto local = game::SaveStateTracker::instance().current_hash();
            if (local != 0 && cs.hash != 0 && cs.hash != local) {
                LOG_SYNC_INFO("DESYNC! Local=0x{:08X} Remote=0x{:08X}", local, cs.hash);
                overlay::NotificationOverlay::instance().show(
                    "Desync detected!", overlay::NotificationType::Warning);
            }
        });

    // State broadcasting is handled by PhysFS hooks (autosave write detection)
    // + SaveTransfer via Lobby Data streaming. No P2P packets needed.

    LOG_SYNC_INFO("SyncController initialized");
    return true;
}

void SyncController::shutdown() { active_ = false; }

void SyncController::update(float dt) {
    if (!active_) return;

    // In Lobby Data mode, cursor sync and heartbeat are handled by MT2MP_Tick
    // via SetLobbyMemberData / GetLobbyMemberData. No packet-based sync needed.

    // Checksum via lobby data (host writes, clients read)
    // TODO: implement via SetLobbyData when needed
}

void SyncController::on_session_start(bool as_host) {
    if (active_) {
        LOG_SYNC_DEBUG("Session already active, ignoring duplicate start");
        return;
    }
    is_host_ = as_host;
    active_ = true;
    cursor_timer_ = heartbeat_timer_ = checksum_timer_ = 0.0f;
    LOG_SYNC_INFO("Session started (host={}, peers={})",
        as_host, net::SteamNetworking::instance().peers().size());
}

void SyncController::on_session_end() {
    active_ = false;
    LOG_SYNC_INFO("Session ended");
}

bool SyncController::is_active() const { return active_; }

} // namespace mt2mp::sync

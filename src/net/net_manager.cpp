#include "mt2mp/net/net_manager.h"
#include "mt2mp/net/packet_serializer.h"
#include "mt2mp/sync/sync_controller.h"
#include "mt2mp/game/player_info.h"
#include "mt2mp/game/save_differ.h"
#include "mt2mp/overlay/notification_overlay.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"

#include <chrono>

namespace mt2mp::net {

NetManager& NetManager::instance() {
    static NetManager mgr;
    return mgr;
}

bool NetManager::init() {
    if (initialized_) return true;

    if (!SteamNetworking::instance().init()) {
        LOG_NET_ERROR("Failed to init SteamNetworking");
        return false;
    }

    SteamMatchmaking::instance().init(); // optional

    SteamNetworking::instance().set_callback(
        [this](uint64_t sender, const uint8_t* data, uint32_t size, int channel) {
            on_packet_received(sender, data, size, channel);
        });

    sync::SyncController::instance().init();

    initialized_ = true;
    LOG_NET_INFO("NetManager initialized");
    return true;
}

void NetManager::shutdown() {
    if (!initialized_) return;
    sync::SyncController::instance().shutdown();
    SteamMatchmaking::instance().shutdown();
    SteamNetworking::instance().shutdown();
    handlers_.clear();
    state_ = ConnectionState::Disconnected;
    initialized_ = false;
}

void NetManager::update(float dt) {
    // In Lobby Data mode, connection state is driven by lobby membership
    // No P2P handshake retries needed
    if (SteamMatchmaking::instance().in_lobby()) {
        if (state_ != ConnectionState::Active) {
            transition_to(ConnectionState::Active);
        }
    } else {
        if (state_ == ConnectionState::Active) {
            transition_to(ConnectionState::Disconnected);
        }
    }
}

void NetManager::register_handler(PacketType type, PacketHandler handler) {
    handlers_[type] = std::move(handler);
}

void NetManager::on_packet_received(uint64_t sender, const uint8_t* data, uint32_t size, int channel) {
    if (size < sizeof(PacketHeader)) return;

    // Auto-discover peers
    auto& net = SteamNetworking::instance();
    bool is_new = true;
    for (auto id : net.peers()) { if (id == sender) { is_new = false; break; } }

    if (sender != 0) {
        if (is_new) {
            net.add_peer(sender);
            LOG_NET_INFO("New peer discovered: {}", sender);
        }

        // Always ensure player is in registry (covers auto-discovered peers)
        if (!game::PlayerRegistry::instance().get(sender)) {
            game::PlayerInfo info;
            info.steam_id = sender;
            info.display_name = std::to_string(sender);
            info.connected = true;
            game::PlayerRegistry::instance().add(info);
            LOG_NET_INFO("Added player {} to registry from packet", sender);

            overlay::NotificationOverlay::instance().show(
                "Player connected!", overlay::NotificationType::Success, 5.0f);
        }

        // Advance connection state
        if (state_ == ConnectionState::Connecting || state_ == ConnectionState::Disconnected) {
            transition_to(ConnectionState::Handshaking);
        }
        if (state_ == ConnectionState::Handshaking) {
            // Any packet received means we're effectively connected
            transition_to(ConnectionState::Active);
        }
    }

    Packet packet = Packet::deserialize(data, size);
    LOG_NET_DEBUG("Packet from {}: type=0x{:02X} seq={} payload={} bytes",
        sender, static_cast<uint8_t>(packet.header.type),
        packet.header.sequence, packet.header.payload_length);

    auto it = handlers_.find(packet.header.type);
    if (it != handlers_.end()) {
        it->second(sender, packet);
    } else {
        LOG_NET_WARN("Unhandled packet type 0x{:02X} from {}", static_cast<uint8_t>(packet.header.type), sender);
    }
}

void NetManager::connect_to_peer(uint64_t steam_id) {
    // In Lobby Data mode, joining is done via JoinLobby, not direct P2P
    LOG_NET_INFO("connect_to_peer is deprecated in Lobby Data mode. Use JoinLobby instead.");
}

void NetManager::transition_to(ConnectionState new_state) {
    if (state_ == new_state) return;
    LOG_NET_INFO("Connection: {} -> {}", state_string(),
        [&]() -> const char* {
            auto old = state_; state_ = new_state; auto s = state_string(); state_ = old; return s;
        }());
    state_ = new_state;

    if (new_state == ConnectionState::Active) {
        sync::SyncController::instance().on_session_start(Config::instance().is_host);
    } else if (new_state == ConnectionState::Disconnected) {
        sync::SyncController::instance().on_session_end();
    }
}

const char* NetManager::state_string() const {
    switch (state_) {
        case ConnectionState::Disconnected: return "Disconnected";
        case ConnectionState::Connecting:   return "Connecting";
        case ConnectionState::Handshaking:  return "Handshaking";
        case ConnectionState::Syncing:      return "Syncing";
        case ConnectionState::Active:       return "Active";
        default: return "?";
    }
}

void NetManager::send_reliable(uint64_t target, const Packet& packet) {
    auto buf = packet.serialize();
    SteamNetworking::instance().send(target, buf.data(),
        static_cast<uint32_t>(buf.size()), NET_CHANNEL_RELIABLE, true);
}

void NetManager::send_unreliable(uint64_t target, const Packet& packet) {
    auto buf = packet.serialize();
    SteamNetworking::instance().send(target, buf.data(),
        static_cast<uint32_t>(buf.size()), NET_CHANNEL_UNRELIABLE, false);
}

void NetManager::broadcast_reliable(const Packet& packet) {
    auto buf = packet.serialize();
    SteamNetworking::instance().broadcast(buf.data(),
        static_cast<uint32_t>(buf.size()), NET_CHANNEL_RELIABLE, true);
}

void NetManager::broadcast_unreliable(const Packet& packet) {
    auto buf = packet.serialize();
    SteamNetworking::instance().broadcast(buf.data(),
        static_cast<uint32_t>(buf.size()), NET_CHANNEL_UNRELIABLE, false);
}

void NetManager::create_lobby(int max_players) {
    if (!initialized_ && !init()) return;

    SteamMatchmaking::instance().create_lobby(max_players, [](bool ok, uint64_t id) {
        if (ok) {
            LOG_NET_INFO("Lobby created: {}", id);
            Config::instance().is_host = true;
        } else {
            LOG_NET_ERROR("Failed to create lobby");
        }
    });
}

void NetManager::join_lobby(uint64_t lobby_id) {
    SteamMatchmaking::instance().join_lobby(lobby_id, [](bool ok, uint64_t id) {
        if (ok) {
            LOG_NET_INFO("Joined lobby: {}", id);
            Config::instance().is_host = false;
        } else {
            LOG_NET_ERROR("Failed to join lobby");
        }
    });
}

void NetManager::leave_lobby() {
    SteamMatchmaking::instance().leave_lobby();
    transition_to(ConnectionState::Disconnected);
    Config::instance().is_host = false;
}

bool NetManager::is_host() const { return Config::instance().is_host; }
bool NetManager::is_connected() const {
    return state_ == ConnectionState::Active || state_ == ConnectionState::Syncing ||
           !SteamNetworking::instance().peers().empty();
}

SteamNetworking& NetManager::networking() { return SteamNetworking::instance(); }
SteamMatchmaking& NetManager::matchmaking() { return SteamMatchmaking::instance(); }

} // namespace mt2mp::net

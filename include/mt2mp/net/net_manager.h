#pragma once

#include "mt2mp/net/steam_networking.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/packet_types.h"

#include <functional>
#include <unordered_map>
#include <string>

namespace mt2mp::net {

using PacketHandler = std::function<void(uint64_t sender, const Packet& packet)>;

enum class ConnectionState {
    Disconnected,
    Connecting,     // AcceptSession called, handshake sent
    Handshaking,    // Waiting for handshake response
    Syncing,        // Full state transfer in progress
    Active,         // Normal multiplayer operation
};

class NetManager {
public:
    static NetManager& instance();

    bool init();
    void shutdown();
    void update(float dt);

    // Register handler for specific packet type
    void register_handler(PacketType type, PacketHandler handler);

    // Send helpers
    void send_reliable(uint64_t target, const Packet& packet);
    void send_unreliable(uint64_t target, const Packet& packet);
    void broadcast_reliable(const Packet& packet);
    void broadcast_unreliable(const Packet& packet);

    // Lobby wrappers
    void create_lobby(int max_players);
    void join_lobby(uint64_t lobby_id);
    void leave_lobby();

    // Connection
    void connect_to_peer(uint64_t steam_id);
    ConnectionState connection_state() const { return state_; }
    const char* state_string() const;

    bool is_host() const;
    bool is_connected() const;

    SteamNetworking& networking();
    SteamMatchmaking& matchmaking();

private:
    NetManager() = default;
    void on_packet_received(uint64_t sender, const uint8_t* data, uint32_t size, int channel);
    void transition_to(ConnectionState new_state);

    std::unordered_map<PacketType, PacketHandler> handlers_;
    ConnectionState state_ = ConnectionState::Disconnected;
    bool initialized_ = false;
    float connect_timeout_ = 0.0f;
};

} // namespace mt2mp::net

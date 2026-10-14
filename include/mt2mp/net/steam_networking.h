#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <string>

namespace mt2mp::net {

// Callback for received packets
using PacketCallback = std::function<void(uint64_t sender_steam_id,
    const uint8_t* data, uint32_t size, int channel)>;

class SteamNetworking {
public:
    static SteamNetworking& instance();

    bool init();
    void shutdown();

    // Send data to a specific Steam user
    bool send(uint64_t target_steam_id, const uint8_t* data, uint32_t size,
              int channel, bool reliable);

    // Broadcast to all connected peers
    void broadcast(const uint8_t* data, uint32_t size, int channel, bool reliable);

    // Poll for incoming packets (call each frame)
    void poll();

    // Set packet receive callback
    void set_callback(PacketCallback cb);

    // Peer management
    void add_peer(uint64_t steam_id);
    void remove_peer(uint64_t steam_id);
    const std::vector<uint64_t>& peers() const;

    void accept_session(uint64_t steam_id); // no-op in lobby mode

    // Lobby Data Transport
    void set_matchmaking_iface(void* mm);
    void write_member_data(uint64_t lobby, const char* key, const char* value);
    const char* read_member_data(uint64_t lobby, uint64_t member, const char* key);
    int get_lobby_member_count(uint64_t lobby);
    uint64_t get_lobby_member_by_index(uint64_t lobby, int index);
    uint64_t get_lobby_owner(uint64_t lobby);
    std::string get_persona_name(uint64_t steam_id);

    // Get local Steam ID
    uint64_t local_steam_id() const;

    void* get_networking_ptr() const;

private:
    SteamNetworking() = default;
    PacketCallback callback_;
    std::vector<uint64_t> peers_;
    bool initialized_ = false;

    void* steam_networking_ = nullptr;
    void* steam_user_ = nullptr;
};

} // namespace mt2mp::net

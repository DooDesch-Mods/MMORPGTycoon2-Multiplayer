#pragma once

#include <cstdint>
#include <vector>
#include <functional>

namespace mt2mp::net {

// Steam P2P transport using ISteamNetworkingSockets.
// - Goes through Steam's relay network (no port forwarding needed)
// - Pure polling, no CCallbackBase (avoids the broken-in-injected-DLL issue)
// - Uses ISteamNetworkingUtils::SetGlobalCallback_SteamNetConnectionStatusChanged
//   which is a simple C function pointer (works in injected DLLs)
class SteamP2P {
public:
    static SteamP2P& instance();

    // Initialize: load interface pointers, init relay network access.
    bool init();

    // Per-frame tick: process incoming messages + connection callbacks
    void tick();

    // Host: start listening for incoming P2P connections
    bool start_listening();

    // Joiner: connect to a host's Steam ID
    bool connect_to(uint64_t host_steam_id);

    // Disconnect all
    void shutdown();

    // Send data to a specific peer (or all peers if steam_id == 0)
    bool send(uint64_t peer_steam_id, const uint8_t* data, uint32_t size, bool reliable = true);

    // Broadcast to all connected peers
    bool broadcast(const uint8_t* data, uint32_t size, bool reliable = true);

    // Set callback for incoming messages
    using ReceiveCallback = std::function<void(uint64_t sender_steam_id, const uint8_t* data, uint32_t size)>;
    void set_receive_callback(ReceiveCallback cb);

    bool is_connected() const;
    int peer_count() const;

private:
    SteamP2P() = default;
};

} // namespace mt2mp::net

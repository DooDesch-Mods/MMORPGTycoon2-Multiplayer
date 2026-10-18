#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace mt2mp::net {

struct LobbyInfo {
    uint64_t lobby_id;
    std::string host_name;
    std::string save_name;
    int player_count;
    int max_players;
    std::string state;
};

using LobbyCreatedCallback = std::function<void(bool success, uint64_t lobby_id)>;
using LobbyJoinedCallback = std::function<void(bool success, uint64_t lobby_id)>;
using LobbyListCallback = std::function<void(bool success, const std::vector<LobbyInfo>& lobbies)>;
using LobbyMemberCallback = std::function<void(uint64_t steam_id, bool joined)>;

class SteamMatchmaking {
public:
    static SteamMatchmaking& instance();

    bool init();
    void shutdown();

    // Create a friends-only lobby
    void create_lobby(int max_players, LobbyCreatedCallback cb, bool public_lobby=false);

    // Join an existing lobby
    void join_lobby(uint64_t lobby_id, LobbyJoinedCallback cb);

    // Leave current lobby
    void leave_lobby();

    // Request lobby list (filtered by our mod tag)
    void request_lobby_list(LobbyListCallback cb);

    // Set lobby metadata
    void set_lobby_data(const char* key, const char* value);
    std::string get_lobby_data(const char* key) const;

    bool publish_lobby_data(const char* key,const std::string& value,std::string& error);
    bool set_lobby_joinable(bool value,std::string& error);

    // Current state
    bool in_lobby() const;
    bool is_host() const;
    uint64_t current_lobby_id() const;
    uint64_t host_steam_id() const;
    std::vector<uint64_t> get_lobby_members() const;

    // Optional Steam presence uses the existing game-thread Steam session.
    void update_rich_presence(bool enabled, bool playing, bool preparing, bool unavailable, uint64_t expected_host);

    // Member change callback
    void set_member_callback(LobbyMemberCallback cb);

    // Poll Steam callbacks (call each frame)
    void poll();

    // Process queued lobby chat messages (call from main thread)
    void process_pending_chats();

private:
    SteamMatchmaking() = default;
    uint64_t lobby_id_ = 0;
    bool is_host_ = false;
    void* steam_matchmaking_ = nullptr;
    LobbyMemberCallback member_callback_;
};

} // namespace mt2mp::net

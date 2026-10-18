namespace {
struct PresenceApi {
    using Set = bool(*)(void*, const char*, const char*);
    using Get = const char*(*)(void*, uint64_t, const char*);
    void* friends = nullptr;
    Set setter = nullptr;
    Get getter = nullptr;
    uint64_t local = 0;
    bool bind(std::string& error) {
        if (friends) return true;
        auto access = load_fn<void*(*)()>("SteamAPI_SteamFriends_v018");
        setter = load_fn<Set>("SteamAPI_ISteamFriends_SetRichPresence");
        getter = load_fn<Get>("SteamAPI_ISteamFriends_GetFriendRichPresence");
        local = SteamNetworking::instance().local_steam_id();
        if (!local) {
            auto user_access = load_fn<void*(*)()>("SteamAPI_SteamUser_v023");
            auto get_id = load_fn<uint64_t(*)(void*)>("SteamAPI_ISteamUser_GetSteamID");
            void* user = user_access ? user_access() : nullptr;
            if (user && get_id) local = get_id(user);
        }
        if (!access || !setter || !getter || !local) { error = "Steam presence unavailable: existing SteamFriends flat API or local identity missing"; return false; }
        friends = access();
        if (!friends) { error = "Steam presence unavailable: SteamFriends accessor returned null"; return false; }
        return true;
    }
    bool get(const char* key, std::string& value, std::string& error) {
        const char* actual = getter(friends, local, key);
        return presence::copy_value(actual, key, value, error);
    }
    bool set(const char* key, const std::string& value, std::string& error) {
        if (!setter(friends, key, value.empty() ? nullptr : value.c_str())) { error = "Steam presence setter rejected key=" + std::string(key); return false; }
        return true;
    }
};
PresenceApi presence_api;
presence::OwnedKeys presence_keys;
std::chrono::steady_clock::time_point presence_next{};
std::string presence_error, presence_status;
bool presence_last_coop = false;

void presence_report(const std::string& error, const std::string& status) {
    if (!error.empty()) {
        if (presence_error != error) LOG_STEAM_WARN("Rich presence degraded: {}", error);
        presence_error = error; return;
    }
    if (!presence_error.empty()) { LOG_STEAM_INFO("Rich presence recovered"); presence_error.clear(); }
    if (presence_status != status) {
        LOG_STEAM_INFO("Rich presence {}", status.empty() ? "restored previous game state" : status);
        presence_status = status;
    }
}
}

static void restore_rich_presence() {
    std::string error;
    presence_keys.restore(presence_api, error);
    presence_report(error, {});
    presence_next = {};
    presence_last_coop = false;
}

void SteamMatchmaking::update_rich_presence(bool enabled, bool playing, bool preparing, bool unavailable, uint64_t expected_host) {
    // enabled gates only confirmed online details. The loaded mod marker also applies to solo and local tests.
    const bool coop = enabled && in_lobby() && !unavailable && expected_host;
    const auto now = std::chrono::steady_clock::now();
    // Clear departed co-op details immediately; unchanged state performs at most one pass every two seconds.
    if (now < presence_next && coop == presence_last_coop) return;
    presence_next = now + std::chrono::seconds(2);
    presence_last_coop = coop;
    std::string error;
    if (!presence_api.bind(error)) { presence_report(error, {}); return; }
    auto values = presence::generic();
    if (coop) {
        const auto members = get_lobby_members();
        const auto has_member = [&](uint64_t member) { return std::find(members.begin(), members.end(), member) != members.end(); };
        using Limit = int(*)(void*, uint64_t);
        const auto get_limit = load_fn<Limit>("SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit");
        if (get_limit && has_member(presence_api.local) && has_member(expected_host) && host_steam_id() == expected_host && get_lobby_data("mt2mp") == "1") {
            const int limit = get_limit(steam_matchmaking_, lobby_id_);
            if (limit > 0) values = presence::describe(lobby_id_, static_cast<unsigned>(members.size()), static_cast<unsigned>(limit), playing, preparing);
        }
    }
    // The local Steam readback may already contain localized display text. Keep its native template intact.
    std::string display;
    if (!presence_api.get("steam_display", display, error)) {
        std::string cleanup;
        if (!presence_keys.restore(presence_api, cleanup)) error += "; restore failed: " + cleanup;
        presence_report(error, {}); return;
    }
    if (!presence_keys.apply(values, presence_api, error)) {
        std::string cleanup;
        if (!presence_keys.restore(presence_api, cleanup)) error += "; restore failed: " + cleanup;
        presence_report(error, {}); return;
    }
    std::string subscribers;
    if (!presence_api.get("subscribercount", subscribers, error)) { presence_report(error, {}); return; }
    presence_report({}, values[0] + " display=" + display + " subscribers=" + subscribers);
}

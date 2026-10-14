// Steam Lobby Data Transport
// Uses SetLobbyData/GetLobbyData + SetLobbyMemberData/GetLobbyMemberData
// NO P2P sessions, NO callbacks needed. Pure polling.

#include "mt2mp/net/steam_networking.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"

#include <windows.h>
#include <cstring>
#include <cstdio>
#include <string>

// Flat C API types
using FnGetSteamID = uint64_t(*)(void* self);
using FnSteamUser = void*(*)();
using FnSteamFriends = void*(*)();
using FnGetPersonaName = const char*(*)(void* self, uint64_t steamID);

// Lobby member data
using FnSetLobbyMemberData = void(*)(void* self, uint64_t lobby, const char* key, const char* value);
using FnGetLobbyMemberData = const char*(*)(void* self, uint64_t lobby, uint64_t member, const char* key);
using FnGetNumLobbyMembers = int(*)(void* self, uint64_t lobby);
using FnGetLobbyMemberByIndex = uint64_t(*)(void* self, uint64_t lobby, int index);
using FnGetLobbyOwner = uint64_t(*)(void* self, uint64_t lobby);

namespace mt2mp::net {

static HMODULE g_steam_dll = nullptr;

// Function pointers
static FnGetSteamID fn_GetSteamID = nullptr;
static FnGetPersonaName fn_GetPersonaName = nullptr;
static FnSetLobbyMemberData fn_SetMemberData = nullptr;
static FnGetLobbyMemberData fn_GetMemberData = nullptr;
static FnGetNumLobbyMembers fn_GetNumMembers = nullptr;
static FnGetLobbyMemberByIndex fn_GetMemberByIndex = nullptr;
static FnGetLobbyOwner fn_GetLobbyOwner = nullptr;

static void* g_user_iface = nullptr;
static void* g_friends_iface = nullptr;
static void* g_mm_iface = nullptr; // matchmaking, set by SteamMatchmaking

static HMODULE get_steam() {
    if (g_steam_dll) return g_steam_dll;
    g_steam_dll = GetModuleHandleA("steam_api64_original.dll");
    if (!g_steam_dll) g_steam_dll = LoadLibraryA(Config::instance().original_steam_dll.c_str());
    return g_steam_dll;
}

template<typename T>
static T load_fn(const char* name) {
    return reinterpret_cast<T>(GetProcAddress(get_steam(), name));
}

SteamNetworking& SteamNetworking::instance() {
    static SteamNetworking inst;
    return inst;
}

bool SteamNetworking::init() {
    if (initialized_) return true;
    if (!get_steam()) { LOG_STEAM_ERROR("Cannot find steam_api64_original.dll"); return false; }

    // Get ISteamUser
    auto fn_user_acc = load_fn<FnSteamUser>("SteamAPI_SteamUser_v023");
    if (!fn_user_acc) fn_user_acc = load_fn<FnSteamUser>("SteamUser");
    if (fn_user_acc) g_user_iface = fn_user_acc();
    steam_user_ = g_user_iface;

    // Get ISteamFriends
    using FindFn = void*(*)(int32_t, const char*);
    using GetHUserFn = int32_t(*)();
    auto find_fn = load_fn<FindFn>("SteamInternal_FindOrCreateUserInterface");
    auto get_huser = load_fn<GetHUserFn>("SteamAPI_GetHSteamUser");
    if (find_fn && get_huser) {
        int32_t huser = get_huser();
        g_friends_iface = find_fn(huser, "SteamFriends018");
        if (!g_friends_iface) g_friends_iface = find_fn(huser, "SteamFriends017");
    }

    // Load functions
    fn_GetSteamID = load_fn<FnGetSteamID>("SteamAPI_ISteamUser_GetSteamID");
    fn_GetPersonaName = load_fn<FnGetPersonaName>("SteamAPI_ISteamFriends_GetFriendPersonaName");
    fn_SetMemberData = load_fn<FnSetLobbyMemberData>("SteamAPI_ISteamMatchmaking_SetLobbyMemberData");
    fn_GetMemberData = load_fn<FnGetLobbyMemberData>("SteamAPI_ISteamMatchmaking_GetLobbyMemberData");
    fn_GetNumMembers = load_fn<FnGetNumLobbyMembers>("SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
    fn_GetMemberByIndex = load_fn<FnGetLobbyMemberByIndex>("SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
    fn_GetLobbyOwner = load_fn<FnGetLobbyOwner>("SteamAPI_ISteamMatchmaking_GetLobbyOwner");

    if (!fn_GetSteamID) { LOG_STEAM_ERROR("Missing GetSteamID"); return false; }

    initialized_ = true;
    steam_networking_ = g_mm_iface; // store for compatibility
    LOG_STEAM_INFO("Lobby Data Transport initialized, local ID: {}", local_steam_id());
    return true;
}

void SteamNetworking::shutdown() {
    peers_.clear();
    initialized_ = false;
}

// Set the matchmaking interface (called from SteamMatchmaking::init)
void SteamNetworking::set_matchmaking_iface(void* mm) {
    g_mm_iface = mm;
}

// ── Lobby Data operations ────────────────────────────────────

void SteamNetworking::write_member_data(uint64_t lobby, const char* key, const char* value) {
    if (!fn_SetMemberData || !g_mm_iface) return;
    fn_SetMemberData(g_mm_iface, lobby, key, value);
}

const char* SteamNetworking::read_member_data(uint64_t lobby, uint64_t member, const char* key) {
    if (!fn_GetMemberData || !g_mm_iface) return "";
    const char* val = fn_GetMemberData(g_mm_iface, lobby, member, key);
    return val ? val : "";
}

int SteamNetworking::get_lobby_member_count(uint64_t lobby) {
    if (!fn_GetNumMembers || !g_mm_iface) return 0;
    return fn_GetNumMembers(g_mm_iface, lobby);
}

uint64_t SteamNetworking::get_lobby_member_by_index(uint64_t lobby, int index) {
    if (!fn_GetMemberByIndex || !g_mm_iface) return 0;
    return fn_GetMemberByIndex(g_mm_iface, lobby, index);
}

uint64_t SteamNetworking::get_lobby_owner(uint64_t lobby) {
    if (!fn_GetLobbyOwner || !g_mm_iface) return 0;
    return fn_GetLobbyOwner(g_mm_iface, lobby);
}

std::string SteamNetworking::get_persona_name(uint64_t steam_id) {
    if (!fn_GetPersonaName || !g_friends_iface) return std::to_string(steam_id);
    const char* name = fn_GetPersonaName(g_friends_iface, steam_id);
    return (name && name[0]) ? name : std::to_string(steam_id);
}

// ── Legacy interface (kept for compatibility) ────────────────

bool SteamNetworking::send(uint64_t, const uint8_t*, uint32_t, int, bool) {
    // Not used in Lobby Data mode
    return false;
}

void SteamNetworking::broadcast(const uint8_t*, uint32_t, int, bool) {}

void SteamNetworking::poll() {
    // Not used - lobby data is polled directly in MT2MP_Tick
}

void SteamNetworking::set_callback(PacketCallback cb) { callback_ = std::move(cb); }
void SteamNetworking::accept_session(uint64_t) {}
void SteamNetworking::add_peer(uint64_t steam_id) {
    if (std::find(peers_.begin(), peers_.end(), steam_id) == peers_.end()) {
        peers_.push_back(steam_id);
    }
}
void SteamNetworking::remove_peer(uint64_t steam_id) {
    peers_.erase(std::remove(peers_.begin(), peers_.end(), steam_id), peers_.end());
}
const std::vector<uint64_t>& SteamNetworking::peers() const { return peers_; }

uint64_t SteamNetworking::local_steam_id() const {
    if (!fn_GetSteamID || !g_user_iface) return 0;
    return fn_GetSteamID(g_user_iface);
}

void* SteamNetworking::get_networking_ptr() const { return steam_networking_; }

} // namespace mt2mp::net

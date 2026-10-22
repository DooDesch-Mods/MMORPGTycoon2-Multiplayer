#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/steam_module.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/net/save_transfer.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"

#include <windows.h>
#include <chrono>
#include <algorithm>
#include <atomic>
#include "mt2mp/net/steam_presence.h"

// Flat C API types
using FnCreateLobby = uint64_t(*)(void* self, int eLobbyType, int cMaxMembers);
using FnJoinLobby = uint64_t(*)(void* self, uint64_t steamIDLobby);
using FnLeaveLobby = void(*)(void* self, uint64_t steamIDLobby);
using FnGetLobbyOwner = uint64_t(*)(void* self, uint64_t steamIDLobby);
using FnSetLobbyData = bool(*)(void* self, uint64_t steamIDLobby, const char* k, const char* v);
using FnGetLobbyData = const char*(*)(void* self, uint64_t steamIDLobby, const char* k);
using FnRequestLobbyList = uint64_t(*)(void* self);
using FnAddStringFilter = void(*)(void* self, const char* k, const char* v, int cmp);
using FnAddDistanceFilter = void(*)(void* self, int distance);
using FnGetLobbyByIndex = uint64_t(*)(void* self, int iLobby);
using FnMatchmakingAccessor = void*(*)();

// SteamUtils for polling async call results
using FnSteamUtilsAccessor = void*(*)();
using FnIsAPICallCompleted = bool(*)(void* self, uint64_t hCall, bool* pbFailed);
using FnGetAPICallResult = bool(*)(void* self, uint64_t hCall, void* pCb, int cubCb, int iExpected, bool* pbFailed);

#pragma pack(push, 8)
struct LobbyCreated_t {
    uint32_t m_eResult;
    uint32_t m_pad;
    uint64_t m_ulSteamIDLobby;
};
#pragma pack(pop)
constexpr int k_LobbyCreated = 513;
constexpr uint32_t k_EResultOK = 1;

// Forward declarations for lobby invite callback (defined before poll())
static void __cdecl cb_run_lobby_join(void* thisptr, void* pvParam);
static void __cdecl cb_run_lobby_join_result(void* thisptr, void* pvParam, bool bIOFailure, uint64_t hCall);
static int  __cdecl cb_get_size_lobby_join(void* thisptr);

constexpr int k_GameLobbyJoinRequested = 333;

static void* g_lobby_join_vtable_storage[5] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&cb_run_lobby_join_result),
    reinterpret_cast<void*>(&cb_run_lobby_join),
    reinterpret_cast<void*>(&cb_get_size_lobby_join),
};
static void** g_lobby_join_vtable = &g_lobby_join_vtable_storage[2];

struct alignas(8) LobbyJoinCallbackObj {
    void** vfptr;
    uint8_t m_nCallbackFlags;
    uint8_t _pad[3];
    int32_t m_iCallback;
};
static LobbyJoinCallbackObj g_lobby_join_cb;
static bool g_lobby_join_cb_registered = false;
static std::atomic<uint64_t> g_pending_invite_lobby{0};
static std::chrono::steady_clock::time_point g_next_invite_init{};

// ── LobbyChatMsg_t Callback (ID 507) ─────────────────────────
constexpr int k_LobbyChatMsg = 507;

static void __cdecl cb_run_lobby_chat(void* thisptr, void* pvParam);
static void __cdecl cb_run_lobby_chat_result(void* thisptr, void* pvParam, bool bIOFailure, uint64_t hCall);
static int  __cdecl cb_get_size_lobby_chat(void* thisptr);

static void* g_lobby_chat_vtable_storage[5] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&cb_run_lobby_chat_result),
    reinterpret_cast<void*>(&cb_run_lobby_chat),
    reinterpret_cast<void*>(&cb_get_size_lobby_chat),
};
static void** g_lobby_chat_vtable = &g_lobby_chat_vtable_storage[2];
static LobbyJoinCallbackObj g_lobby_chat_cb;
static bool g_lobby_chat_cb_registered = false;

// Queue of pending chat messages (lobby_id, chat_id) to process in main thread
#include <vector>
#include <mutex>
static std::mutex g_chat_mutex;
static std::vector<std::pair<uint64_t, int>> g_pending_chats;

#pragma pack(push, 8)
struct LobbyChatMsg_t {
    uint64_t m_ulSteamIDLobby;
    uint64_t m_ulSteamIDUser;
    uint8_t m_eChatEntryType;
    uint32_t m_iChatID;
};
#pragma pack(pop)

#pragma pack(push, 8)
struct GameLobbyJoinRequested_t {
    uint64_t m_steamIDLobby;
    uint64_t m_steamIDFriend;
};
#pragma pack(pop)

// ── LobbyChatMsg callback implementations ────────────────────
static void __cdecl cb_run_lobby_chat(void* thisptr, void* pvParam) {
    auto* ev = reinterpret_cast<LobbyChatMsg_t*>(pvParam);
    if (ev) {
        // Forward to SaveTransfer for chunk processing
        FILE* f = fopen("mt2mp_chat_debug.log", "a");
        if (f) { fprintf(f, "LobbyChatMsg! lobby=%llu user=%llu chatID=%u\n",
            ev->m_ulSteamIDLobby, ev->m_ulSteamIDUser, ev->m_iChatID); fclose(f); }

        // Will be processed by SaveTransfer::on_chat_message via poll
        // Queue for main thread processing
        std::lock_guard<std::mutex> lock(g_chat_mutex);
        g_pending_chats.push_back({ev->m_ulSteamIDLobby, static_cast<int>(ev->m_iChatID)});
    }
}

static void __cdecl cb_run_lobby_chat_result(void* thisptr, void* pvParam, bool bIOFailure, uint64_t hCall) {}

static int __cdecl cb_get_size_lobby_chat(void* thisptr) {
    return sizeof(LobbyChatMsg_t);
}

// ── GameLobbyJoinRequested callback implementations ──────────
static void __cdecl cb_run_lobby_join(void* thisptr, void* pvParam) {
    auto* ev = reinterpret_cast<GameLobbyJoinRequested_t*>(pvParam);
    if (ev && ev->m_steamIDLobby != 0) {
        g_pending_invite_lobby = ev->m_steamIDLobby;
        LOG_STEAM_INFO("GameLobbyJoinRequested received lobby={}", ev->m_steamIDLobby);
    } else LOG_STEAM_ERROR("GameLobbyJoinRequested rejected: missing event or zero lobby ID");
}

static void __cdecl cb_run_lobby_join_result(void* thisptr, void* pvParam, bool bIOFailure, uint64_t hCall) {
}

static int __cdecl cb_get_size_lobby_join(void* thisptr) {
    return sizeof(GameLobbyJoinRequested_t);
}

namespace mt2mp::net {

static void* g_mm_iface = nullptr;
static void* g_utils_iface = nullptr;

static FnCreateLobby fn_CreateLobby = nullptr;
static FnJoinLobby fn_JoinLobby = nullptr;
static FnLeaveLobby fn_LeaveLobby = nullptr;
static FnGetLobbyOwner fn_GetLobbyOwner = nullptr;
static FnSetLobbyData fn_SetLobbyData = nullptr;
static FnGetLobbyData fn_GetLobbyData = nullptr;
static FnRequestLobbyList fn_RequestLobbyList = nullptr;
static FnAddStringFilter fn_AddStringFilter = nullptr;
static FnAddDistanceFilter fn_AddDistanceFilter = nullptr;
static FnGetLobbyByIndex fn_GetLobbyByIndex = nullptr;
static FnIsAPICallCompleted fn_IsCallCompleted = nullptr;
static FnGetAPICallResult fn_GetCallResult = nullptr;

// Pending async lobby create
static uint64_t g_pending_create_call = 0;
static LobbyCreatedCallback g_pending_create_cb;
static LobbyJoinedCallback g_pending_join_cb;
static LobbyListCallback g_pending_list_cb;
static uint64_t g_pending_list_call = 0;
static std::chrono::steady_clock::time_point g_create_started, g_join_started, g_list_started;
static bool g_cancel_create = false, g_cancel_join = false;

static HMODULE get_steam() { return steam_module(); }

template<typename T>
static T load_fn(const char* name) {
    return reinterpret_cast<T>(GetProcAddress(get_steam(), name));
}

static void restore_rich_presence();

SteamMatchmaking& SteamMatchmaking::instance() {
    static SteamMatchmaking inst;
    return inst;
}

bool SteamMatchmaking::init() {
    if (steam_matchmaking_) return true;
    if (!get_steam()) return false;

    // Get matchmaking interface
    auto accessor = load_fn<FnMatchmakingAccessor>("SteamAPI_SteamMatchmaking_v009");
    if (!accessor) accessor = load_fn<FnMatchmakingAccessor>("SteamMatchmaking");
    if (!accessor) {
        using FindFn = void*(*)(int32_t, const char*);
        using GetHUserFn = int32_t(*)();
        auto find_fn = load_fn<FindFn>("SteamInternal_FindOrCreateUserInterface");
        auto get_huser = load_fn<GetHUserFn>("SteamAPI_GetHSteamUser");
        if (find_fn && get_huser)
            steam_matchmaking_ = find_fn(get_huser(), "SteamMatchMaking009");
    } else {
        steam_matchmaking_ = accessor();
    }

    if (!steam_matchmaking_) { LOG_STEAM_WARN("ISteamMatchmaking not available"); return false; }

    // Get SteamUtils for async call result polling
    auto utils_acc = load_fn<FnSteamUtilsAccessor>("SteamAPI_SteamUtils_v010");
    if (!utils_acc) utils_acc = load_fn<FnSteamUtilsAccessor>("SteamUtils");
    if (utils_acc) g_utils_iface = utils_acc();

    // Load all functions
    fn_CreateLobby = load_fn<FnCreateLobby>("SteamAPI_ISteamMatchmaking_CreateLobby");
    fn_JoinLobby = load_fn<FnJoinLobby>("SteamAPI_ISteamMatchmaking_JoinLobby");
    fn_LeaveLobby = load_fn<FnLeaveLobby>("SteamAPI_ISteamMatchmaking_LeaveLobby");
    fn_GetLobbyOwner = load_fn<FnGetLobbyOwner>("SteamAPI_ISteamMatchmaking_GetLobbyOwner");
    fn_SetLobbyData = load_fn<FnSetLobbyData>("SteamAPI_ISteamMatchmaking_SetLobbyData");
    fn_GetLobbyData = load_fn<FnGetLobbyData>("SteamAPI_ISteamMatchmaking_GetLobbyData");
    fn_RequestLobbyList = load_fn<FnRequestLobbyList>("SteamAPI_ISteamMatchmaking_RequestLobbyList");
    fn_AddStringFilter = load_fn<FnAddStringFilter>("SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter");
    fn_AddDistanceFilter = load_fn<FnAddDistanceFilter>("SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter");
    fn_GetLobbyByIndex = load_fn<FnGetLobbyByIndex>("SteamAPI_ISteamMatchmaking_GetLobbyByIndex");
    fn_IsCallCompleted = load_fn<FnIsAPICallCompleted>("SteamAPI_ISteamUtils_IsAPICallCompleted");
    fn_GetCallResult = load_fn<FnGetAPICallResult>("SteamAPI_ISteamUtils_GetAPICallResult");

    if (!fn_CreateLobby || !fn_JoinLobby || !fn_LeaveLobby || !fn_RequestLobbyList ||
        !fn_AddStringFilter || !fn_AddDistanceFilter || !fn_GetLobbyByIndex || !fn_GetLobbyData ||
        !g_utils_iface || !fn_IsCallCompleted || !fn_GetCallResult) {
        LOG_STEAM_ERROR("Lobby operations unavailable: required async Steam API missing");
        steam_matchmaking_ = nullptr;
        return false;
    }

    // Share the MM interface with SteamNetworking for Lobby Data operations
    SteamNetworking::instance().set_matchmaking_iface(steam_matchmaking_);

    // Register GameLobbyJoinRequested_t callback for Steam invite handling
    if (!g_lobby_join_cb_registered) {
        using FnRegisterCb = void(*)(void*, int);
        auto fn_register = reinterpret_cast<FnRegisterCb>(
            GetProcAddress(get_steam(), "SteamAPI_RegisterCallback"));
        if (!fn_register) {
            LOG_STEAM_ERROR("Lobby invite handling unavailable: SteamAPI_RegisterCallback export missing");
            steam_matchmaking_ = nullptr;
            return false;
        }
        if (fn_register) {
            g_lobby_join_cb.vfptr = g_lobby_join_vtable;
            g_lobby_join_cb.m_nCallbackFlags = 0;
            memset(g_lobby_join_cb._pad, 0, sizeof(g_lobby_join_cb._pad));
            g_lobby_join_cb.m_iCallback = k_GameLobbyJoinRequested;
            fn_register(&g_lobby_join_cb, k_GameLobbyJoinRequested);
            g_lobby_join_cb_registered = true;
            LOG_STEAM_INFO("Registered GameLobbyJoinRequested_t callback (ID {})", k_GameLobbyJoinRequested);
        }

        // Register LobbyChatMsg_t callback for save transfer
        if (fn_register && !g_lobby_chat_cb_registered) {
            g_lobby_chat_cb.vfptr = g_lobby_chat_vtable;
            g_lobby_chat_cb.m_nCallbackFlags = 0;
            memset(g_lobby_chat_cb._pad, 0, sizeof(g_lobby_chat_cb._pad));
            g_lobby_chat_cb.m_iCallback = k_LobbyChatMsg;
            fn_register(&g_lobby_chat_cb, k_LobbyChatMsg);
            g_lobby_chat_cb_registered = true;
            LOG_STEAM_INFO("Registered LobbyChatMsg_t callback (ID {})", k_LobbyChatMsg);
        }
    }

    LOG_STEAM_INFO("SteamMatchmaking loaded (utils={}, async={})",
        g_utils_iface ? "yes" : "no",
        (fn_IsCallCompleted && fn_GetCallResult) ? "yes" : "no");
    return true;
}

void SteamMatchmaking::shutdown() {
    if (in_lobby()) leave_lobby();
    restore_rich_presence();
    steam_matchmaking_ = nullptr;
}

void SteamMatchmaking::create_lobby(int max_players, LobbyCreatedCallback cb, bool public_lobby) {
    if (!steam_matchmaking_ || !fn_CreateLobby) {
        if (cb) cb(false, 0);
        return;
    }
    if (g_pending_create_call != 0) {
        LOG_STEAM_WARN("CreateLobby already pending");
        if (cb) cb(false, 0);
        return;
    }

    LOG_STEAM_INFO("Creating lobby public={} max={}...", public_lobby,max_players);
    uint64_t call = fn_CreateLobby(steam_matchmaking_,public_lobby?2:1,max_players);
    LOG_STEAM_INFO("CreateLobby call handle: {}", call);

    if (call == 0) {
        if (cb) cb(false, 0);
        return;
    }

    g_pending_create_call = call;
    g_pending_create_cb = std::move(cb);
    g_create_started = std::chrono::steady_clock::now();
    g_cancel_create = false;
}

// Get lobby members for auto-accept
using FnGetNumMembers = int(*)(void*, uint64_t);
using FnGetMemberByIdx = uint64_t(*)(void*, uint64_t, int);

static FnGetNumMembers fn_GetNumMembers_cached = nullptr;
static FnGetMemberByIdx fn_GetMemberByIdx_cached = nullptr;

std::vector<uint64_t> SteamMatchmaking::get_lobby_members() const {
    std::vector<uint64_t> members;
    if (!steam_matchmaking_ || !in_lobby()) return members;

    if (!fn_GetNumMembers_cached) {
        fn_GetNumMembers_cached = reinterpret_cast<FnGetNumMembers>(
            GetProcAddress(get_steam(), "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers"));
    }
    if (!fn_GetMemberByIdx_cached) {
        fn_GetMemberByIdx_cached = reinterpret_cast<FnGetMemberByIdx>(
            GetProcAddress(get_steam(), "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex"));
    }

    if (fn_GetNumMembers_cached && fn_GetMemberByIdx_cached) {
        int count = fn_GetNumMembers_cached(steam_matchmaking_, lobby_id_);
        for (int i = 0; i < count; ++i) {
            uint64_t member = fn_GetMemberByIdx_cached(steam_matchmaking_, lobby_id_, i);
            if (member != 0) members.push_back(member);
        }
    }
    return members;
}

// Pending join state
static uint64_t g_pending_join_call = 0;
static uint64_t g_pending_join_lobby_id = 0;

#pragma pack(push, 8)
struct LobbyEnter_t {
    uint64_t m_ulSteamIDLobby;
    uint32_t m_rgfChatPermissions;
    uint32_t m_bLocked;
    uint32_t m_EChatRoomEnterResponse; // 1 = success
};
#pragma pack(pop)
constexpr int k_LobbyEnter = 504;
struct LobbyMatchList_t { uint32_t m_nLobbiesMatching; };
constexpr int k_LobbyMatchList = 510;

// Callback implementations are at file scope (before namespace)

void SteamMatchmaking::poll() {
    // Register before the first accepted invite, even with the Multiplayer panel closed.
    // Local tests do not bind a handler and must not start an online session here.
    if (invite_handler_ && !steam_matchmaking_) {
        const auto now = std::chrono::steady_clock::now();
        if (now < g_next_invite_init) return;
        g_next_invite_init = now + std::chrono::seconds(2);
        if (!init()) return;
    }
    if (invite_handler_) {
        const auto invite_lobby = g_pending_invite_lobby.exchange(0);
        if (invite_lobby) {
            LOG_STEAM_INFO("Steam invite received for lobby {}; routing through native admission", invite_lobby);
            invite_handler_(invite_lobby);
        }
    }

    // Poll async lobby CREATION result
    if (g_pending_create_call != 0 && g_utils_iface && fn_IsCallCompleted && fn_GetCallResult) {
        bool call_failed = false;
        if (fn_IsCallCompleted(g_utils_iface, g_pending_create_call, &call_failed)) {
            LobbyCreated_t result{};
            bool io_failed = false;
            bool got = fn_GetCallResult(g_utils_iface, g_pending_create_call,
                &result, sizeof(result), k_LobbyCreated, &io_failed);

            auto cb = std::move(g_pending_create_cb);
            g_pending_create_call = 0;
            g_pending_create_cb = {};

            if (got && !call_failed && !io_failed && result.m_eResult == k_EResultOK && result.m_ulSteamIDLobby != 0) {
                if (g_cancel_create) {
                    fn_LeaveLobby(steam_matchmaking_, result.m_ulSteamIDLobby);
                    LOG_STEAM_INFO("Cancelled lobby creation completed; leave requested for {}", result.m_ulSteamIDLobby);
                    if (cb) cb(false, 0);
                    return;
                }
                lobby_id_ = result.m_ulSteamIDLobby;
                is_host_ = true;
                set_lobby_data("mt2mp", "1");
                LOG_STEAM_INFO("Lobby CREATED: {} (Steam confirmed)", lobby_id_);
                if (cb) cb(true, lobby_id_);
            } else {
                LOG_STEAM_ERROR("Lobby creation FAILED (result={}, io_failed={}, call_failed={})",
                    result.m_eResult, io_failed, call_failed);
                if (cb) cb(false, 0);
            }
        }
    }

    if (g_pending_list_call && g_utils_iface && fn_IsCallCompleted && fn_GetCallResult) {
        bool failed = false;
        if (fn_IsCallCompleted(g_utils_iface, g_pending_list_call, &failed)) {
            LobbyMatchList_t result{};
            bool io_failed = false;
            bool got = fn_GetCallResult(g_utils_iface, g_pending_list_call, &result, sizeof(result), k_LobbyMatchList, &io_failed);
            auto cb = std::move(g_pending_list_cb);
            g_pending_list_call = 0;
            std::vector<LobbyInfo> lobbies;
            if (got && !failed && !io_failed && fn_GetLobbyByIndex && fn_GetLobbyData) {
                using FnGetLimit = int(*)(void*, uint64_t);
                auto limit = load_fn<FnGetLimit>("SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit");
                auto members = load_fn<FnGetNumMembers>("SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
                for (uint32_t i = 0; i < result.m_nLobbiesMatching && i < 50; ++i) {
                    uint64_t id = fn_GetLobbyByIndex(steam_matchmaking_, static_cast<int>(i));
                    if (!id) continue;
                    LobbyInfo info{}; info.lobby_id = id;
                    auto name = fn_GetLobbyData(steam_matchmaking_, id, "host_name");
                    info.host_name = name ? name : "";
                    auto save = fn_GetLobbyData(steam_matchmaking_, id, "game_name");
                    info.save_name = save ? save : "";
                    info.player_count = members ? members(steam_matchmaking_, id) : 0;
                    info.max_players = limit ? limit(steam_matchmaking_, id) : 0;
                    auto state=fn_GetLobbyData(steam_matchmaking_,id,"lobby_state");info.state=state?state:"";
                    if(!members||!limit||info.player_count<1||info.max_players<info.player_count||info.max_players>4)info.state="unavailable";
                    if(info.state!="open"&&info.state!="preparing"&&info.state!="playing"&&info.state!="leaving")info.state="unavailable";
                    lobbies.push_back(std::move(info));
                }
            } else LOG_STEAM_ERROR("Lobby search failed: got={}, call={}, io={}", got, failed, io_failed);
            if (got && !failed && !io_failed) LOG_STEAM_INFO("Public lobby search completed matches={} usable={}", result.m_nLobbiesMatching, lobbies.size());
            if (cb) cb(got && !failed && !io_failed && fn_GetLobbyByIndex && fn_GetLobbyData, lobbies);
        }
    }

    // Timed-out create/join calls stay pending until their late response can be cleaned up.
    auto now = std::chrono::steady_clock::now();
    if (g_pending_create_call && now - g_create_started > std::chrono::seconds(20) && !g_cancel_create) {
        g_cancel_create = true;
        LOG_STEAM_ERROR("Lobby creation timed out after 20 seconds; awaiting late result cleanup");
        auto cb = std::move(g_pending_create_cb);
        if (cb) cb(false, 0);
    }
    if (g_pending_join_call && now - g_join_started > std::chrono::seconds(20) && !g_cancel_join) {
        g_cancel_join = true;
        LOG_STEAM_ERROR("Lobby join timed out after 20 seconds; awaiting late result cleanup");
        auto cb = std::move(g_pending_join_cb);
        if (cb) cb(false, 0);
    }
    if (g_pending_list_call && now - g_list_started > std::chrono::seconds(20)) {
        g_pending_list_call = 0;
        LOG_STEAM_ERROR("Lobby search timed out after 20 seconds");
        auto cb = std::move(g_pending_list_cb);
        if (cb) cb(false, {});
    }

    // Poll async lobby JOIN result
    if (g_pending_join_call != 0 && g_utils_iface && fn_IsCallCompleted && fn_GetCallResult) {
        bool call_failed = false;
        if (fn_IsCallCompleted(g_utils_iface, g_pending_join_call, &call_failed)) {
            LobbyEnter_t result{};
            bool io_failed = false;
            bool got = fn_GetCallResult(g_utils_iface, g_pending_join_call,
                &result, sizeof(result), k_LobbyEnter, &io_failed);

            auto cb = std::move(g_pending_join_cb);
            g_pending_join_call = 0;
            g_pending_join_cb = {};

            if (got && !call_failed && !io_failed && result.m_EChatRoomEnterResponse == 1 &&
                result.m_ulSteamIDLobby == g_pending_join_lobby_id && result.m_ulSteamIDLobby != 0) {
                if (g_cancel_join) {
                    fn_LeaveLobby(steam_matchmaking_, result.m_ulSteamIDLobby);
                    LOG_STEAM_INFO("Cancelled lobby join completed; leave requested for {}", result.m_ulSteamIDLobby);
                    if (cb) cb(false, 0);
                    return;
                }
                lobby_id_ = result.m_ulSteamIDLobby;
                is_host_ = false;
                LOG_STEAM_INFO("Lobby JOINED: {} (Steam confirmed)", lobby_id_);
                if (cb) cb(true, lobby_id_);
            } else {
                LOG_STEAM_ERROR("Lobby join FAILED (response={}, io={}, call={})",
                    result.m_EChatRoomEnterResponse, io_failed, call_failed);
                if (cb) cb(false, 0);
            }
        }
    }
}

void SteamMatchmaking::join_lobby(uint64_t lobby_id, LobbyJoinedCallback cb) {
    if (!steam_matchmaking_ || !fn_JoinLobby || lobby_id == 0 || in_lobby()) {
        LOG_STEAM_ERROR("JoinLobby rejected: invalid target or unavailable Steam lobby interface");
        if (cb) cb(false, 0);
        return;
    }
    if (g_pending_join_call != 0) {
        LOG_STEAM_WARN("JoinLobby already pending");
        if (cb) cb(false, 0);
        return;
    }
    LOG_STEAM_INFO("Joining lobby {}...", lobby_id);
    uint64_t call = fn_JoinLobby(steam_matchmaking_, lobby_id);
    if (call != 0) {
        g_pending_join_call = call;
        g_pending_join_lobby_id = lobby_id;
        g_pending_join_cb = std::move(cb);
        g_join_started = std::chrono::steady_clock::now();
        g_cancel_join = false;
        LOG_STEAM_INFO("JoinLobby call handle: {}", call);
    } else {
        LOG_STEAM_ERROR("JoinLobby returned null handle");
        if (cb) cb(false, 0);
    }
}

void SteamMatchmaking::leave_lobby() {
    update_rich_presence(false, false, false, true, 0);
    g_cancel_create = true;
    g_cancel_join = true;
    if (!steam_matchmaking_ || !fn_LeaveLobby || !in_lobby()) return;
    LOG_STEAM_INFO("Leaving lobby {}", lobby_id_);
    fn_LeaveLobby(steam_matchmaking_, lobby_id_);
    lobby_id_ = 0;
    is_host_ = false;
}

void SteamMatchmaking::request_lobby_list(LobbyListCallback cb) {
    if (!steam_matchmaking_ || !fn_RequestLobbyList || !fn_AddStringFilter || !fn_AddDistanceFilter || g_pending_list_call != 0) {
        LOG_STEAM_ERROR("Lobby search rejected: unavailable Steam interface or pending request");
        if (cb) cb(false, {});
        return;
    }
    fn_AddDistanceFilter(steam_matchmaking_, 3); // Steam worldwide search for the small co-op lobby pool.
    fn_AddStringFilter(steam_matchmaking_,"mt2mp","1",0);
    fn_AddStringFilter(steam_matchmaking_,"mt2mp_version",MOD_VERSION,0);
    g_pending_list_call = fn_RequestLobbyList(steam_matchmaking_);
    if (!g_pending_list_call) {
        LOG_STEAM_ERROR("Lobby search returned null handle");
        if (cb) cb(false, {});
        return;
    }
    LOG_STEAM_INFO("Public lobby search requested worldwide version={} handle={}", MOD_VERSION, g_pending_list_call);
    g_pending_list_cb = std::move(cb);
    g_list_started = std::chrono::steady_clock::now();
}

void SteamMatchmaking::process_pending_chats() {
    std::vector<std::pair<uint64_t, int>> chats;
    {
        std::lock_guard<std::mutex> lock(g_chat_mutex);
        chats.swap(g_pending_chats);
    }
    for (auto& [lobby, chatid] : chats) {
        SaveTransfer::instance().on_chat_message(lobby, chatid);
    }
}

void SteamMatchmaking::set_lobby_data(const char* key, const char* value) {
    if (!steam_matchmaking_ || !fn_SetLobbyData || !in_lobby()) return;
    fn_SetLobbyData(steam_matchmaking_, lobby_id_, key, value);
}

std::string SteamMatchmaking::get_lobby_data(const char* key) const {
    if (!steam_matchmaking_ || !fn_GetLobbyData || !in_lobby()) return "";
    const char* v = fn_GetLobbyData(steam_matchmaking_, lobby_id_, key);
    return v ? v : "";
}

bool SteamMatchmaking::publish_lobby_data(const char* key,const std::string& value,std::string& error){
    if(!steam_matchmaking_||!fn_SetLobbyData||!fn_GetLobbyData||!in_lobby()||!is_host_||!key||!fn_SetLobbyData(steam_matchmaking_,lobby_id_,key,value.c_str())){error="Steam Host metadata setter rejected key="+std::string(key?key:"");LOG_STEAM_ERROR("{}",error);return false;}
    const char* actual=fn_GetLobbyData(steam_matchmaking_,lobby_id_,key);
    if(!actual||value!=actual){error="Steam Host metadata local readback differs key="+std::string(key);LOG_STEAM_ERROR("{}",error);return false;}
    error.clear();return true;
}
bool SteamMatchmaking::set_lobby_joinable(bool value,std::string& error){
    using Set=bool(*)(void*,uint64_t,bool);auto set=load_fn<Set>("SteamAPI_ISteamMatchmaking_SetLobbyJoinable");
    if(!steam_matchmaking_||!set||!in_lobby()||!is_host_||!set(steam_matchmaking_,lobby_id_,value)){error="Steam Host SetLobbyJoinable rejected";LOG_STEAM_ERROR("{}",error);return false;}
    error.clear();return true;
}
bool SteamMatchmaking::in_lobby() const { return lobby_id_ != 0; }
bool SteamMatchmaking::is_host() const { return is_host_; }
uint64_t SteamMatchmaking::current_lobby_id() const { return lobby_id_; }

uint64_t SteamMatchmaking::host_steam_id() const {
    if (!steam_matchmaking_ || !fn_GetLobbyOwner || !in_lobby()) return 0;
    return fn_GetLobbyOwner(steam_matchmaking_, lobby_id_);
}

void SteamMatchmaking::set_invite_handler(std::function<void(uint64_t)> handler) { invite_handler_ = std::move(handler); }

void SteamMatchmaking::set_member_callback(LobbyMemberCallback cb) { member_callback_ = std::move(cb); }

#include "steam_presence.inc"

} // namespace mt2mp::net

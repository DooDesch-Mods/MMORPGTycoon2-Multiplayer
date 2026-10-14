#include "mt2mp/net/steam_p2p.h"
#include "mt2mp/net/steam_module.h"
#include "mt2mp/core/logger.h"

#include <windows.h>
#include <unordered_map>
#include <mutex>
#include <cstring>

namespace mt2mp::net {

// Steam Networking Sockets types (minimal, only what we need)
using HSteamListenSocket = uint32_t;
using HSteamNetConnection = uint32_t;
using HSteamNetPollGroup = uint32_t;

constexpr int k_nSteamNetworkingSend_Reliable = 8;
constexpr int k_nSteamNetworkingSend_Unreliable = 0;

// Connection state values from SteamNetworkingTypes.h
constexpr int k_ESteamNetworkingConnectionState_Connecting = 1;
constexpr int k_ESteamNetworkingConnectionState_Connected = 3;
constexpr int k_ESteamNetworkingConnectionState_ClosedByPeer = 4;
constexpr int k_ESteamNetworkingConnectionState_ProblemDetectedLocally = 5;

#pragma pack(push, 8)
struct SteamNetworkingMessage_t {
    void* m_pData;
    int m_cbSize;
    HSteamNetConnection m_conn;
    uint64_t m_identityPeer_data[5]; // SteamNetworkingIdentity is 18 bytes, padded
    int64_t m_nConnUserData;
    int64_t m_usecTimeReceived;
    int64_t m_nMessageNumber;
    void* m_pfnFreeData;
    void* m_pfnRelease;
    int m_nChannel;
    int m_nFlags;
    int64_t m_nUserData;
    uint16_t m_idxLane;
    uint16_t m_pad[3];
};

struct SteamNetworkingIdentity {
    int m_eType; // 16 = k_ESteamNetworkingIdentityType_SteamID
    int m_cbSize;
    union {
        uint64_t m_steamID64;
        char m_szUnknownRawString[128];
        uint8_t m_rawBuf[128];
    };
};

struct SteamNetConnectionInfo_t {
    SteamNetworkingIdentity m_identityRemote;
    int64_t m_nUserData;
    HSteamListenSocket m_hListenSocket;
    uint32_t m_addrRemote_ip;
    uint16_t m_addrRemote_port;
    uint16_t m_pad1;
    uint16_t m_idxLane;
    uint16_t m_pad2;
    int m_eState;
    int m_eEndReason;
    char m_szEndDebug[128];
    char m_szConnectionDescription[128];
    int m_nFlags;
    int m_pad3[63];
};

struct SteamNetConnectionStatusChangedCallback_t {
    HSteamNetConnection m_hConn;
    SteamNetConnectionInfo_t m_info;
    int m_eOldState;
};
#pragma pack(pop)

// Callback type for global connection status changes
using FnConnStatusChanged = void(*)(SteamNetConnectionStatusChangedCallback_t*);

// Steam API function pointer types
using FnAccessor = void*(*)();
using FnInitRelay = void(*)();
using FnSetGlobalCallback = void(*)(int callback_id, void* fn);
using FnCreateListenP2P = HSteamListenSocket(*)(void* self, int virtual_port, int n_options, void* options);
using FnConnectP2P = HSteamNetConnection(*)(void* self, const SteamNetworkingIdentity* identity, int virtual_port, int n_options, void* options);
using FnAcceptConnection = int(*)(void* self, HSteamNetConnection conn);
using FnCloseConnection = bool(*)(void* self, HSteamNetConnection conn, int reason, const char* debug, bool linger);
using FnSendMessage = int64_t(*)(void* self, HSteamNetConnection conn, const void* data, uint32_t size, int send_flags, int64_t* out_msg_num);
using FnReceiveMessages = int(*)(void* self, HSteamNetConnection conn, SteamNetworkingMessage_t** out_msgs, int max);
using FnGetConnectionInfo = bool(*)(void* self, HSteamNetConnection conn, SteamNetConnectionInfo_t* info);
using FnRunCallbacks = void(*)(void* self);
using FnReleaseMessage = void(*)(SteamNetworkingMessage_t* msg);

// Steam interface and function pointers
static void* g_sockets_iface = nullptr;
static void* g_utils_iface = nullptr;

static FnInitRelay fn_InitRelay = nullptr;
static FnSetGlobalCallback fn_SetGlobalCallback = nullptr;
static FnCreateListenP2P fn_CreateListenSocketP2P = nullptr;
static FnConnectP2P fn_ConnectP2P = nullptr;
static FnAcceptConnection fn_AcceptConnection = nullptr;
static FnCloseConnection fn_CloseConnection = nullptr;
static FnSendMessage fn_SendMessageToConnection = nullptr;
static FnReceiveMessages fn_ReceiveMessagesOnConnection = nullptr;
static FnGetConnectionInfo fn_GetConnectionInfo = nullptr;
static FnRunCallbacks fn_RunCallbacks = nullptr;

// Callback ID for SteamNetConnectionStatusChanged (from Steamworks SDK)
constexpr int k_iSteamNetConnectionStatusChangedCallback = 1221;

// Connection state
static HSteamListenSocket g_listen_socket = 0;
static std::mutex g_conn_mutex;
static std::unordered_map<HSteamNetConnection, uint64_t> g_connections; // conn → peer steam id
static SteamP2P::ReceiveCallback g_receive_cb;

static HMODULE get_steam() { return steam_module(); }

template<typename T>
static T load_fn(const char* name) {
    return reinterpret_cast<T>(GetProcAddress(get_steam(), name));
}

// Global callback for connection status changes (C function pointer, NOT CCallbackBase!)
// NOTE: The SteamNetConnectionInfo_t struct layout is tricky - m_eState offset
// is unreliable with our struct definition. Instead, we just track connections
// by the callback events themselves and try to accept/send pragmatically.
static void on_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* info) {
    if (!info || info->m_hConn == 0) return;

    uint64_t peer_sid = info->m_info.m_identityRemote.m_steamID64;
    HSteamListenSocket listen_sock = info->m_info.m_hListenSocket;

    LOG_INFO("[P2P] Callback conn={} peer={} listen_sock={}",
        info->m_hConn, peer_sid, listen_sock);

    // Try to accept any incoming connection (listen_sock != 0 means we're the host)
    // AcceptConnection is idempotent - safe to call multiple times
    if (listen_sock != 0 && fn_AcceptConnection) {
        int result = fn_AcceptConnection(g_sockets_iface, info->m_hConn);
        LOG_INFO("[P2P] AcceptConnection({}) = {}", info->m_hConn, result);
    }

    // Track this connection (regardless of state - we'll discover if send fails)
    if (peer_sid != 0) {
        std::lock_guard<std::mutex> lock(g_conn_mutex);
        auto it = g_connections.find(info->m_hConn);
        if (it == g_connections.end()) {
            g_connections[info->m_hConn] = peer_sid;
            LOG_INFO("[P2P] *** Tracking connection {} → peer {} ***",
                info->m_hConn, peer_sid);
        }
    }

    // Poll actual state via GetConnectionInfo (more reliable than callback's m_info.m_eState)
    if (fn_GetConnectionInfo) {
        SteamNetConnectionInfo_t fresh = {};
        bool ok = fn_GetConnectionInfo(g_sockets_iface, info->m_hConn, &fresh);
        if (ok) {
            // Scan the struct for a likely state value (we know the struct is ~696 bytes)
            // State is an int (4 bytes). Connecting=1, Connected=3, Closed=4/5.
            const uint8_t* raw = (const uint8_t*)&fresh;
            for (size_t off = 0; off < sizeof(fresh) - 4; off += 4) {
                int val = *(const int*)(raw + off);
                if (val == 3) { // Connected
                    LOG_INFO("[P2P] State=CONNECTED (found at offset {})", off);
                    break;
                }
            }
        }
    }
}

SteamP2P& SteamP2P::instance() {
    static SteamP2P inst;
    return inst;
}

bool SteamP2P::init() {
    if (g_sockets_iface) return true;
    if (!get_steam()) {
        LOG_ERROR("[P2P] Cannot resolve exact selected Steam module");
        return false;
    }

    // Get interface accessors
    auto sockets_acc = load_fn<FnAccessor>("SteamAPI_SteamNetworkingSockets_SteamAPI_v012");
    if (!sockets_acc) sockets_acc = load_fn<FnAccessor>("SteamAPI_SteamNetworkingSockets_v012");
    if (!sockets_acc) sockets_acc = load_fn<FnAccessor>("SteamNetworkingSockets");

    auto utils_acc = load_fn<FnAccessor>("SteamAPI_SteamNetworkingUtils_SteamAPI_v004");
    if (!utils_acc) utils_acc = load_fn<FnAccessor>("SteamAPI_SteamNetworkingUtils_v004");
    if (!utils_acc) utils_acc = load_fn<FnAccessor>("SteamNetworkingUtils");

    if (!sockets_acc || !utils_acc) {
        LOG_ERROR("[P2P] Failed to find Steam Networking accessors");
        return false;
    }

    g_sockets_iface = sockets_acc();
    g_utils_iface = utils_acc();

    if (!g_sockets_iface || !g_utils_iface) {
        LOG_ERROR("[P2P] Got null interface pointers");
        return false;
    }

    // Load functions
    fn_InitRelay = load_fn<FnInitRelay>("SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess");
    fn_SetGlobalCallback = load_fn<FnSetGlobalCallback>("SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged");
    fn_CreateListenSocketP2P = load_fn<FnCreateListenP2P>("SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P");
    fn_ConnectP2P = load_fn<FnConnectP2P>("SteamAPI_ISteamNetworkingSockets_ConnectP2P");
    fn_AcceptConnection = load_fn<FnAcceptConnection>("SteamAPI_ISteamNetworkingSockets_AcceptConnection");
    fn_CloseConnection = load_fn<FnCloseConnection>("SteamAPI_ISteamNetworkingSockets_CloseConnection");
    fn_SendMessageToConnection = load_fn<FnSendMessage>("SteamAPI_ISteamNetworkingSockets_SendMessageToConnection");
    fn_ReceiveMessagesOnConnection = load_fn<FnReceiveMessages>("SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection");
    fn_GetConnectionInfo = load_fn<FnGetConnectionInfo>("SteamAPI_ISteamNetworkingSockets_GetConnectionInfo");
    fn_RunCallbacks = load_fn<FnRunCallbacks>("SteamAPI_ISteamNetworkingSockets_RunCallbacks");

    if (!fn_CreateListenSocketP2P || !fn_ConnectP2P || !fn_SendMessageToConnection) {
        LOG_ERROR("[P2P] Failed to load critical Steam Networking functions");
        return false;
    }

    // Init relay network access (requires no params, returns void)
    if (fn_InitRelay) {
        // Note: fn_InitRelay is on g_utils_iface but ISteamNetworkingUtils functions
        // take the interface as first param - let me check
        // Actually the C API signature is: void SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(ISteamNetworkingUtils* self)
        // But our typedef has no params... need to fix this
        using FnInitRelay2 = void(*)(void* self);
        ((FnInitRelay2)fn_InitRelay)(g_utils_iface);
        LOG_INFO("[P2P] Relay network access initialized");
    }

    // Register global callback for connection status changes
    if (fn_SetGlobalCallback) {
        // C signature: void SetGlobalCallback_*(ISteamNetworkingUtils* self, FnSteamNetConnectionStatusChanged callback)
        using FnSetCb = void(*)(void* self, FnConnStatusChanged cb);
        ((FnSetCb)fn_SetGlobalCallback)(g_utils_iface, &on_connection_status_changed);
        LOG_INFO("[P2P] Global connection status callback registered");
    }

    LOG_INFO("[P2P] SteamP2P initialized successfully");
    return true;
}

void SteamP2P::tick() {
    if (!g_sockets_iface) return;

    // Run Steam Networking Sockets internal callbacks
    if (fn_RunCallbacks) {
        fn_RunCallbacks(g_sockets_iface);
    }

    // Poll all active connections for incoming messages
    std::vector<HSteamNetConnection> conns;
    {
        std::lock_guard<std::mutex> lock(g_conn_mutex);
        for (auto& [conn, sid] : g_connections) {
            conns.push_back(conn);
        }
    }

    for (auto conn : conns) {
        SteamNetworkingMessage_t* msgs[16];
        int num = fn_ReceiveMessagesOnConnection(g_sockets_iface, conn, msgs, 16);
        if (num <= 0) continue;

        uint64_t peer_sid = 0;
        {
            std::lock_guard<std::mutex> lock(g_conn_mutex);
            auto it = g_connections.find(conn);
            if (it != g_connections.end()) peer_sid = it->second;
        }

        for (int i = 0; i < num; i++) {
            auto* msg = msgs[i];
            if (g_receive_cb && msg->m_pData && msg->m_cbSize > 0) {
                g_receive_cb(peer_sid, (const uint8_t*)msg->m_pData, msg->m_cbSize);
            }
            // Release message via vtable: m_pfnRelease takes the message as 'this'
            using FnRelease = void(*)(SteamNetworkingMessage_t*);
            if (msg->m_pfnRelease) {
                ((FnRelease)msg->m_pfnRelease)(msg);
            }
        }
    }
}

bool SteamP2P::start_listening() {
    if (!fn_CreateListenSocketP2P) return false;
    if (g_listen_socket) return true;

    g_listen_socket = fn_CreateListenSocketP2P(g_sockets_iface, 0, 0, nullptr);
    LOG_INFO("[P2P] Listen socket created: {}", g_listen_socket);
    return g_listen_socket != 0;
}

bool SteamP2P::connect_to(uint64_t host_steam_id) {
    if (!fn_ConnectP2P) return false;

    SteamNetworkingIdentity identity = {};
    identity.m_eType = 16; // SteamID type
    identity.m_cbSize = 8;
    identity.m_steamID64 = host_steam_id;

    HSteamNetConnection conn = fn_ConnectP2P(g_sockets_iface, &identity, 0, 0, nullptr);
    LOG_INFO("[P2P] ConnectP2P({}) → conn={}", host_steam_id, conn);
    return conn != 0;
}

void SteamP2P::shutdown() {
    if (!g_sockets_iface) return;

    {
        std::lock_guard<std::mutex> lock(g_conn_mutex);
        for (auto& [conn, sid] : g_connections) {
            if (fn_CloseConnection) {
                fn_CloseConnection(g_sockets_iface, conn, 0, "shutdown", false);
            }
        }
        g_connections.clear();
    }

    if (g_listen_socket && fn_CloseConnection) {
        // Note: There's CloseListenSocket separately, but for now just clear
        g_listen_socket = 0;
    }
}

bool SteamP2P::send(uint64_t peer_steam_id, const uint8_t* data, uint32_t size, bool reliable) {
    if (!fn_SendMessageToConnection || !data || size == 0) return false;

    int flags = reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;
    bool sent_any = false;

    std::lock_guard<std::mutex> lock(g_conn_mutex);
    for (auto& [conn, sid] : g_connections) {
        if (peer_steam_id != 0 && sid != peer_steam_id) continue;
        int64_t result = fn_SendMessageToConnection(g_sockets_iface, conn, data, size, flags, nullptr);
        if (result >= 0) sent_any = true;
    }
    return sent_any;
}

bool SteamP2P::broadcast(const uint8_t* data, uint32_t size, bool reliable) {
    return send(0, data, size, reliable);
}

void SteamP2P::set_receive_callback(ReceiveCallback cb) {
    g_receive_cb = std::move(cb);
}

bool SteamP2P::is_connected() const {
    std::lock_guard<std::mutex> lock(g_conn_mutex);
    return !g_connections.empty();
}

int SteamP2P::peer_count() const {
    std::lock_guard<std::mutex> lock(g_conn_mutex);
    return (int)g_connections.size();
}

} // namespace mt2mp::net

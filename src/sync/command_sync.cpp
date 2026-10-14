#include "mt2mp/sync/command_sync.h"
#include "mt2mp/sync/command_intercept.h"
#include "mt2mp/net/steam_p2p.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/core/logger.h"

#include <cstring>

namespace mt2mp::sync {

// ── Wire format for placement commands ──────────────────────────
// Magic: 0x4D544D50 "MTMP" (4 bytes)
// Version: 1 (1 byte)
// CommandType: 1 byte
// Reserved: 2 bytes
// SenderSteamID: 8 bytes
// PositionX/Y/Z: 12 bytes (3 floats)
// HasPosition: 1 byte
// Reserved: 3 bytes
// ActionStringLen: 2 bytes
// ActionString: variable

constexpr uint32_t kPacketMagic = 0x504D544D; // "MTMP" little-endian

#pragma pack(push, 1)
struct CommandPacket {
    uint32_t magic;
    uint8_t version;
    uint8_t cmd_type;
    uint16_t reserved1;
    uint64_t sender_steam_id;
    float pos_x, pos_y, pos_z;
    uint8_t has_position;
    uint8_t reserved2[3];
    uint16_t action_str_len;
    // followed by action_str_len bytes of UTF-8 string
};
#pragma pack(pop)

static bool g_initialized = false;

static void send_command(const GameCommand& cmd) {
    auto& p2p = net::SteamP2P::instance();
    if (!p2p.is_connected()) {
        return; // No peers connected - nothing to do
    }

    // Build packet
    std::vector<uint8_t> packet(sizeof(CommandPacket) + cmd.action.size());
    auto* hdr = reinterpret_cast<CommandPacket*>(packet.data());

    hdr->magic = kPacketMagic;
    hdr->version = 1;
    hdr->cmd_type = (uint8_t)cmd.type;
    hdr->reserved1 = 0;
    hdr->sender_steam_id = net::SteamNetworking::instance().local_steam_id();
    hdr->pos_x = cmd.x;
    hdr->pos_y = cmd.y;
    hdr->pos_z = cmd.z;
    hdr->has_position = cmd.has_position ? 1 : 0;
    memset(hdr->reserved2, 0, 3);
    hdr->action_str_len = (uint16_t)cmd.action.size();

    if (!cmd.action.empty()) {
        memcpy(packet.data() + sizeof(CommandPacket),
               cmd.action.data(), cmd.action.size());
    }

    bool ok = p2p.broadcast(packet.data(), (uint32_t)packet.size(), true);
    LOG_INFO("[CMD-SYNC] Broadcast \"{}\" pos=({:.1f},{:.1f},{:.1f}) → {} ({} bytes)",
        cmd.action, cmd.x, cmd.y, cmd.z, ok ? "OK" : "FAIL", packet.size());
}

static void on_command_received(uint64_t sender_id, const uint8_t* data, uint32_t size) {
    if (size < sizeof(CommandPacket)) {
        LOG_WARN("[CMD-SYNC] Packet too small: {} bytes", size);
        return;
    }

    auto* hdr = reinterpret_cast<const CommandPacket*>(data);
    if (hdr->magic != kPacketMagic) {
        LOG_WARN("[CMD-SYNC] Bad magic: 0x{:X}", hdr->magic);
        return;
    }

    if (hdr->version != 1) {
        LOG_WARN("[CMD-SYNC] Unsupported version: {}", hdr->version);
        return;
    }

    if (size < sizeof(CommandPacket) + hdr->action_str_len) {
        LOG_WARN("[CMD-SYNC] Truncated packet");
        return;
    }

    std::string action((const char*)(data + sizeof(CommandPacket)), hdr->action_str_len);
    GameCommand cmd = parse_action(action);
    cmd.x = hdr->pos_x;
    cmd.y = hdr->pos_y;
    cmd.z = hdr->pos_z;
    cmd.has_position = hdr->has_position != 0;
    cmd.source_steam_id = hdr->sender_steam_id;

    LOG_INFO("[CMD-SYNC] *** RECEIVED from {} : \"{}\" pos=({:.1f},{:.1f},{:.1f}) ***",
        sender_id, action, cmd.x, cmd.y, cmd.z);

    // TODO: Replay the command locally
    // For now, just log the receipt.
}

bool init_command_sync() {
    if (g_initialized) return true;

    // Initialize Steam P2P
    if (!net::SteamP2P::instance().init()) {
        LOG_WARN("[CMD-SYNC] Steam P2P init failed");
        return false;
    }

    // Set up receive callback
    net::SteamP2P::instance().set_receive_callback(&on_command_received);

    // Hook command interceptor → send to peers
    set_command_callback([](const GameCommand& cmd) {
        // Only send PLACE commands for now (most relevant for testing)
        if (cmd.type == GameCommand::PLACE && cmd.has_position) {
            send_command(cmd);
        }
    });

    g_initialized = true;
    LOG_INFO("[CMD-SYNC] Command sync initialized");
    return true;
}

void tick_command_sync() {
    if (!g_initialized) return;
    net::SteamP2P::instance().tick();
}

} // namespace mt2mp::sync

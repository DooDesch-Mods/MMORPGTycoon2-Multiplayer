#pragma once

#include "mt2mp/net/packet_types.h"
#include "mt2mp/game/edit_operation.h"
#include <vector>
#include <cstdint>

namespace mt2mp::net {

class PacketSerializer {
public:
    // Build packets from game data
    static Packet make_cursor_update(uint64_t steam_id, uint32_t shard_id,
        float wx, float wy, float wz, uint8_t cursor_type);

    static Packet make_edit_request(const game::EditOperation& op);
    static Packet make_edit_approved(const game::EditOperation& op);
    static Packet make_edit_rejected(uint32_t sequence_id, const char* reason);

    static Packet make_time_sync(uint32_t day, float fraction, uint8_t speed);
    static Packet make_time_pause();
    static Packet make_time_unpause();
    static Packet make_time_speed(uint8_t speed);

    static Packet make_handshake_request(uint64_t steam_id, const char* name);
    static Packet make_handshake_response(bool accepted);
    static Packet make_heartbeat();

    static Packet make_lock_request(uint64_t steam_id, uint32_t region_id);
    static Packet make_lock_granted(uint32_t region_id);
    static Packet make_lock_denied(uint32_t region_id);
    static Packet make_lock_release(uint64_t steam_id, uint32_t region_id);

    static Packet make_chat_message(uint64_t steam_id, const char* name, const char* text);

    static Packet make_save_chunk(uint32_t chunk_index, const uint8_t* data, uint32_t size);
    static Packet make_save_complete(uint32_t total_chunks);

    static Packet make_checksum(const ChecksumPayload& cs);

    // Extract data from packets
    static CursorPayload read_cursor(const Packet& p);
    static game::EditOperation read_edit(const Packet& p);
    static TimeSyncPayload read_time_sync(const Packet& p);
    static RegionLockPayload read_lock(const Packet& p);
    static ChecksumPayload read_checksum(const Packet& p);

private:
    static uint32_t next_sequence();
};

} // namespace mt2mp::net

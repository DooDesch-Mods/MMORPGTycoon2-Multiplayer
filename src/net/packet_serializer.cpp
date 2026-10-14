#include "mt2mp/net/packet_serializer.h"
#include <cstring>
#include <atomic>

namespace mt2mp::net {

static std::atomic<uint32_t> g_sequence{0};

uint32_t PacketSerializer::next_sequence() {
    return g_sequence.fetch_add(1);
}

// Packet serialization
std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> buf(sizeof(PacketHeader) + payload.size());
    std::memcpy(buf.data(), &header, sizeof(PacketHeader));
    if (!payload.empty()) {
        std::memcpy(buf.data() + sizeof(PacketHeader), payload.data(), payload.size());
    }
    return buf;
}

Packet Packet::deserialize(const uint8_t* data, size_t length) {
    Packet p;
    if (length < sizeof(PacketHeader)) return p;

    std::memcpy(&p.header, data, sizeof(PacketHeader));
    size_t payload_size = length - sizeof(PacketHeader);
    if (payload_size > 0 && payload_size == p.header.payload_length) {
        p.payload.resize(payload_size);
        std::memcpy(p.payload.data(), data + sizeof(PacketHeader), payload_size);
    }
    return p;
}

Packet PacketSerializer::make_cursor_update(uint64_t steam_id, uint32_t shard_id,
    float wx, float wy, float wz, uint8_t cursor_type) {
    CursorPayload cp{steam_id, shard_id, wx, wy, wz, cursor_type};
    Packet p;
    p.header.type = PacketType::CURSOR_UPDATE;
    p.header.sequence = next_sequence();
    p.header.payload_length = sizeof(cp);
    p.payload.resize(sizeof(cp));
    std::memcpy(p.payload.data(), &cp, sizeof(cp));
    return p;
}

Packet PacketSerializer::make_heartbeat() {
    Packet p;
    p.header.type = PacketType::HEARTBEAT;
    p.header.sequence = next_sequence();
    p.header.payload_length = 0;
    return p;
}

Packet PacketSerializer::make_time_sync(uint32_t day, float fraction, uint8_t speed) {
    TimeSyncPayload ts{day, fraction, speed};
    Packet p;
    p.header.type = PacketType::TIME_SYNC;
    p.header.sequence = next_sequence();
    p.header.payload_length = sizeof(ts);
    p.payload.resize(sizeof(ts));
    std::memcpy(p.payload.data(), &ts, sizeof(ts));
    return p;
}

Packet PacketSerializer::make_time_pause() {
    Packet p;
    p.header.type = PacketType::TIME_PAUSE;
    p.header.sequence = next_sequence();
    p.header.payload_length = 0;
    return p;
}

Packet PacketSerializer::make_time_unpause() {
    Packet p;
    p.header.type = PacketType::TIME_UNPAUSE;
    p.header.sequence = next_sequence();
    p.header.payload_length = 0;
    return p;
}

Packet PacketSerializer::make_time_speed(uint8_t speed) {
    Packet p;
    p.header.type = PacketType::TIME_SPEED_CHANGE;
    p.header.sequence = next_sequence();
    p.header.payload_length = 1;
    p.payload = {speed};
    return p;
}

Packet PacketSerializer::make_lock_request(uint64_t steam_id, uint32_t region_id) {
    RegionLockPayload rl{steam_id, region_id};
    Packet p;
    p.header.type = PacketType::LOCK_REQUEST;
    p.header.sequence = next_sequence();
    p.header.payload_length = sizeof(rl);
    p.payload.resize(sizeof(rl));
    std::memcpy(p.payload.data(), &rl, sizeof(rl));
    return p;
}

Packet PacketSerializer::make_lock_granted(uint32_t region_id) {
    Packet p;
    p.header.type = PacketType::LOCK_GRANTED;
    p.header.sequence = next_sequence();
    p.header.payload_length = sizeof(region_id);
    p.payload.resize(sizeof(region_id));
    std::memcpy(p.payload.data(), &region_id, sizeof(region_id));
    return p;
}

Packet PacketSerializer::make_lock_denied(uint32_t region_id) {
    Packet p;
    p.header.type = PacketType::LOCK_DENIED;
    p.header.sequence = next_sequence();
    p.header.payload_length = sizeof(region_id);
    p.payload.resize(sizeof(region_id));
    std::memcpy(p.payload.data(), &region_id, sizeof(region_id));
    return p;
}

Packet PacketSerializer::make_lock_release(uint64_t steam_id, uint32_t region_id) {
    RegionLockPayload rl{steam_id, region_id};
    Packet p;
    p.header.type = PacketType::LOCK_RELEASE;
    p.header.sequence = next_sequence();
    p.header.payload_length = sizeof(rl);
    p.payload.resize(sizeof(rl));
    std::memcpy(p.payload.data(), &rl, sizeof(rl));
    return p;
}

Packet PacketSerializer::make_chat_message(uint64_t steam_id, const char* name, const char* text) {
    // Format: steamid(8) + name_len(2) + name + text_len(2) + text
    uint16_t name_len = static_cast<uint16_t>(strlen(name));
    uint16_t text_len = static_cast<uint16_t>(strlen(text));
    size_t total = 8 + 2 + name_len + 2 + text_len;

    Packet p;
    p.header.type = PacketType::CHAT_MESSAGE;
    p.header.sequence = next_sequence();
    p.header.payload_length = static_cast<uint32_t>(total);
    p.payload.resize(total);

    uint8_t* ptr = p.payload.data();
    std::memcpy(ptr, &steam_id, 8); ptr += 8;
    std::memcpy(ptr, &name_len, 2); ptr += 2;
    std::memcpy(ptr, name, name_len); ptr += name_len;
    std::memcpy(ptr, &text_len, 2); ptr += 2;
    std::memcpy(ptr, text, text_len);
    return p;
}

Packet PacketSerializer::make_save_chunk(uint32_t chunk_index, const uint8_t* data, uint32_t size) {
    Packet p;
    p.header.type = PacketType::SAVE_CHUNK;
    p.header.sequence = next_sequence();
    p.header.payload_length = 4 + size;
    p.payload.resize(4 + size);
    std::memcpy(p.payload.data(), &chunk_index, 4);
    std::memcpy(p.payload.data() + 4, data, size);
    return p;
}

Packet PacketSerializer::make_save_complete(uint32_t total_chunks) {
    Packet p;
    p.header.type = PacketType::SAVE_COMPLETE;
    p.header.sequence = next_sequence();
    p.header.payload_length = 4;
    p.payload.resize(4);
    std::memcpy(p.payload.data(), &total_chunks, 4);
    return p;
}

Packet PacketSerializer::make_checksum(const ChecksumPayload& cs) {
    Packet p;
    p.header.type = PacketType::CHECKSUM;
    p.header.sequence = next_sequence();
    p.header.payload_length = sizeof(cs);
    p.payload.resize(sizeof(cs));
    std::memcpy(p.payload.data(), &cs, sizeof(cs));
    return p;
}

Packet PacketSerializer::make_handshake_request(uint64_t steam_id, const char* name) {
    uint16_t name_len = static_cast<uint16_t>(strlen(name));
    Packet p;
    p.header.type = PacketType::HANDSHAKE_REQUEST;
    p.header.sequence = next_sequence();
    p.header.payload_length = 8 + 2 + name_len;
    p.payload.resize(8 + 2 + name_len);
    std::memcpy(p.payload.data(), &steam_id, 8);
    std::memcpy(p.payload.data() + 8, &name_len, 2);
    std::memcpy(p.payload.data() + 10, name, name_len);
    return p;
}

Packet PacketSerializer::make_handshake_response(bool accepted) {
    Packet p;
    p.header.type = PacketType::HANDSHAKE_RESPONSE;
    p.header.sequence = next_sequence();
    p.header.payload_length = 1;
    p.payload = {static_cast<uint8_t>(accepted ? 1 : 0)};
    return p;
}

Packet PacketSerializer::make_edit_request(const game::EditOperation& op) {
    auto data = op.serialize();
    Packet p;
    p.header.type = PacketType::EDIT_REQUEST;
    p.header.sequence = next_sequence();
    p.header.payload_length = static_cast<uint32_t>(data.size());
    p.payload = std::move(data);
    return p;
}

Packet PacketSerializer::make_edit_approved(const game::EditOperation& op) {
    auto data = op.serialize();
    Packet p;
    p.header.type = PacketType::EDIT_APPROVED;
    p.header.sequence = next_sequence();
    p.header.payload_length = static_cast<uint32_t>(data.size());
    p.payload = std::move(data);
    return p;
}

Packet PacketSerializer::make_edit_rejected(uint32_t sequence_id, const char* reason) {
    uint16_t reason_len = static_cast<uint16_t>(strlen(reason));
    Packet p;
    p.header.type = PacketType::EDIT_REJECTED;
    p.header.sequence = next_sequence();
    p.header.payload_length = 4 + 2 + reason_len;
    p.payload.resize(4 + 2 + reason_len);
    std::memcpy(p.payload.data(), &sequence_id, 4);
    std::memcpy(p.payload.data() + 4, &reason_len, 2);
    std::memcpy(p.payload.data() + 6, reason, reason_len);
    return p;
}

CursorPayload PacketSerializer::read_cursor(const Packet& p) {
    CursorPayload cp{};
    if (p.payload.size() >= sizeof(cp)) {
        std::memcpy(&cp, p.payload.data(), sizeof(cp));
    }
    return cp;
}

TimeSyncPayload PacketSerializer::read_time_sync(const Packet& p) {
    TimeSyncPayload ts{};
    if (p.payload.size() >= sizeof(ts)) {
        std::memcpy(&ts, p.payload.data(), sizeof(ts));
    }
    return ts;
}

RegionLockPayload PacketSerializer::read_lock(const Packet& p) {
    RegionLockPayload rl{};
    if (p.payload.size() >= sizeof(rl)) {
        std::memcpy(&rl, p.payload.data(), sizeof(rl));
    }
    return rl;
}

ChecksumPayload PacketSerializer::read_checksum(const Packet& p) {
    ChecksumPayload cs{};
    if (p.payload.size() >= sizeof(cs)) {
        std::memcpy(&cs, p.payload.data(), sizeof(cs));
    }
    return cs;
}

game::EditOperation PacketSerializer::read_edit(const Packet& p) {
    return game::EditOperation::deserialize(p.payload.data(), p.payload.size());
}

} // namespace mt2mp::net

#include "mt2mp/game/edit_operation.h"
#include <cstring>

namespace mt2mp::game {

std::vector<uint8_t> EditOperation::serialize() const {
    // Layout: seq(4) + timestamp(8) + steam_id(8) + op_type(2) + region_id(4)
    //         + shard_id(4) + reason_len(2) + reason + data_len(4) + data
    uint16_t reason_len = static_cast<uint16_t>(reason.size());
    uint32_t data_len = static_cast<uint32_t>(data.size());
    size_t total = 4 + 8 + 8 + 2 + 4 + 4 + 2 + reason_len + 4 + data_len;

    std::vector<uint8_t> buf(total);
    uint8_t* p = buf.data();

    std::memcpy(p, &sequence_id, 4); p += 4;
    std::memcpy(p, &timestamp, 8); p += 8;
    std::memcpy(p, &player_steam_id, 8); p += 8;
    std::memcpy(p, &op_type, 2); p += 2;
    std::memcpy(p, &region_id, 4); p += 4;
    std::memcpy(p, &shard_id, 4); p += 4;
    std::memcpy(p, &reason_len, 2); p += 2;
    if (reason_len > 0) { std::memcpy(p, reason.data(), reason_len); p += reason_len; }
    std::memcpy(p, &data_len, 4); p += 4;
    if (data_len > 0) { std::memcpy(p, data.data(), data_len); }

    return buf;
}

EditOperation EditOperation::deserialize(const uint8_t* buf, size_t len) {
    EditOperation op;
    if (len < 32) return op; // minimum size

    const uint8_t* p = buf;
    std::memcpy(&op.sequence_id, p, 4); p += 4;
    std::memcpy(&op.timestamp, p, 8); p += 8;
    std::memcpy(&op.player_steam_id, p, 8); p += 8;
    std::memcpy(&op.op_type, p, 2); p += 2;
    std::memcpy(&op.region_id, p, 4); p += 4;
    std::memcpy(&op.shard_id, p, 4); p += 4;

    uint16_t reason_len = 0;
    std::memcpy(&reason_len, p, 2); p += 2;
    if (reason_len > 0 && p + reason_len <= buf + len) {
        op.reason.assign(reinterpret_cast<const char*>(p), reason_len);
        p += reason_len;
    }

    uint32_t data_len = 0;
    if (p + 4 <= buf + len) {
        std::memcpy(&data_len, p, 4); p += 4;
        if (data_len > 0 && p + data_len <= buf + len) {
            op.data.assign(p, p + data_len);
        }
    }

    return op;
}

} // namespace mt2mp::game

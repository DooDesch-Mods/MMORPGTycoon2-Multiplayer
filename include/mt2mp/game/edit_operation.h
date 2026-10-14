#pragma once

#include "mt2mp/net/packet_types.h"
#include <vector>
#include <cstdint>
#include <string>

namespace mt2mp::game {

struct EditOperation {
    uint32_t sequence_id = 0;
    uint64_t timestamp = 0;          // ms since epoch
    uint64_t player_steam_id = 0;
    net::EditOpType op_type = net::EditOpType::GENERIC_EDIT;
    uint32_t region_id = 0;
    uint32_t shard_id = 0;

    // Type-specific serialized data
    std::vector<uint8_t> data;

    // Edit reason string from EditManager
    std::string reason;

    // Serialize/deserialize for network
    std::vector<uint8_t> serialize() const;
    static EditOperation deserialize(const uint8_t* buf, size_t len);
};

} // namespace mt2mp::game

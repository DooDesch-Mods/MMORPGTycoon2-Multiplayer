#pragma once

#include <cstdint>
#include <vector>

namespace mt2mp::game {

// Binary diff engine for RecordV2 save files.
// Uses block-level comparison (256 byte blocks) to find changed regions.
// Only changed blocks are transmitted, dramatically reducing bandwidth.

struct DiffBlock {
    uint32_t block_index;   // which 256-byte block changed
    uint8_t data[256];      // the new content
};

struct SaveDiff {
    uint32_t base_crc;      // CRC of the state this diff is relative to
    uint32_t new_crc;       // CRC after applying this diff
    uint32_t new_total_size; // total size of new state (may differ from base)
    std::vector<DiffBlock> blocks;

    // Serialize for transmission
    std::vector<uint8_t> serialize() const;
    static SaveDiff deserialize(const uint8_t* data, size_t len);

    size_t wire_size() const { return 12 + blocks.size() * (4 + 256); }
};

class DiffEngine {
public:
    // Compute diff between two states
    static SaveDiff compute(const std::vector<uint8_t>& old_state,
                            const std::vector<uint8_t>& new_state,
                            uint32_t old_crc, uint32_t new_crc);

    // Apply diff to produce new state
    static std::vector<uint8_t> apply(const std::vector<uint8_t>& old_state,
                                       const SaveDiff& diff);

    static constexpr int BLOCK_SIZE = 256;
};

} // namespace mt2mp::game

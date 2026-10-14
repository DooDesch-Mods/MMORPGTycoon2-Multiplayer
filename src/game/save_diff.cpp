#include "mt2mp/game/save_diff.h"
#include "mt2mp/core/logger.h"

#include <cstring>
#include <algorithm>

namespace mt2mp::game {

SaveDiff DiffEngine::compute(const std::vector<uint8_t>& old_state,
                              const std::vector<uint8_t>& new_state,
                              uint32_t old_crc, uint32_t new_crc) {
    SaveDiff diff;
    diff.base_crc = old_crc;
    diff.new_crc = new_crc;
    diff.new_total_size = static_cast<uint32_t>(new_state.size());

    size_t max_blocks = (std::max(old_state.size(), new_state.size()) + BLOCK_SIZE - 1) / BLOCK_SIZE;

    for (size_t i = 0; i < max_blocks; ++i) {
        size_t offset = i * BLOCK_SIZE;

        // Get block from old and new state
        const uint8_t* old_block = (offset < old_state.size()) ? old_state.data() + offset : nullptr;
        const uint8_t* new_block = (offset < new_state.size()) ? new_state.data() + offset : nullptr;

        size_t old_len = old_block ? std::min<size_t>(BLOCK_SIZE, old_state.size() - offset) : 0;
        size_t new_len = new_block ? std::min<size_t>(BLOCK_SIZE, new_state.size() - offset) : 0;

        bool changed = false;
        if (old_len != new_len) {
            changed = true;
        } else if (new_len > 0 && old_len > 0) {
            changed = (memcmp(old_block, new_block, new_len) != 0);
        } else if (new_len > 0) {
            changed = true; // new data that didn't exist before
        }

        if (changed && new_len > 0) {
            DiffBlock db;
            db.block_index = static_cast<uint32_t>(i);
            memset(db.data, 0, BLOCK_SIZE);
            memcpy(db.data, new_block, new_len);
            diff.blocks.push_back(db);
        }
    }

    LOG_INFO("[DIFF] Computed: {} changed blocks out of {} total ({} bytes diff, {:.1f}% of full state)",
        diff.blocks.size(), max_blocks, diff.wire_size(),
        diff.wire_size() * 100.0 / std::max<size_t>(1, new_state.size()));

    return diff;
}

std::vector<uint8_t> DiffEngine::apply(const std::vector<uint8_t>& old_state,
                                        const SaveDiff& diff) {
    // Start with old state, resize to new size
    std::vector<uint8_t> result = old_state;
    result.resize(diff.new_total_size, 0);

    // Apply changed blocks
    for (const auto& block : diff.blocks) {
        size_t offset = static_cast<size_t>(block.block_index) * BLOCK_SIZE;
        size_t copy_len = std::min<size_t>(BLOCK_SIZE, result.size() - offset);
        if (offset < result.size()) {
            memcpy(result.data() + offset, block.data, copy_len);
        }
    }

    LOG_INFO("[DIFF] Applied: {} blocks to state ({} -> {} bytes)",
        diff.blocks.size(), old_state.size(), result.size());

    return result;
}

std::vector<uint8_t> SaveDiff::serialize() const {
    std::vector<uint8_t> buf;
    size_t total = 12 + blocks.size() * (4 + 256);
    buf.resize(total);

    uint8_t* p = buf.data();
    memcpy(p, &base_crc, 4); p += 4;
    memcpy(p, &new_crc, 4); p += 4;
    memcpy(p, &new_total_size, 4); p += 4;

    for (const auto& block : blocks) {
        memcpy(p, &block.block_index, 4); p += 4;
        memcpy(p, block.data, 256); p += 256;
    }

    return buf;
}

SaveDiff SaveDiff::deserialize(const uint8_t* data, size_t len) {
    SaveDiff diff;
    if (len < 12) return diff;

    const uint8_t* p = data;
    memcpy(&diff.base_crc, p, 4); p += 4;
    memcpy(&diff.new_crc, p, 4); p += 4;
    memcpy(&diff.new_total_size, p, 4); p += 4;

    size_t remaining = len - 12;
    while (remaining >= 260) {
        DiffBlock block;
        memcpy(&block.block_index, p, 4); p += 4;
        memcpy(block.data, p, 256); p += 256;
        diff.blocks.push_back(block);
        remaining -= 260;
    }

    return diff;
}

} // namespace mt2mp::game

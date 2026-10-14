#include "mt2mp/game/save_differ.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::game {

SaveStateTracker& SaveStateTracker::instance() {
    static SaveStateTracker tracker;
    return tracker;
}

uint32_t SaveStateTracker::compute_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

const std::vector<uint8_t>& SaveStateTracker::previous_state() const {
    return previous_state_;
}

void SaveStateTracker::on_save_complete(const std::vector<uint8_t>& uncompressed_data) {
    std::lock_guard lock(mutex_);
    uint32_t old_hash = current_hash_;
    previous_state_ = std::move(current_state_);
    previous_hash_ = old_hash;
    current_state_ = uncompressed_data;
    current_hash_ = compute_crc32(uncompressed_data.data(), uncompressed_data.size());

    if (old_hash != 0 && old_hash != current_hash_) {
        LOG_INFO("State changed: CRC32 0x{:08X} -> 0x{:08X} ({} bytes)",
            old_hash, current_hash_, uncompressed_data.size());
        // SAFETY: Don't fire callbacks here - they run complex code (diffs, Steam API)
        // that can crash. Instead, set a flag and process in the main tick loop.
        pending_change_ = true;
        pending_old_hash_ = old_hash;
        pending_new_hash_ = current_hash_;
        pending_size_ = uncompressed_data.size();
    } else if (old_hash == 0) {
        LOG_INFO("Initial state captured: CRC32 0x{:08X} ({} bytes)",
            current_hash_, uncompressed_data.size());
    }
}

void SaveStateTracker::on_load_complete(const std::vector<uint8_t>& uncompressed_data) {
    std::lock_guard lock(mutex_);
    // DON'T overwrite current_state_/current_hash_ from loads!
    // Loads happen frequently (game reads various files) and would
    // reset our save-state baseline, making diffs between saves useless.
    // Only log for diagnostics.
    uint32_t load_crc = compute_crc32(uncompressed_data.data(), uncompressed_data.size());
    LOG_INFO("State loaded: CRC32 0x{:08X} ({} bytes) [not overwriting save baseline]",
        load_crc, uncompressed_data.size());
}

void SaveStateTracker::process_pending_changes() {
    if (!pending_change_) return;
    pending_change_ = false;

    LOG_INFO("Processing pending state change on main thread: 0x{:08X} -> 0x{:08X} ({} bytes)",
        pending_old_hash_, pending_new_hash_, pending_size_);

    if (on_change_) {
        on_change_(pending_old_hash_, pending_new_hash_, pending_size_);
    }
}

uint32_t SaveStateTracker::current_hash() const {
    std::lock_guard lock(mutex_);
    return current_hash_;
}

const std::vector<uint8_t>& SaveStateTracker::current_state() const {
    return current_state_;
}

size_t SaveStateTracker::current_state_size() const {
    std::lock_guard lock(mutex_);
    return current_state_.size();
}

bool SaveStateTracker::has_changed_since(uint32_t previous_hash) const {
    std::lock_guard lock(mutex_);
    return current_hash_ != previous_hash;
}

void SaveStateTracker::set_change_callback(StateChangedCallback cb) {
    on_change_ = std::move(cb);
}

void SaveStateTracker::add_change_callback(StateChangedCallback cb) {
    if (!on_change_) {
        on_change_ = std::move(cb);
    } else {
        // Chain: call existing + new
        auto old = std::move(on_change_);
        on_change_ = [old = std::move(old), cb = std::move(cb)](uint32_t a, uint32_t b, size_t c) {
            old(a, b, c);
            cb(a, b, c);
        };
    }
}

} // namespace mt2mp::game

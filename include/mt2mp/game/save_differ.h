#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>

namespace mt2mp::game {

class SaveStateTracker {
public:
    static SaveStateTracker& instance();

    // Called by zlib hooks when a complete save/load cycle finishes
    void on_save_complete(const std::vector<uint8_t>& uncompressed_data);
    void on_load_complete(const std::vector<uint8_t>& uncompressed_data);

    // Get current state hash for desync detection
    uint32_t current_hash() const;

    // Get the full current state buffer (for transmission to new joiners)
    const std::vector<uint8_t>& current_state() const;
    const std::vector<uint8_t>& previous_state() const;
    size_t current_state_size() const;

    // Check if state changed since last check
    bool has_changed_since(uint32_t previous_hash) const;

    using StateChangedCallback = std::function<void(uint32_t old_hash, uint32_t new_hash, size_t size)>;
    void add_change_callback(StateChangedCallback cb);  // supports multiple callbacks
    void set_change_callback(StateChangedCallback cb);  // legacy single callback

    // Call from main thread tick to safely process pending state changes.
    // Fires callbacks on the main thread instead of the worker thread.
    void process_pending_changes();

private:
    SaveStateTracker() = default;
    static uint32_t compute_crc32(const uint8_t* data, size_t len);

    mutable std::mutex mutex_;
    std::vector<uint8_t> current_state_;
    std::vector<uint8_t> previous_state_;
    uint32_t current_hash_ = 0;
    uint32_t previous_hash_ = 0;
    StateChangedCallback on_change_;

    // Pending change notification (set in worker thread, processed in main thread)
    volatile bool pending_change_ = false;
    uint32_t pending_old_hash_ = 0;
    uint32_t pending_new_hash_ = 0;
    size_t pending_size_ = 0;
};

} // namespace mt2mp::game

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>

namespace mt2mp::sync {

class SaveSync {
public:
    static SaveSync& instance();

    // Host: stream current save to a joining client
    void send_save_to_peer(uint64_t steam_id);

    // Client: receive save chunks
    void on_save_chunk(uint32_t index, const uint8_t* data, uint32_t size);
    void on_save_complete(uint32_t total_chunks);

    // Client: load received save
    bool load_received_save();

    using SaveCompleteCallback = std::function<void(bool success)>;
    void set_complete_callback(SaveCompleteCallback cb);

private:
    SaveSync() = default;
    std::vector<std::vector<uint8_t>> chunks_;
    SaveCompleteCallback on_complete_;
};

} // namespace mt2mp::sync

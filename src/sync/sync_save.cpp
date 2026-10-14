#include "mt2mp/sync/sync_save.h"
#include "mt2mp/net/net_manager.h"
#include "mt2mp/net/packet_serializer.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"

#include <fstream>
#include <filesystem>

namespace mt2mp::sync {

SaveSync& SaveSync::instance() {
    static SaveSync ss;
    return ss;
}

void SaveSync::send_save_to_peer(uint64_t steam_id) {
    // TODO: Trigger game save, read the .vrz file, stream in chunks
    LOG_INFO("Sending save to peer {}", steam_id);

    // Placeholder: would read the autosave file and chunk it
    // std::string save_path = Config::instance().user_data_dir + "/save/...autosave.vrz";
    // std::ifstream file(save_path, std::ios::binary);
    // ... read and send chunks
}

void SaveSync::on_save_chunk(uint32_t index, const uint8_t* data, uint32_t size) {
    if (index >= chunks_.size()) chunks_.resize(index + 1);
    chunks_[index].assign(data, data + size);
    LOG_DEBUG("Received save chunk {}, {} bytes", index, size);
}

void SaveSync::on_save_complete(uint32_t total_chunks) {
    LOG_INFO("Save transfer complete: {} chunks", total_chunks);

    // Reassemble and write to disk
    std::string out_path = Config::instance().user_data_dir + "\\mp_sync.vrz";

    std::ofstream file(out_path, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to write sync save to {}", out_path);
        if (on_complete_) on_complete_(false);
        return;
    }

    for (const auto& chunk : chunks_) {
        file.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
    }
    file.close();
    chunks_.clear();

    LOG_INFO("Sync save written to {}", out_path);
    if (on_complete_) on_complete_(true);
}

bool SaveSync::load_received_save() {
    // TODO: Trigger game load of mp_sync.vrz
    LOG_INFO("Loading received save...");
    return true;
}

void SaveSync::set_complete_callback(SaveCompleteCallback cb) {
    on_complete_ = std::move(cb);
}

} // namespace mt2mp::sync

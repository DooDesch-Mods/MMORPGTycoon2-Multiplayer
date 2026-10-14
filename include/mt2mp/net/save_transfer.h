#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>

namespace mt2mp::net {

class SaveTransfer {
public:
    static SaveTransfer& instance();

    // Send a .vrz file to all lobby members via SendLobbyChatMsg
    bool send_save(uint64_t lobby_id, const std::string& game_name,
                   const uint8_t* data, uint32_t size);

    // Called when a lobby chat message is received (from callback)
    void on_chat_message(uint64_t lobby_id, int chat_id);

    // Check if a transfer is in progress
    bool is_sending() const { return sending_; }
    bool is_receiving() const { return receiving_; }
    float send_progress() const;
    float receive_progress() const;

    using SaveReceivedCallback = std::function<void(const std::string& game_name,
        const std::vector<uint8_t>& data)>;
    void set_receive_callback(SaveReceivedCallback cb);

private:
    SaveTransfer() = default;

    // Send state
    bool sending_ = false;
    std::vector<uint8_t> send_buffer_;
    uint32_t send_transfer_id_ = 0;
    uint16_t send_chunk_count_ = 0;
    uint16_t send_chunk_sent_ = 0;
    uint64_t send_lobby_ = 0;

    // Receive state
    bool receiving_ = false;
    std::vector<uint8_t> recv_buffer_;
    uint32_t recv_transfer_id_ = 0;
    uint16_t recv_chunk_count_ = 0;
    uint16_t recv_chunks_got_ = 0;
    std::string recv_game_name_;

    SaveReceivedCallback on_received_;

    // Send next chunk (called from tick)
public:
    void tick(); // call each frame to send queued chunks
};

// Chunk header
#pragma pack(push, 1)
struct SaveChunkHeader {
    char magic[4];          // "MT2S"
    uint32_t transfer_id;
    uint16_t chunk_idx;
    uint16_t chunk_count;
    uint32_t total_size;
    // Followed by: game_name (null-terminated) for chunk 0, or raw data for others
};
#pragma pack(pop)

constexpr int CHUNK_PAYLOAD_SIZE = 3500; // leave room for header in 4KB message
constexpr int CHUNK_HEADER_SIZE = sizeof(SaveChunkHeader);

} // namespace mt2mp::net

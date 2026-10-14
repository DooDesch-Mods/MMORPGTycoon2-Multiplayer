// Save Transfer via Lobby Data Streaming
// Host writes chunks sequentially as lobby data, joiner polls.
// No callbacks needed - pure polling like cursor sync.

#include "mt2mp/net/save_transfer.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/core/logger.h"

#include <cstring>
#include <cstdlib>
#include <random>
#include <algorithm>
#include <chrono>

// Base64 encode/decode for binary data in lobby data strings
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];
        out += b64_table[(n >> 18) & 0x3F];
        out += b64_table[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? b64_table[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? b64_table[n & 0x3F] : '=';
    }
    return out;
}

static std::vector<uint8_t> base64_decode(const char* str, size_t len) {
    static uint8_t dtable[256] = {};
    static bool init = false;
    if (!init) {
        memset(dtable, 0x80, sizeof(dtable));
        for (int i = 0; i < 64; i++) dtable[(uint8_t)b64_table[i]] = i;
        dtable['='] = 0;
        init = true;
    }

    std::vector<uint8_t> out;
    out.reserve(len * 3 / 4);
    uint32_t buf = 0;
    int bits = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = dtable[(uint8_t)str[i]];
        if (c == 0x80) continue;
        buf = (buf << 6) | c;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((buf >> bits) & 0xFF);
        }
    }
    return out;
}

namespace mt2mp::net {

// Chunk size: ~5KB raw → ~6.7KB base64 (fits in 8KB lobby data value limit)
constexpr int STREAM_CHUNK_RAW = 5000;

SaveTransfer& SaveTransfer::instance() {
    static SaveTransfer inst;
    return inst;
}

bool SaveTransfer::send_save(uint64_t lobby_id, const std::string& game_name,
                              const uint8_t* data, uint32_t size) {
    send_buffer_.assign(data, data + size);
    send_lobby_ = lobby_id;
    send_transfer_id_++;
    send_chunk_count_ = static_cast<uint16_t>((size + STREAM_CHUNK_RAW - 1) / STREAM_CHUNK_RAW);
    send_chunk_sent_ = 0;
    sending_ = true;

    // Write metadata to lobby data
    auto& mm = SteamMatchmaking::instance();
    mm.set_lobby_data("st_name", game_name.c_str());

    char meta[64];
    snprintf(meta, sizeof(meta), "%u,%u,%u", send_transfer_id_, size, (uint32_t)send_chunk_count_);
    mm.set_lobby_data("st_meta", meta);
    mm.set_lobby_data("st_idx", "0");
    mm.set_lobby_data("st_data", ""); // clear

    LOG_NET_INFO("Save stream start: '{}' ({} bytes, {} chunks)", game_name, size, send_chunk_count_);
    return true;
}

void SaveTransfer::tick() {
    // === SENDER: write one chunk at a time, wait for joiner ACK ===
    static auto last_send = std::chrono::steady_clock::now();
    auto now_send = std::chrono::steady_clock::now();
    float send_dt = std::chrono::duration<float>(now_send - last_send).count();

    if (sending_ && send_chunk_sent_ < send_chunk_count_ && send_dt >= 0.1f) {
        // Check if joiner has ACKed the previous chunk
        auto& mm = SteamMatchmaking::instance();
        auto& net = SteamNetworking::instance();
        uint64_t lobby = mm.current_lobby_id();
        int member_count = net.get_lobby_member_count(lobby);
        uint64_t local_id = net.local_steam_id();

        bool can_send = true;
        if (send_chunk_sent_ > 0 && member_count > 1) {
            // Check if ALL joiners have ACKed the last chunk
            for (int i = 0; i < member_count; ++i) {
                uint64_t member = net.get_lobby_member_by_index(lobby, i);
                if (member == local_id || member == 0) continue;
                const char* ack = net.read_member_data(lobby, member, "st_ack");
                int acked = ack[0] ? atoi(ack) : -1;
                if (acked < static_cast<int>(send_chunk_sent_) - 1) {
                    can_send = false; // joiner hasn't read previous chunk yet
                    break;
                }
            }
        }

        if (!can_send) return; // wait for joiner to catch up
        last_send = now_send;
        uint32_t offset = send_chunk_sent_ * STREAM_CHUNK_RAW;
        uint32_t remaining = static_cast<uint32_t>(send_buffer_.size()) - offset;
        uint32_t chunk_size = std::min((uint32_t)STREAM_CHUNK_RAW, remaining);

        std::string b64 = base64_encode(send_buffer_.data() + offset, chunk_size);

        char idx_str[16];
        snprintf(idx_str, sizeof(idx_str), "%u", send_chunk_sent_);
        mm.set_lobby_data("st_idx", idx_str);
        mm.set_lobby_data("st_data", b64.c_str());

        send_chunk_sent_++;

        if (send_chunk_sent_ % 5 == 0 || send_chunk_sent_ == send_chunk_count_) {
            LOG_NET_INFO("Stream sent chunk {}/{} ({} bytes b64)", send_chunk_sent_, send_chunk_count_, b64.size());
        }

        if (send_chunk_sent_ >= send_chunk_count_) {
            mm.set_lobby_data("st_idx", "done");
            LOG_NET_INFO("Save stream complete ({} chunks)", send_chunk_count_);
            sending_ = false;
        }
    }

    // === RECEIVER: poll lobby data for chunks ===
    auto& mm = SteamMatchmaking::instance();
    if (!mm.in_lobby()) return;

    // Check if we're the host (hosts don't receive their own saves)
    auto local_id = SteamNetworking::instance().local_steam_id();
    auto owner = SteamNetworking::instance().get_lobby_owner(mm.current_lobby_id());
    if (local_id == owner) return; // host doesn't receive

    std::string meta = mm.get_lobby_data("st_meta");
    if (meta.empty()) return;

    // Parse metadata: "transfer_id,total_size,chunk_count"
    uint32_t tid = 0, tsize = 0, tcount = 0;
    sscanf(meta.c_str(), "%u,%u,%u", &tid, &tsize, &tcount);

    if (tid == 0 || tsize == 0 || tcount == 0) return;

    // New transfer?
    if (tid != recv_transfer_id_) {
        recv_transfer_id_ = tid;
        recv_chunk_count_ = static_cast<uint16_t>(tcount);
        recv_chunks_got_ = 0;
        recv_buffer_.clear();
        recv_buffer_.resize(tsize, 0);
        receiving_ = true;
        recv_game_name_ = mm.get_lobby_data("st_name");
        LOG_NET_INFO("Receiving save '{}' ({} bytes, {} chunks)", recv_game_name_, tsize, tcount);
    }

    if (!receiving_) return;

    // Check current chunk index
    std::string idx_str = mm.get_lobby_data("st_idx");
    if (idx_str.empty()) return;

    if (idx_str == "done") {
        // Host finished sending. Accept whatever we have.
        // Some chunks may have been skipped due to polling timing.
        LOG_NET_INFO("Host signaled done. Got {}/{} chunks for '{}'",
            recv_chunks_got_, recv_chunk_count_, recv_game_name_);

        if (recv_chunks_got_ > 0) {
            LOG_NET_INFO("Save received: '{}' ({} bytes, {}/{} chunks)",
                recv_game_name_, recv_buffer_.size(), recv_chunks_got_, recv_chunk_count_);
            receiving_ = false;
            if (on_received_) {
                on_received_(recv_game_name_, recv_buffer_);
            }
        }
        return;
    }

    uint32_t current_idx = static_cast<uint32_t>(atoi(idx_str.c_str()));

    // Already got this chunk?
    if (current_idx < recv_chunks_got_) return;

    // Read chunk data
    std::string b64_data = mm.get_lobby_data("st_data");
    if (b64_data.empty()) return;

    auto decoded = base64_decode(b64_data.c_str(), b64_data.size());
    if (decoded.empty()) return;

    // Write to buffer
    uint32_t offset = current_idx * STREAM_CHUNK_RAW;
    uint32_t copy_size = std::min(static_cast<uint32_t>(decoded.size()),
                                   static_cast<uint32_t>(recv_buffer_.size()) - offset);
    if (offset + copy_size <= recv_buffer_.size()) {
        memcpy(recv_buffer_.data() + offset, decoded.data(), copy_size);
        recv_chunks_got_ = static_cast<uint16_t>(current_idx + 1);

        // ACK this chunk so host knows we read it
        {
            auto& mm_ack = SteamMatchmaking::instance();
            char ack_str[16];
            snprintf(ack_str, sizeof(ack_str), "%u", current_idx);
            SteamNetworking::instance().write_member_data(mm_ack.current_lobby_id(), "st_ack", ack_str);
        }

        if (recv_chunks_got_ % 10 == 0 || recv_chunks_got_ == recv_chunk_count_) {
            LOG_NET_INFO("Stream received chunk {}/{}", recv_chunks_got_, recv_chunk_count_);
        }

        // Check if complete (all chunks received)
        if (recv_chunks_got_ >= recv_chunk_count_) {
            LOG_NET_INFO("Save received (all chunks): '{}' ({} bytes)", recv_game_name_, recv_buffer_.size());
            receiving_ = false;
            if (on_received_) {
                on_received_(recv_game_name_, recv_buffer_);
            }
            return; // don't process further
        }
    }
}

float SaveTransfer::send_progress() const {
    if (!sending_ || send_chunk_count_ == 0) return 0;
    return static_cast<float>(send_chunk_sent_) / send_chunk_count_;
}

float SaveTransfer::receive_progress() const {
    if (!receiving_ || recv_chunk_count_ == 0) return 0;
    return static_cast<float>(recv_chunks_got_) / recv_chunk_count_;
}

void SaveTransfer::set_receive_callback(SaveReceivedCallback cb) {
    on_received_ = std::move(cb);
}

// Legacy interface (not used in lobby data mode)
void SaveTransfer::on_chat_message(uint64_t, int) {}

} // namespace mt2mp::net

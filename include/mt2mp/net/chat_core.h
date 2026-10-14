#pragma once

#include "mt2mp/net/chat_protocol.h"
#include <array>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace mt2mp::net::chat {

enum class Delivery { Queued, Confirmed, Failed, Received };
struct Entry {
    uint8_t origin = 0;
    uint64_t sequence = 0;
    std::string text;
    Delivery delivery = Delivery::Queued;
    uint8_t expected_mask = 0;
    uint8_t ack_mask = 0;
    std::chrono::steady_clock::time_point sent_at{};
};
using Send = std::function<bool(const uint8_t*, uint32_t, uint64_t)>;

class Core {
public:
    void configure(uint64_t session, uint8_t local_peer, uint8_t peer_mask);
    bool submit(const std::string& text, std::string& error, uint64_t* sequence = nullptr);
    bool tick(const Send& send, std::string& error,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    bool receive(uint8_t sender, const uint8_t* bytes, uint32_t size, const Send& send, std::string& error);
    const std::vector<Entry>& entries() const { return entries_; }
    bool connected() const {
        return session_ && local_ && (local_ == 1 ? (mask_ & 0x0e) != 0 : (mask_ & 1) != 0);
    }

private:
    bool send_packet(const Packet& packet, uint8_t target, const Send& send, std::string& error);
    void append(Entry entry);

    uint64_t session_ = 0;
    uint64_t next_sequence_ = 0;
    uint8_t local_ = 0;
    uint8_t mask_ = 0;
    std::array<uint64_t, 5> last_received_{};
    std::vector<Entry> entries_;
    size_t next_unsent_ = 0;
};

} // namespace mt2mp::net::chat

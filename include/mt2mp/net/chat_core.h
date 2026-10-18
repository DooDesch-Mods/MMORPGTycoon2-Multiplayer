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
    uint64_t host_order = 0;
    uint64_t origin_admission = 0;
    bool completion_sent = false;
};
using Send = std::function<bool(const uint8_t*, uint32_t, uint64_t)>;
class Core {
public:
    void configure(uint64_t session, uint8_t local_peer, uint8_t peer_mask, uint64_t admission);
    bool retire(const Send& send, std::string& error);
    bool submit(const std::string& text, std::string& error, uint64_t* sequence = nullptr);
    bool tick(const Send& send, std::string& error,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    bool receive(uint8_t sender, const uint8_t* bytes, uint32_t size, const Send& send, std::string& error);
    const std::vector<Entry>& entries() const { return entries_; }
    bool connected() const;
    uint64_t conversation() const { return conversation_; }
    bool retired() const { return blocked_; }
private:
    bool send_packet(Packet packet, uint8_t target, const Send& send, std::string& error);
    bool room(std::string& error, bool incoming = false);
    void sort_entries();
    void fail_pending();
    bool pending(const Entry& entry) const;
    bool commit(Entry& entry, const Send& send, std::string& error,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    bool complete(Entry& entry, const Send& send, std::string& error);
    bool depart(uint8_t peer, const Send& send, std::string& error);
    Packet message(Kind kind, const Entry& entry) const;
    uint64_t session_ = 0, admission_ = 0, conversation_ = 0, last_conversation_ = 0;
    uint64_t next_sequence_ = 0, host_order_ = 0;
    uint8_t local_ = 0, mask_ = 0, ready_mask_ = 0;
    bool blocked_ = false;
    std::array<uint64_t, 5> peer_challenge_{}, peer_baseline_{}, last_request_{};
    std::vector<Entry> entries_;
    std::string pending_error_;
    std::chrono::steady_clock::time_point last_hello_{};
};
} // namespace mt2mp::net::chat

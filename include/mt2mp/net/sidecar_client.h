#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace mt2mp::net {
enum class SidecarChannel : uint8_t { Save = 1, Cursor = 2, State = 3 };
using SidecarHandler = std::function<void(uint64_t sender, const uint8_t* data, uint32_t size)>;
class SidecarClient {
public:
    static SidecarClient& instance();
    bool init(uint64_t session_id, uint64_t peer_id, bool is_host);
    void shutdown();
    void tick();
    bool connected() const { return connected_.load(); }
    bool ready() const { return ready_.load(); }
    std::string failure_reason() const;
    bool send(uint8_t channel, const uint8_t* data, uint32_t size, uint64_t target = 0);
    void register_handler(uint8_t channel, SidecarHandler handler);
    bool claim_handler(uint8_t channel, SidecarHandler handler, std::string& error);
    bool retire_action_handler(std::string& error);
    void register_session_handler(SidecarHandler handler, std::function<void()> maintenance);
    void request_disconnect(const std::string& reason);
private:
    SidecarClient() = default;
    ~SidecarClient() = default;
    bool connect_control_socket();
    void worker_loop();
    void poll_socket();
    void dispatch_frame(uint8_t type, uint8_t channel, const uint8_t* payload, uint32_t size);
    void close_socket();
    bool queue_packet(std::vector<uint8_t> packet, bool priority);
    struct PendingFrame { uint64_t sender; uint8_t channel; std::vector<uint8_t> payload; };
    uintptr_t sock_ = ~uintptr_t(0);
    bool winsock_ready_ = false;
    std::atomic<bool> connected_{false}, ready_{false}, running_{false};
    std::atomic<bool> disconnect_requested_{false}, failure_recorded_{false};
    std::atomic<int64_t> last_main_tick_ms_{0};
    std::thread worker_;
    std::mutex wait_mutex_;
    std::condition_variable wake_;
    mutable std::mutex failure_mutex_;
    std::string failure_reason_;
    std::mutex queue_mutex_, handler_mutex_;
    std::deque<std::vector<uint8_t>> priority_tx_, pending_tx_;
    std::deque<PendingFrame> pending_rx_;
    size_t queued_tx_bytes_ = 0, queued_rx_bytes_ = 0;
    SidecarHandler session_handler_;
    std::function<void()> maintenance_;
    std::vector<uint8_t> rx_buffer_, tx_buffer_;
    size_t tx_offset_ = 0;
    std::thread::id action_owner_{};std::atomic<bool> action_dispatching_{false};
    std::unordered_map<uint8_t, SidecarHandler> handlers_;
};
} // namespace mt2mp::net

#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/net/relay_protocol.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <cstring>

namespace mt2mp::net {
using namespace relay;
namespace {
constexpr size_t max_queue = 8 * 1024 * 1024;
constexpr size_t max_pending_frames = 4096;
int64_t wall_ms() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
}
SidecarClient& SidecarClient::instance() {
    // Process detach cannot join or lock a worker under the DLL loader lock.
    static auto* client = new SidecarClient;
    return *client;
}
bool SidecarClient::init(uint64_t session, uint64_t peer, bool host) {
    if (worker_.joinable()) { LOG_NET_ERROR("[LOCAL] Transport initialization refused: previous worker requires explicit shutdown; no reconnect"); return false; }
    if (!winsock_ready_) {
        WSADATA w{}; int code = WSAStartup(MAKEWORD(2,2), &w);
        if (code) { LOG_NET_ERROR("[LOCAL] WSAStartup failed: {}", code); return false; }
        winsock_ready_ = true;
    }
    if (!Config::instance().local_test) { LOG_NET_ERROR("[LOCAL] Transport requires a launcher-owned relay"); return false; }
    const auto& token = Config::instance().session_token;
    if (!session || peer < 1 || peer > 4 || host != (peer == 1) || token.size() != sizeof(Identity::token)) {
        LOG_NET_ERROR("[LOCAL] Transport identity invalid: peer={} host={} token_bytes={}", peer, host, token.size()); return false;
    }
    if (!connect_control_socket()) return false;
    Identity identity{}; identity.session = session; identity.peer = peer; identity.host = host;
    memcpy(identity.token, token.data(), token.size());
    failure_recorded_ = false; disconnect_requested_ = false;
    { std::lock_guard lock(failure_mutex_); failure_reason_.clear(); }
    connected_ = true; ready_ = false;
    if (!queue_packet(frame(Type::Hello, 0, &identity, sizeof(identity)), true)) { close_socket(); return false; }
    last_main_tick_ms_ = wall_ms(); running_ = true;
    try { worker_ = std::thread(&SidecarClient::worker_loop, this); }
    catch (const std::exception& error) {
        running_ = false; close_socket();
        LOG_NET_ERROR("[LOCAL] Transport worker creation failed: {}", error.what()); return false;
    }
    LOG_NET_INFO("[LOCAL] Control connected: peer={} session={}", peer, session);
    return true;
}
void SidecarClient::shutdown() {
    running_ = false; wake_.notify_all();
    if (worker_.joinable()) {
        if (worker_.get_id() == std::this_thread::get_id()) {
            LOG_NET_ERROR("[LOCAL] Transport shutdown requested from worker; owner must join before destruction"); return;
        }
        worker_.join();
    }
    close_socket(); rx_buffer_.clear(); tx_buffer_.clear(); tx_offset_ = 0;
    { std::lock_guard lock(queue_mutex_); priority_tx_.clear(); pending_tx_.clear(); pending_rx_.clear(); queued_tx_bytes_ = queued_rx_bytes_ = 0; }
}
std::string SidecarClient::failure_reason() const { std::lock_guard lock(failure_mutex_); return failure_reason_; }
void SidecarClient::request_disconnect(const std::string& reason) {
    if (!failure_recorded_.exchange(true)) {
        { std::lock_guard lock(failure_mutex_); failure_reason_ = reason; }
        LOG_NET_ERROR("[LOCAL] Transport stopped: {}", reason);
    }
    ready_ = false; connected_ = false; disconnect_requested_ = true; wake_.notify_all();
}
void SidecarClient::worker_loop() {
    LOG_NET_INFO("[LOCAL] TRANSPORT_WORKER_STARTED thread={}", GetCurrentThreadId());
    bool dispatch_delayed = false;
    try {
        while (running_) {
            if (disconnect_requested_) close_socket();
            if (connected_) poll_socket();
            std::function<void()> maintenance;
            { std::lock_guard lock(handler_mutex_); maintenance = maintenance_; }
            if (maintenance) maintenance();
            if (!connected_) break;
            const auto age = wall_ms() - last_main_tick_ms_.load();
            if (age > 10000 && !dispatch_delayed) {
                size_t frames, bytes;
                { std::lock_guard lock(queue_mutex_); frames = pending_rx_.size(); bytes = queued_rx_bytes_; }
                LOG_NET_WARN("[LOCAL] MAINTHREAD_DISPATCH_DELAY age_ms={} pending_frames={} pending_bytes={}; transport worker remains active", age, frames, bytes);
                dispatch_delayed = true;
            } else if (age <= 10000 && dispatch_delayed) {
                LOG_NET_INFO("[LOCAL] MAINTHREAD_DISPATCH_RECOVERED age_ms={}", age); dispatch_delayed = false;
            }
            std::unique_lock lock(wait_mutex_);
            wake_.wait_for(lock, std::chrono::milliseconds(20), [this] { return !running_ || disconnect_requested_; });
        }
    } catch (const std::exception& error) {
        request_disconnect(std::string("transport worker exception: ") + error.what());
        std::function<void()> maintenance;
        { std::lock_guard lock(handler_mutex_); maintenance = maintenance_; }
        if (maintenance) {
            try { maintenance(); }
            catch (const std::exception& nested) { LOG_NET_ERROR("[LOCAL] Session failure publication failed: {}; original={}", nested.what(), error.what()); }
        }
    }
    close_socket(); running_ = false;
    LOG_NET_INFO("[LOCAL] TRANSPORT_WORKER_STOPPED thread={}", GetCurrentThreadId());
}
void SidecarClient::tick() {
    last_main_tick_ms_ = wall_ms();
    for (size_t dispatched = 0; dispatched < 64; ++dispatched) {
        PendingFrame pending;
        {
            std::lock_guard lock(queue_mutex_);
            if (pending_rx_.empty()) break;
            pending = std::move(pending_rx_.front()); pending_rx_.pop_front(); queued_rx_bytes_ -= pending.payload.size();
        }
        SidecarHandler handler;
        { std::lock_guard lock(handler_mutex_); auto found = handlers_.find(pending.channel); if (found != handlers_.end()) handler = found->second; }
        if (handler) {
            try {
                if(pending.channel==9)action_dispatching_=true;
                handler(pending.sender, pending.payload.data(), uint32_t(pending.payload.size()));
                if(pending.channel==9)action_dispatching_=false;
            }
            catch (const std::exception& error) {
                if(pending.channel==9)action_dispatching_=false;
                request_disconnect("mainthread handler failed channel=" + std::to_string(pending.channel) + " sender=" + std::to_string(pending.sender) + " cause=" + error.what()); break;
            }
        }
    }
}
bool SidecarClient::queue_packet(std::vector<uint8_t> packet, bool priority) {
    if (packet.empty()) { request_disconnect("invalid outbound relay frame"); return false; }
    {
        std::lock_guard lock(queue_mutex_);
        if (queued_tx_bytes_ + packet.size() > max_queue) {
            request_disconnect("outbound queue limit actual=" + std::to_string(queued_tx_bytes_ + packet.size()) + " required<=" + std::to_string(max_queue)); return false;
        }
        queued_tx_bytes_ += packet.size();
        (priority ? priority_tx_ : pending_tx_).push_back(std::move(packet));
    }
    wake_.notify_all(); return true;
}
bool SidecarClient::send(uint8_t channel, const uint8_t* data, uint32_t size, uint64_t target) {
    if (!ready_ || size > max_payload - sizeof(Address) || (size && !data)) return false;
    Address address{target, 0}; std::vector<uint8_t> payload(sizeof(address) + size);
    memcpy(payload.data(), &address, sizeof(address));
    if (size) memcpy(payload.data() + sizeof(address), data, size);
    return queue_packet(frame(Type::Data, channel, payload.data(), uint32_t(payload.size())), channel == 3);
}
void SidecarClient::register_handler(uint8_t channel, SidecarHandler handler) {
    if (channel == 3) { LOG_NET_ERROR("[LOCAL] Session channel requires worker-safe session registration"); return; }
    std::lock_guard lock(handler_mutex_); handlers_[channel] = std::move(handler);
}
bool SidecarClient::claim_handler(uint8_t channel, SidecarHandler handler, std::string& error) {
    if(channel==3||!handler) { error="exclusive channel claim requires a non-session handler"; return false; }
    std::lock_guard lock(handler_mutex_);
    if(handlers_.contains(channel)) { error="channel already claimed: "+std::to_string(channel); return false; }
    if(channel==9)action_owner_=std::this_thread::get_id();
    handlers_.emplace(channel,std::move(handler));error.clear();return true;
}
bool SidecarClient::retire_action_handler(std::string& error){
    std::scoped_lock lock(handler_mutex_,queue_mutex_);
    if(action_dispatching_||(action_owner_!=std::thread::id{}&&action_owner_!=std::this_thread::get_id())){error="local action retirement requires idle channel owner thread";return false;}
    handlers_.erase(9);action_owner_={};
    for(auto it=pending_rx_.begin();it!=pending_rx_.end();){
        if(it->channel==9){queued_rx_bytes_-=it->payload.size();it=pending_rx_.erase(it);}else ++it;
    }
    error.clear();return true;
}
void SidecarClient::register_session_handler(SidecarHandler handler, std::function<void()> maintenance) {
    std::lock_guard lock(handler_mutex_); session_handler_ = std::move(handler); maintenance_ = std::move(maintenance);
}
bool SidecarClient::connect_control_socket() {
    close_socket();
    SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_handle == INVALID_SOCKET) { LOG_NET_ERROR("[LOCAL] socket failed: {}", WSAGetLastError()); return false; }
    sockaddr_in address{}; address.sin_family = AF_INET;
    address.sin_port = htons(Config::instance().sidecar_control_port); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(socket_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        LOG_NET_ERROR("[LOCAL] Control connect port={} failed: {}", Config::instance().sidecar_control_port, WSAGetLastError()); closesocket(socket_handle); return false;
    }
    u_long nonblocking = 1;
    if (ioctlsocket(socket_handle, FIONBIO, &nonblocking) != 0) { LOG_NET_ERROR("[LOCAL] Nonblocking failed: {}", WSAGetLastError()); closesocket(socket_handle); return false; }
    sock_ = uintptr_t(socket_handle); return true;
}
void SidecarClient::poll_socket() {
    const SOCKET socket_handle = SOCKET(sock_);
    for (size_t packets = 0; packets < 32; ++packets) {
        if (tx_buffer_.empty()) {
            std::lock_guard lock(queue_mutex_);
            auto& queue = priority_tx_.empty() ? pending_tx_ : priority_tx_;
            if (queue.empty()) break;
            tx_buffer_ = std::move(queue.front()); queue.pop_front(); tx_offset_ = 0;
        }
        int count = ::send(socket_handle, reinterpret_cast<const char*>(tx_buffer_.data() + tx_offset_), int(tx_buffer_.size() - tx_offset_), 0);
        if (count > 0) {
            tx_offset_ += count;
            { std::lock_guard lock(queue_mutex_); queued_tx_bytes_ -= size_t(count); }
            if (tx_offset_ == tx_buffer_.size()) { tx_buffer_.clear(); tx_offset_ = 0; }
            continue;
        }
        const int code = count < 0 ? WSAGetLastError() : 0;
        if (count < 0 && code == WSAEWOULDBLOCK) break;
        request_disconnect("socket send failed WSA=" + std::to_string(code)); return;
    }
    for (int reads = 0; reads < 32; ++reads) {
        char buffer[16384]; const int count = recv(socket_handle, buffer, sizeof(buffer), 0);
        if (count == 0) { request_disconnect("relay TCP EOF"); return; }
        if (count < 0) {
            const int code = WSAGetLastError(); if (code == WSAEWOULDBLOCK) break;
            request_disconnect("socket receive failed WSA=" + std::to_string(code)); return;
        }
        rx_buffer_.insert(rx_buffer_.end(), buffer, buffer + count);
        Header header{}; std::vector<uint8_t> payload;
        for (;;) {
            const auto parsed = pop(rx_buffer_, header, payload);
            if (parsed == Parse::Incomplete) break;
            if (parsed == Parse::Invalid) { request_disconnect("invalid relay frame"); return; }
            dispatch_frame(uint8_t(header.type), header.channel, payload.data(), uint32_t(payload.size()));
            if (!connected_) return;
        }
        if (rx_buffer_.size() > max_payload + sizeof(Header)) { request_disconnect("inbound frame exceeds protocol limit"); return; }
    }
}
void SidecarClient::dispatch_frame(uint8_t type, uint8_t channel, const uint8_t* payload, uint32_t size) {
    if (type == uint8_t(Type::Ready)) {
        if (size) { request_disconnect("nonempty relay authentication acknowledgement"); return; }
        ready_ = true; LOG_NET_INFO("[LOCAL] RELAY_AUTHENTICATED"); return;
    }
    if (type == uint8_t(Type::Error)) { request_disconnect("relay rejection: " + std::string(reinterpret_cast<const char*>(payload), size)); return; }
    if (type != uint8_t(Type::Data)) return;
    if (!ready_ || size < sizeof(Address)) { request_disconnect("relay data before authentication or truncated address"); return; }
    Address address{}; memcpy(&address, payload, sizeof(address));
    if (address.sender < 1 || address.sender > 4) { request_disconnect("invalid relay sender=" + std::to_string(address.sender)); return; }
    if (channel == 3) {
        SidecarHandler handler;
        { std::lock_guard lock(handler_mutex_); handler = session_handler_; }
        if (handler) handler(address.sender, payload + sizeof(address), size - sizeof(address));
        else request_disconnect("session control handler missing");
        return;
    }
    const size_t bytes = size - sizeof(Address);
    std::lock_guard lock(queue_mutex_);
    if (queued_rx_bytes_ + bytes > max_queue || pending_rx_.size() >= max_pending_frames) {
        request_disconnect("mainthread receive queue limit bytes=" + std::to_string(queued_rx_bytes_ + bytes) + " required<=" + std::to_string(max_queue) + " frames=" + std::to_string(pending_rx_.size()) + " required<" + std::to_string(max_pending_frames)); return;
    }
    pending_rx_.push_back({address.sender, channel, std::vector<uint8_t>(payload + sizeof(address), payload + size)});
    queued_rx_bytes_ += bytes;
}
void SidecarClient::close_socket() {
    if (SOCKET(sock_) != INVALID_SOCKET && closesocket(SOCKET(sock_))) LOG_NET_ERROR("[LOCAL] Close control socket failed: WSA={}", WSAGetLastError());
    sock_ = ~uintptr_t(0); connected_ = false; ready_ = false;
}
} // namespace mt2mp::net

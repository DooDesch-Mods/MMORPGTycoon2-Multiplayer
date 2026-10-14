#include "mt2mp/net/chat_runtime.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::net::chat {
namespace {
bool send(const uint8_t* bytes, uint32_t size, uint64_t target) {
    return session_wire::send(channel, bytes, size, target);
}
}

Runtime& Runtime::instance() {
    static Runtime runtime;
    return runtime;
}

bool Runtime::initialize(std::string& error) {
    if (initialized_) { error.clear(); return true; }
    if (!session_wire::claim_handler(channel,
        [this](uint64_t sender, const uint8_t* bytes, uint32_t size) { receive(sender, bytes, size); }, error)) return false;
    initialized_ = true;
    LOG_NET_INFO("[Chat] Kanal 11 für Sitzungschat registriert");
    return true;
}

void Runtime::tick() {
    if (!initialized_) return;
    const bool connected = session_wire::connected();
    const uint64_t session = connected ? session_wire::session_id() : 0;
    const auto local = connected ? uint8_t(session_wire::local_peer()) : uint8_t(0);
    const auto mask = connected ? session_wire::peer_mask() : uint8_t(0);
    std::string error;
    {
        std::lock_guard lock(mutex_);
        core_.configure(session, local, mask);
        if (connected) { session_ = session; local_ = local; }
        if (!core_.tick(send, error)) LOG_NET_ERROR("[Chat] Senden fehlgeschlagen: {}", error);
    }
}

void Runtime::receive(uint64_t sender, const uint8_t* bytes, uint32_t size) {
    const bool connected = session_wire::connected();
    const uint64_t session = connected ? session_wire::session_id() : 0;
    const auto local = connected ? uint8_t(session_wire::local_peer()) : uint8_t(0);
    const auto mask = connected ? session_wire::peer_mask() : uint8_t(0);
    std::string error;
    {
        std::lock_guard lock(mutex_);
        core_.configure(session, local, mask);
        if (connected) { session_ = session; local_ = local; }
        if (!core_.receive(uint8_t(sender), bytes, size, send, error)) {
            LOG_NET_WARN("[Chat] Paket abgelehnt physischer_peer={} grund={}", sender, error);
        }
    }
}

bool Runtime::submit(const std::string& text, std::string& error, uint64_t* sequence) {
    std::lock_guard lock(mutex_);
    if (!initialized_) { error = "Chat: Initialisierung ausstehend"; return false; }
    return core_.submit(text, error, sequence);
}

Snapshot Runtime::snapshot() const {
    std::lock_guard lock(mutex_);
    return {initialized_ && core_.connected(), session_, local_, core_.entries()};
}

} // namespace mt2mp::net::chat

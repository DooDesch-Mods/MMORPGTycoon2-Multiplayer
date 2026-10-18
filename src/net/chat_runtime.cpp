#include "mt2mp/net/chat_runtime.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/core/logger.h"
#include <limits>

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

void Runtime::set_lobby_admitted(bool admitted) {
    std::lock_guard lock(mutex_);
    if (admitted_ == admitted) return;
    if (!admitted) {
        admitted_ = false;
        std::string error;
        if (!core_.retire(send, error)) LOG_NET_ERROR("[Chat] Lobby retirement failed: {}", error);
        return;
    }
    if (admission_ == std::numeric_limits<uint64_t>::max()) {
        LOG_NET_ERROR("[Chat] Lobby admission counter exhausted"); return;
    }
    ++admission_; admitted_ = true;
}

void Runtime::tick() {
    if (!initialized_) return;
    std::lock_guard lock(mutex_);
    std::string error;
    // Preparation publishes Host metadata before a world exists. SessionWire
    // retains and logs its exact failures and ordinary admission waits.
    const bool prepared = !admitted_ || session_wire::prepare(error);
    const bool connected = prepared && session_wire::connected();
    const uint64_t session = connected ? session_wire::session_id() : 0;
    const auto local = connected ? uint8_t(session_wire::local_peer()) : uint8_t(0);
    const auto mask = connected ? session_wire::peer_mask() : uint8_t(0);
    core_.configure(session, local, mask, admitted_ ? admission_ : 0);
    if (connected) { session_ = session; local_ = local; }
    if (!core_.tick(send, error)) LOG_NET_ERROR("[Chat] Senden fehlgeschlagen: {}", error);
}

void Runtime::receive(uint64_t sender, const uint8_t* bytes, uint32_t size) {
    if (sender < 1 || sender > 4) {
        LOG_NET_WARN("[Chat] Paket abgelehnt physischer_peer={} grund=peer outside admitted slots", sender);
        return;
    }
    const bool connected = session_wire::connected();
    const uint64_t session = connected ? session_wire::session_id() : 0;
    const auto local = connected ? uint8_t(session_wire::local_peer()) : uint8_t(0);
    const auto mask = connected ? session_wire::peer_mask() : uint8_t(0);
    std::string error;
    {
        std::lock_guard lock(mutex_);
        core_.configure(session, local, mask, admitted_ ? admission_ : 0);
        if (connected) { session_ = session; local_ = local; }
        if (!core_.receive(uint8_t(sender), bytes, size, send, error)) {
            LOG_NET_WARN("[Chat] Paket abgelehnt physischer_peer={} grund={}", sender, error);
        }
    }
}

bool Runtime::submit(const std::string& text, std::string& error, uint64_t* sequence) {
    std::lock_guard lock(mutex_);
    if (!initialized_) { error = "Chat: Initialisierung ausstehend"; return false; }
    const bool connected = admitted_ && session_wire::connected();
    core_.configure(connected ? session_wire::session_id() : 0,
        connected ? uint8_t(session_wire::local_peer()) : uint8_t(0),
        connected ? session_wire::peer_mask() : uint8_t(0), admitted_ ? admission_ : 0);
    return core_.submit(text, error, sequence);
}

Snapshot Runtime::snapshot() const {
    std::lock_guard lock(mutex_);
    return {initialized_ && admitted_ && core_.connected(), session_, local_, core_.entries(), core_.conversation(), core_.retired()};
}

} // namespace mt2mp::net::chat

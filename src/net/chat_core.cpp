#include "mt2mp/net/chat_core.h"

#include <algorithm>

namespace mt2mp::net::chat {

void Core::configure(uint64_t session, uint8_t local_peer, uint8_t peer_mask) {
    if (!session || local_peer < 1 || local_peer > 4 || !(peer_mask & (1u << (local_peer - 1)))) {
        mask_ = 0; return;
    }
    if (session_ != session || local_ != local_peer) {
        session_ = session; local_ = local_peer; next_sequence_ = 0;
        last_received_ = {}; entries_.clear(); next_unsent_ = 0;
    }
    mask_ = peer_mask;
}

void Core::append(Entry entry) {
    if (entries_.size() == 100) {
        entries_.erase(entries_.begin());
        if (next_unsent_) --next_unsent_;
    }
    entries_.push_back(std::move(entry));
}

bool Core::submit(const std::string& text, std::string& error, uint64_t* sequence) {
    if (!connected()) { error = "Chat: keine verbundene Gegenstelle"; return false; }
    if (!valid_text(text)) { error = "Chat: Text muss 1 bis 240 gültige UTF-8-Bytes ohne Steuerzeichen enthalten"; return false; }
    if (next_unsent_ < entries_.size() && entries_.size() - next_unsent_ >= 16) {
        error = "Chat: Sendewarteschlange voll"; return false;
    }
    append({local_, ++next_sequence_, text, Delivery::Queued, 0, 0});
    if (sequence) *sequence = next_sequence_;
    error.clear(); return true;
}

bool Core::send_packet(const Packet& packet, uint8_t target, const Send& send, std::string& error) {
    std::vector<uint8_t> bytes;
    if (!encode(packet, bytes, error)) return false;
    if (!send(bytes.data(), uint32_t(bytes.size()), target)) {
        error = "Chat: Transport hat Paket für Peer " + std::to_string(target) + " abgelehnt"; return false;
    }
    return true;
}

bool Core::tick(const Send& send, std::string& error, std::chrono::steady_clock::time_point now) {
    if (!connected()) {
        size_t failed = 0;
        for (Entry& entry : entries_) {
            if (entry.origin == local_ && entry.delivery == Delivery::Queued) {
                entry.delivery = Delivery::Failed; ++failed;
            }
        }
        if (failed) {
            error = "Chat: Verbindung vor Empfangsbestätigung verloren Nachrichten=" + std::to_string(failed);
            return false;
        }
        error.clear(); return true;
    }
    for (Entry& entry : entries_) {
        if (entry.origin == local_ && entry.delivery == Delivery::Queued && entry.expected_mask &&
            entry.sent_at != std::chrono::steady_clock::time_point{} &&
            now - entry.sent_at > std::chrono::seconds(10)) {
            entry.delivery = Delivery::Failed;
            error = "Chat: Empfangsbestätigung nach 10 Sekunden ausgeblieben Peer-Maske=" +
                std::to_string(entry.ack_mask) + " erforderlich=" + std::to_string(entry.expected_mask);
            return false;
        }
    }
    while (next_unsent_ < entries_.size()) {
        Entry& entry = entries_[next_unsent_++];
        if (entry.origin != local_ || entry.delivery != Delivery::Queued) continue;
        entry.expected_mask = local_ == 1 ? uint8_t(mask_ & ~1u) : uint8_t(1);
        for (uint8_t peer = 1; peer <= 4; ++peer) {
            const uint8_t bit = uint8_t(1u << (peer - 1));
            if (!(entry.expected_mask & bit)) continue;
            if (!send_packet({Kind::Message, session_, local_, entry.sequence, entry.text}, peer, send, error)) {
                entry.delivery = Delivery::Failed; return false;
            }
        }
        entry.sent_at = now;
    }
    error.clear(); return true;
}

bool Core::receive(uint8_t sender, const uint8_t* bytes, uint32_t size, const Send& send, std::string& error) {
    error.clear();
    Packet packet;
    if (!connected() || !decode(bytes, size, packet, error) || packet.session != session_ ||
        sender < 1 || sender > 4 || sender == local_ || !(mask_ & (1u << (sender - 1)))) {
        if (error.empty()) error = "Chat: Sitzung oder Absender ungültig";
        return false;
    }
    if (packet.kind == Kind::Ack) {
        if (packet.origin != local_) {
            if (local_ != 1 || packet.origin == sender || packet.origin < 2 || packet.origin > 4) {
                error = "Chat: Bestätigung für fremden Ursprung"; return false;
            }
            for (Entry& entry : entries_) {
                if (entry.origin == packet.origin && entry.sequence == packet.sequence &&
                    (entry.expected_mask & (1u << (sender - 1)))) {
                    entry.ack_mask |= uint8_t(1u << (sender - 1));
                    error.clear(); return true;
                }
            }
            error = "Chat: unbekannte Weiterleitungsbestätigung"; return false;
        }
        for (Entry& entry : entries_) {
            if (entry.origin == local_ && entry.sequence == packet.sequence && entry.delivery != Delivery::Failed &&
                (entry.expected_mask & (1u << (sender - 1)))) {
                entry.ack_mask |= uint8_t(1u << (sender - 1));
                if (entry.ack_mask == entry.expected_mask) entry.delivery = Delivery::Confirmed;
                error.clear(); return true;
            }
        }
        error = "Chat: unbekannte oder verspätete Bestätigung"; return false;
    }
    if (packet.origin == local_ || packet.origin < 1 || packet.origin > 4 ||
        (local_ == 1 && packet.origin != sender) || (local_ != 1 && sender != 1)) {
        error = "Chat: Ursprung stimmt nicht mit Hostroute überein"; return false;
    }
    if (packet.sequence > last_received_[packet.origin]) {
        last_received_[packet.origin] = packet.sequence;
        const uint8_t relay_mask = local_ == 1 ? uint8_t(mask_ & ~1u & ~(1u << (sender - 1))) : uint8_t(0);
        append({packet.origin, packet.sequence, packet.text, Delivery::Received, relay_mask, 0});
        if (local_ == 1) {
            for (uint8_t peer = 2; peer <= 4; ++peer) {
                if (peer == sender || !(mask_ & (1u << (peer - 1)))) continue;
                if (!send_packet(packet, peer, send, error)) return false;
            }
        }
    }
    if (!send_packet({Kind::Ack, session_, packet.origin, packet.sequence, {}}, sender, send, error)) return false;
    error.clear(); return true;
}

} // namespace mt2mp::net::chat

#include "mt2mp/net/chat_core.h"
#include <algorithm>
#include <limits>

namespace mt2mp::net::chat {
namespace { uint8_t bit(uint8_t peer) { return uint8_t(1u << (peer - 1)); } }

bool Core::connected() const {
    return admission_ && !blocked_ && conversation_ &&
        (local_ == 1 ? (ready_mask_ & 0x0e) != 0 : (ready_mask_ & 1) != 0);
}
bool Core::pending(const Entry& entry) const {
    return entry.delivery == Delivery::Queued ||
        (local_ == 1 && entry.host_order && !entry.completion_sent && entry.delivery != Delivery::Failed);
}
void Core::fail_pending() {
    for (auto& entry : entries_) if (pending(entry)) entry.delivery = Delivery::Failed;
}
void Core::configure(uint64_t session, uint8_t local, uint8_t mask, uint64_t admission) {
    if (!session || local < 1 || local > 4 || !(mask & bit(local)) || !admission) {
        if (admission_ && !blocked_) pending_error_ = "Chat: Lobby oder Verbindung nicht mehr verfügbar";
        fail_pending(); ready_mask_ = mask_ = 0; if (admission_) blocked_ = true; return;
    }
    const bool different_wire = session_ != session || local_ != local;
    if (different_wire || admission_ != admission) {
        fail_pending(); entries_.clear(); next_sequence_ = host_order_ = 0;
        last_request_ = {}; peer_baseline_ = {}; last_hello_ = {};
        if (different_wire) { peer_challenge_ = {}; last_conversation_ = 0; }
        session_ = session; local_ = local; admission_ = admission;
        conversation_ = local == 1 ? admission : 0;
        ready_mask_ = local == 1 ? uint8_t(1) : uint8_t(0); blocked_ = false;
        pending_error_.clear();
    }
    // Membership loss cannot silently shrink a message's required recipients.
    if (ready_mask_ & uint8_t(~mask)) {
        if (!blocked_) pending_error_ = "Chat: bestätigter Empfänger hat die Verbindung verlassen";
        fail_pending(); blocked_ = true;
    }
    mask_ = mask;
}
bool Core::send_packet(Packet packet, uint8_t target, const Send& send, std::string& error) {
    packet.session = session_;
    packet.challenge = local_ == 1 ? peer_challenge_[target] : admission_;
    std::vector<uint8_t> bytes;
    if (!encode(packet, bytes, error)) return false;
    if (!send(bytes.data(), uint32_t(bytes.size()), target)) {
        blocked_ = true; fail_pending();
        error = "Chat: Transport hat Paket für Peer " + std::to_string(target) + " abgelehnt";
        return false;
    }
    return true;
}
bool Core::retire(const Send& send, std::string& error) {
    error.clear(); std::string first;
    if (admission_ && conversation_) {
        Packet packet; packet.kind = local_ == 1 ? Kind::Retire : Kind::Leave;
        packet.origin = local_; packet.conversation = conversation_;
        for (uint8_t peer = 1; peer <= 4; ++peer) {
            if (peer == local_ || !(ready_mask_ & bit(peer))) continue;
            std::string cause;
            if (!send_packet(packet, peer, send, cause) && first.empty()) first = cause;
        }
    }
    fail_pending(); blocked_ = true; ready_mask_ = 0;
    error = first; return error.empty();
}
bool Core::room(std::string& error, bool incoming) {
    const auto open = std::count_if(entries_.begin(), entries_.end(), [this](const Entry& e) { return pending(e); });
    if ((!incoming && open >= 16) || (incoming && local_ == 1 && open >= 64)) {
        error = "Chat: Sendewarteschlange voll"; return false;
    }
    if (entries_.size() == 100) {
        const auto old = std::find_if(entries_.begin(), entries_.end(), [this](const Entry& e) { return !pending(e); });
        if (old == entries_.end()) { error = "Chat: Verlauf enthält nur offene Nachrichten"; return false; }
        entries_.erase(old);
    }
    return true;
}
void Core::sort_entries() {
    std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        if (!a.host_order) return false;
        return !b.host_order || a.host_order < b.host_order;
    });
}
bool Core::submit(const std::string& text, std::string& error, uint64_t* sequence) {
    if (!connected()) { error = "Chat: keine verbundene Gegenstelle"; return false; }
    if (!valid_text(text)) { error = "Chat: Text muss 1 bis 240 gültige UTF-8-Bytes ohne Steuerzeichen enthalten"; return false; }
    if (next_sequence_ == std::numeric_limits<uint64_t>::max()) { error = "Chat: Nachrichtenfolge erschöpft"; return false; }
    if (!room(error)) return false;
    Entry entry; entry.origin = local_; entry.sequence = ++next_sequence_; entry.text = text;
    entry.origin_admission = admission_; entries_.push_back(std::move(entry));
    if (sequence) *sequence = next_sequence_;
    error.clear(); return true;
}
Packet Core::message(Kind kind, const Entry& entry) const {
    Packet packet; packet.kind = kind; packet.origin = entry.origin; packet.sequence = entry.sequence;
    packet.conversation = conversation_; packet.host_order = entry.host_order;
    packet.origin_admission = entry.origin_admission;
    packet.expected_mask = entry.expected_mask; packet.ack_mask = entry.ack_mask;
    if (kind == Kind::Message || kind == Kind::Request) packet.text = entry.text;
    return packet;
}
bool Core::complete(Entry& entry, const Send& send, std::string& error) {
    const bool success = entry.ack_mask == entry.expected_mask;
    if (entry.origin == local_) entry.delivery = success ? Delivery::Confirmed : Delivery::Failed;
    else if (peer_challenge_[entry.origin] == entry.origin_admission && (ready_mask_ & bit(entry.origin))) {
        if (!send_packet(message(Kind::Completion, entry), entry.origin, send, error)) return false;
    }
    entry.completion_sent = true; return true;
}
bool Core::commit(Entry& entry, const Send& send, std::string& error, std::chrono::steady_clock::time_point now) {
    if (host_order_ == std::numeric_limits<uint64_t>::max()) { error = "Chat: Host-Reihenfolge erschöpft"; return false; }
    entry.host_order = ++host_order_;
    entry.expected_mask = uint8_t(ready_mask_ & ~bit(entry.origin));
    entry.ack_mask = uint8_t(entry.expected_mask & 1);
    entry.sent_at = now;
    const Packet packet = message(Kind::Message, entry);
    for (uint8_t peer = 2; peer <= 4; ++peer) if (ready_mask_ & bit(peer)) {
        // The origin also receives this echo, which assigns its transcript position.
        if (!send_packet(packet, peer, send, error)) return false;
    }
    if (entry.ack_mask == entry.expected_mask) return complete(entry, send, error);
    return true;
}
bool Core::depart(uint8_t peer, const Send& send, std::string& error) {
    ready_mask_ &= uint8_t(~bit(peer));
    for (auto& entry : entries_) if (pending(entry) &&
        (entry.origin == peer || ((entry.expected_mask & bit(peer)) && !(entry.ack_mask & bit(peer))))) {
        entry.delivery = Delivery::Failed;
        if (!complete(entry, send, error)) return false;
    }
    return true;
}
bool Core::tick(const Send& send, std::string& error, std::chrono::steady_clock::time_point now) {
    error.clear();
    if (!pending_error_.empty()) { error = std::move(pending_error_); pending_error_.clear(); return false; }
    if (!admission_ || blocked_ || !mask_) return true;
    if (local_ != 1 && !conversation_) {
        if (last_hello_ == std::chrono::steady_clock::time_point{} || now - last_hello_ >= std::chrono::seconds(1)) {
            Packet hello; hello.kind = Kind::Hello; hello.origin = local_;
            if (!send_packet(hello, 1, send, error)) return false;
            last_hello_ = now;
        }
        return true;
    }
    for (auto& entry : entries_) {
        if (!pending(entry)) continue;
        if (entry.sent_at != std::chrono::steady_clock::time_point{} && now - entry.sent_at > std::chrono::seconds(10)) {
            entry.delivery = Delivery::Failed;
            std::string completion_error;
            if (local_ == 1 && !complete(entry, send, completion_error)) {
                error = "Chat: Bestätigung nach 10 Sekunden ausgeblieben; " + completion_error; return false;
            }
            error = "Chat: Empfangsbestätigung nach 10 Sekunden ausgeblieben Peer-Maske=" +
                std::to_string(entry.ack_mask) + " erforderlich=" + std::to_string(entry.expected_mask);
            return false;
        }
        if (entry.origin != local_ || entry.host_order || entry.sent_at != std::chrono::steady_clock::time_point{}) continue;
        if (local_ == 1) {
            if (!commit(entry, send, error, now)) return false;
        } else {
            if (!send_packet(message(Kind::Request, entry), 1, send, error)) return false;
            entry.sent_at = now;
        }
    }
    sort_entries(); return true;
}
bool Core::receive(uint8_t sender, const uint8_t* bytes, uint32_t size, const Send& send, std::string& error) {
    error.clear(); Packet packet;
    if (!admission_ || blocked_ || !mask_ || sender < 1 || sender > 4 || sender == local_ ||
        !(mask_ & bit(sender)) || !decode(bytes, size, packet, error) || packet.session != session_) {
        if (error.empty()) error = "Chat: Sitzung oder Absender ungültig";
        return false;
    }
    if (packet.kind == Kind::Hello) {
        if (local_ != 1 || sender != packet.origin || packet.challenge < peer_challenge_[sender] ||
            (packet.challenge == peer_challenge_[sender] && !(ready_mask_ & bit(sender)))) {
            error = "Chat: veraltete oder fremde Lobby-Anmeldung"; return false;
        }
        if (packet.challenge != peer_challenge_[sender]) {
            if (!depart(sender, send, error)) return false;
            peer_challenge_[sender] = packet.challenge; last_request_[sender] = 0;
            peer_baseline_[sender] = host_order_;
        }
        Packet welcome; welcome.kind = Kind::Welcome; welcome.origin = 1;
        welcome.conversation = conversation_; welcome.host_order = peer_baseline_[sender];
        if (!send_packet(welcome, sender, send, error)) return false;
        ready_mask_ |= bit(sender); return true;
    }
    if (packet.kind == Kind::Welcome) {
        if (local_ == 1 || sender != 1 || packet.origin != 1 || packet.challenge != admission_ ||
            packet.conversation < last_conversation_ || (conversation_ && packet.conversation != conversation_)) {
            error = "Chat: Lobby-Bestätigung gehört nicht zur aktuellen Anmeldung"; return false;
        }
        if (!conversation_) {
            conversation_ = last_conversation_ = packet.conversation;
            host_order_ = packet.host_order; ready_mask_ = uint8_t(bit(local_) | 1);
        }
        return true;
    }
    if (!conversation_ || packet.conversation != conversation_ ||
        packet.challenge != (local_ == 1 ? peer_challenge_[sender] : admission_) ||
        !(ready_mask_ & bit(sender)) || (local_ != 1 && sender != 1)) {
        error = "Chat: veraltete Unterhaltung oder Lobby-Anmeldung"; return false;
    }
    if (packet.kind == Kind::Retire && local_ != 1 && packet.origin == 1) {
        fail_pending(); blocked_ = true; ready_mask_ = 0; return true;
    }
    if (packet.kind == Kind::Leave && local_ == 1 && packet.origin == sender) return depart(sender, send, error);
    if (packet.kind == Kind::Request && local_ == 1 && packet.origin == sender &&
        packet.origin_admission == packet.challenge) {
        if (packet.sequence <= last_request_[sender]) {
            const auto previous = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) {
                return e.origin == sender && e.origin_admission == packet.origin_admission && e.sequence == packet.sequence;
            });
            if (previous == entries_.end() || previous->text != packet.text) {
                error = "Chat: alte oder widersprüchliche Nachrichten-ID"; return false;
            }
            return true;
        }
        if (packet.sequence != last_request_[sender] + 1) { error = "Chat: Lücke in Absenderfolge"; return false; }
        if (!room(error, true)) return false;
        Entry entry; entry.origin = sender; entry.sequence = packet.sequence; entry.text = packet.text;
        entry.origin_admission = packet.origin_admission; entry.delivery = Delivery::Received;
        entries_.push_back(std::move(entry)); last_request_[sender] = packet.sequence;
        if (!commit(entries_.back(), send, error)) return false;
        sort_entries(); return true;
    }
    if (packet.kind == Kind::Message && local_ != 1) {
        auto previous = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.host_order == packet.host_order; });
        if (packet.host_order <= host_order_) {
            if (previous == entries_.end() || previous->origin != packet.origin || previous->sequence != packet.sequence ||
                previous->origin_admission != packet.origin_admission || previous->text != packet.text ||
                previous->expected_mask != packet.expected_mask) {
                error = "Chat: alte oder widersprüchliche Host-Reihenfolge"; return false;
            }
        } else {
            if (packet.host_order != host_order_ + 1) { error = "Chat: Lücke in Host-Reihenfolge"; return false; }
            if (packet.origin == local_) {
                previous = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) {
                    return e.origin == local_ && e.origin_admission == admission_ && e.sequence == packet.sequence;
                });
                if (packet.origin_admission != admission_ || previous == entries_.end() ||
                    previous->text != packet.text || previous->host_order) {
                    error = "Chat: Host-Echo ohne passende lokale Nachricht"; return false;
                }
            } else {
                if (!(packet.expected_mask & bit(local_))) { error = "Chat: Nachricht ohne lokalen Empfänger"; return false; }
                if (!room(error, true)) return false;
                Entry entry; entry.origin = packet.origin; entry.sequence = packet.sequence; entry.text = packet.text;
                entry.origin_admission = packet.origin_admission; entry.delivery = Delivery::Received;
                entries_.push_back(std::move(entry)); previous = entries_.end() - 1;
            }
            previous->host_order = packet.host_order; previous->expected_mask = packet.expected_mask;
            previous->ack_mask = packet.ack_mask; host_order_ = packet.host_order;
        }
        if (packet.origin != local_ && !send_packet(message(Kind::Ack, *previous), 1, send, error)) return false;
        sort_entries(); return true;
    }
    if (packet.kind == Kind::Ack && local_ == 1) {
        for (auto& entry : entries_) if (entry.host_order == packet.host_order && entry.origin == packet.origin &&
            entry.sequence == packet.sequence && entry.origin_admission == packet.origin_admission &&
            entry.expected_mask == packet.expected_mask && (entry.expected_mask & bit(sender))) {
            if (entry.delivery == Delivery::Failed) { error = "Chat: Bestätigung nach fehlgeschlagener Zustellung"; return false; }
            entry.ack_mask |= bit(sender);
            if (!entry.completion_sent && entry.ack_mask == entry.expected_mask) return complete(entry, send, error);
            return true;
        }
    }
    if (packet.kind == Kind::Completion && local_ != 1 && packet.origin == local_ && packet.origin_admission == admission_) {
        for (auto& entry : entries_) if (entry.host_order == packet.host_order && entry.origin == local_ &&
            entry.sequence == packet.sequence && entry.origin_admission == packet.origin_admission &&
            entry.expected_mask == packet.expected_mask && entry.delivery != Delivery::Failed) {
            entry.ack_mask = packet.ack_mask;
            entry.delivery = entry.ack_mask == entry.expected_mask ? Delivery::Confirmed : Delivery::Failed;
            return true;
        }
    }
    error = "Chat: Nachrichtentyp, Ursprung oder Bestätigung passt nicht zur Hostroute"; return false;
}
} // namespace mt2mp::net::chat

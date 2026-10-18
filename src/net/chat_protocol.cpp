#include "mt2mp/net/chat_protocol.h"

namespace mt2mp::net::chat {
namespace {
constexpr size_t header_size = 59;

void put64(std::vector<uint8_t>& out, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) out.push_back(uint8_t(value >> (i * 8)));
}
uint64_t get64(const uint8_t* bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= uint64_t(bytes[i]) << (i * 8);
    return value;
}
}

bool valid_text(const std::string& text) {
    if (text.empty() || text.size() > max_text_bytes) return false;
    const auto* data = reinterpret_cast<const uint8_t*>(text.data());
    for (size_t i = 0; i < text.size();) {
        const uint8_t first = data[i];
        if (first < 0x20 || first == 0x7f) return false;
        if (first < 0x80) { ++i; continue; }
        unsigned count = 0;
        uint32_t point = 0;
        if (first >= 0xc2 && first <= 0xdf) { count = 2; point = first & 0x1f; }
        else if (first >= 0xe0 && first <= 0xef) { count = 3; point = first & 0x0f; }
        else if (first >= 0xf0 && first <= 0xf4) { count = 4; point = first & 0x07; }
        else return false;
        if (i + count > text.size()) return false;
        for (unsigned n = 1; n < count; ++n) {
            if ((data[i + n] & 0xc0) != 0x80) return false;
            point = (point << 6) | (data[i + n] & 0x3f);
        }
        if ((count == 3 && point < 0x800) || (count == 4 && point < 0x10000) ||
            (point >= 0xd800 && point <= 0xdfff) || point > 0x10ffff ||
            (point >= 0x80 && point <= 0x9f)) return false;
        i += count;
    }
    return true;
}

bool encode(const Packet& packet, std::vector<uint8_t>& bytes, std::string& error) {
    const bool data = packet.kind == Kind::Request || packet.kind == Kind::Message;
    const bool ordered = packet.kind == Kind::Message || packet.kind == Kind::Ack || packet.kind == Kind::Completion;
    const bool control = packet.kind == Kind::Hello || packet.kind == Kind::Welcome ||
        packet.kind == Kind::Leave || packet.kind == Kind::Retire;
    if (!packet.session || packet.origin < 1 || packet.origin > 4 || !packet.challenge ||
        (!data && !ordered && !control) ||
        (data ? !valid_text(packet.text) : !packet.text.empty()) ||
        ((data || ordered) && (!packet.sequence || !packet.origin_admission)) ||
        (control && (packet.sequence || packet.origin_admission || packet.expected_mask || packet.ack_mask)) ||
        (packet.kind == Kind::Hello ? packet.conversation != 0 : packet.conversation == 0) ||
        (ordered && (!packet.host_order || !packet.expected_mask || (packet.expected_mask & 0xf0) ||
                     (packet.expected_mask & (1u << (packet.origin - 1))) || (packet.ack_mask & ~packet.expected_mask))) ||
        (!ordered && packet.kind != Kind::Welcome && packet.host_order) ||
        (packet.kind == Kind::Request && (packet.expected_mask || packet.ack_mask))) {
        error = "chat packet fields invalid"; return false;
    }
    bytes.clear(); bytes.reserve(header_size + packet.text.size());
    bytes.insert(bytes.end(), {'M','T','2','C',2,uint8_t(packet.kind),packet.origin,packet.expected_mask,packet.ack_mask});
    put64(bytes, packet.session); put64(bytes, packet.sequence);
    put64(bytes, packet.conversation); put64(bytes, packet.challenge);
    put64(bytes, packet.host_order); put64(bytes, packet.origin_admission);
    bytes.push_back(uint8_t(packet.text.size())); bytes.push_back(uint8_t(packet.text.size() >> 8));
    bytes.insert(bytes.end(), packet.text.begin(), packet.text.end());
    error.clear(); return true;
}

bool decode(const uint8_t* bytes, uint32_t size, Packet& packet, std::string& error) {
    if (!bytes || size < header_size || size > header_size + max_text_bytes ||
        bytes[0] != 'M' || bytes[1] != 'T' || bytes[2] != '2' || bytes[3] != 'C' || bytes[4] != 2) {
        error = "chat packet signature or length invalid"; return false;
    }
    Packet value;
    value.kind = Kind(bytes[5]); value.origin = bytes[6];
    value.expected_mask = bytes[7]; value.ack_mask = bytes[8];
    value.session = get64(bytes + 9); value.sequence = get64(bytes + 17);
    value.conversation = get64(bytes + 25); value.challenge = get64(bytes + 33);
    value.host_order = get64(bytes + 41); value.origin_admission = get64(bytes + 49);
    const size_t text_size = size_t(bytes[57]) | (size_t(bytes[58]) << 8);
    if (size != header_size + text_size) { error = "chat packet text length invalid"; return false; }
    value.text.assign(reinterpret_cast<const char*>(bytes + header_size), text_size);
    std::vector<uint8_t> check;
    if (!encode(value, check, error)) return false;
    packet = std::move(value); error.clear(); return true;
}

} // namespace mt2mp::net::chat

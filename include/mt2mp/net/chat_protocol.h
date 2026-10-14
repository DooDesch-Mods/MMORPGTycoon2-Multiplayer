#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::net::chat {

inline constexpr uint8_t channel = 11;
inline constexpr size_t max_text_bytes = 240;

enum class Kind : uint8_t { Message = 1, Ack = 2 };

struct Packet {
    Kind kind = Kind::Message;
    uint64_t session = 0;
    uint8_t origin = 0;
    uint64_t sequence = 0;
    std::string text;
};

bool valid_text(const std::string& text);
bool encode(const Packet& packet, std::vector<uint8_t>& bytes, std::string& error);
bool decode(const uint8_t* bytes, uint32_t size, Packet& packet, std::string& error);

} // namespace mt2mp::net::chat

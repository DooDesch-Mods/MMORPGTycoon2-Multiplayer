#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace mt2mp::net::relay {
constexpr uint32_t magic = 0x4d32524c;
constexpr uint32_t max_payload = 128 * 1024;
enum Type : uint8_t { Hello = 1, Data = 2, Ready = 3, Error = 4, Ping = 5, Pong = 6, Shutdown = 7 };
#pragma pack(push, 1)
struct Header { uint32_t magic; uint8_t type; uint8_t channel; uint16_t version; uint32_t length; };
struct Identity { uint64_t session; uint64_t peer; uint8_t host; char token[32]; };
struct Address { uint64_t target; uint64_t sender; };
#pragma pack(pop)
enum class Parse { Incomplete, Complete, Invalid };
inline std::vector<uint8_t> frame(uint8_t type, uint8_t channel, const void* payload, uint32_t size) {
    if (size > max_payload || (size && !payload)) return {};
    Header header{magic, type, channel, 1, size};
    std::vector<uint8_t> result(sizeof(header) + size);
    memcpy(result.data(), &header, sizeof(header));
    if (size) memcpy(result.data() + sizeof(header), payload, size);
    return result;
}
inline Parse pop(std::vector<uint8_t>& bytes, Header& header, std::vector<uint8_t>& payload) {
    if (bytes.size() < sizeof(Header)) return Parse::Incomplete;
    memcpy(&header, bytes.data(), sizeof(header));
    if (header.magic != magic || header.version != 1 || header.length > max_payload || header.type < Hello || header.type > Shutdown) return Parse::Invalid;
    const size_t total = sizeof(Header) + header.length;
    if (bytes.size() < total) return Parse::Incomplete;
    payload.assign(bytes.begin() + sizeof(Header), bytes.begin() + total);
    bytes.erase(bytes.begin(), bytes.begin() + total);
    return Parse::Complete;
}
}

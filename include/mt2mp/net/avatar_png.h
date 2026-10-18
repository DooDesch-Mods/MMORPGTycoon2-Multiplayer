#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::net::avatars {
// Bounded RGBA PNG output uses stored DEFLATE blocks, avoiding a new codec dependency.
inline bool encode_png(uint32_t width, uint32_t height, const std::vector<unsigned char>& rgba,
                       std::vector<unsigned char>& png, std::string& error) {
    png.clear();
    if (!width || !height || width > 128 || height > 128 || rgba.size() != size_t(width) * height * 4) {
        error = "Avatar PNG requires 1..128 pixel dimensions and exact RGBA bytes"; return false;
    }
    const auto be32 = [](std::vector<unsigned char>& out, uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<unsigned char>(value >> shift));
    };
    const auto chunk = [&](const char* type, const std::vector<unsigned char>& bytes) {
        be32(png, static_cast<uint32_t>(bytes.size()));
        const auto begin = png.size();
        for (unsigned i = 0; i < 4; ++i) png.push_back(static_cast<unsigned char>(type[i]));
        png.insert(png.end(), bytes.begin(), bytes.end());
        uint32_t crc = 0xffffffffu;
        for (size_t i = begin; i < png.size(); ++i) {
            crc ^= png[i];
            for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
        be32(png, ~crc);
    };
    png = {137,80,78,71,13,10,26,10};
    std::vector<unsigned char> header;
    be32(header, width); be32(header, height);
    header.insert(header.end(), {8,6,0,0,0});
    chunk("IHDR", header);
    std::vector<unsigned char> raw;
    raw.reserve((size_t(width)*4+1)*height);
    for (uint32_t row = 0; row < height; ++row) {
        raw.push_back(0);
        const auto begin = rgba.begin() + size_t(row)*width*4;
        raw.insert(raw.end(), begin, begin + size_t(width)*4);
    }
    std::vector<unsigned char> zlib{0x78,0x01};
    size_t offset = 0;
    while (offset < raw.size()) {
        const auto remaining = raw.size() - offset;
        const uint16_t length = static_cast<uint16_t>(remaining > 65535 ? 65535 : remaining);
        const uint16_t inverse = static_cast<uint16_t>(~length);
        zlib.push_back(offset + length == raw.size() ? 1 : 0);
        zlib.push_back(static_cast<unsigned char>(length)); zlib.push_back(static_cast<unsigned char>(length >> 8));
        zlib.push_back(static_cast<unsigned char>(inverse)); zlib.push_back(static_cast<unsigned char>(inverse >> 8));
        zlib.insert(zlib.end(), raw.begin() + offset, raw.begin() + offset + length);
        offset += length;
    }
    uint32_t a = 1, b = 0;
    for (unsigned char byte : raw) { a = (a + byte) % 65521; b = (b + a) % 65521; }
    be32(zlib, (b << 16) | a);
    chunk("IDAT", zlib); chunk("IEND", {});
    error.clear(); return true;
}
}

#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::render_pool_guard {
struct Fingerprint { uintptr_t rva; size_t size; const char* sha256; };
inline constexpr std::array<Fingerprint, 9> original_bodies = {{
    {0x9b53e0, 288, "876549a03d978b9f51a91b3f0548dc6c47994d6d5599484b52fa23b26e7c925f"},
    {0x9b5790, 368, "acbda14185a5afcf6960833467933b1a283c540f613bdd054317bbbbcd7e4aa7"},
    {0x9b6c90, 368, "556903bf0dd4c32d1600be5679bcfdc1dc3019c4fd395d4328b998000a89e1e4"},
    {0x9b6ef0, 368, "36ef7287e23fabdd6650f7a7b4678998756e1a91b2cef28e5956484a7249128c"},
    {0x9b7060, 368, "9e08b55c29eb74162e9989af10ef4f8f109c8343491f1c5a3f71e4aed1f9c61f"},
    {0x9b72c0, 368, "8394a362eb5052db3836b67ad6860dfd45cdd571f4f6c67a9c4cab5ad61b4733"},
    {0x9b7430, 368, "b50a294cba2b2615bb7565fa59e432bbe9ced3b37a77d56e594795bd2667684f"},
    {0x9b8bc0, 384, "14667f94abfad0728c216d98eb70f57a0b9e8b3db50aab7c4e80c2c2782459ee"},
    {0x9b8ff0, 144, "6582b7a07afb8968e196d052cfa9bbdfbefbc6edc4f1aa580efc6e1458ad8817"},
}};
}

#pragma once
#include <array>
#include <cstdint>
namespace mt2mp::game::dungeon_config_codes {
struct Code {uintptr_t rva;size_t size;const char* sha;};
inline constexpr std::array<Code,4> helpers{{
{0x825e50,147,"fb3b2a61a796f1f1b174e0b4fbe790cc5a9419b00a1033aaa054b6118cccd8cf"},
{0x55280,113,"e876162c33bdd72d1be34d4b2001369291b5d583f304e7bbcdd94af2128f9f17"},
{0x6b4fb0,3527,"43cb0bb61db029186204f9b08b0f93b83a3d4e2cabf02f580a7fa424f01ad2b4"},
{0x6b5ee0,24,"e0e8f9900dfde377d73258a11bd39bc13d3df89245a5202c93d2626e247b50cd"},
}};
}

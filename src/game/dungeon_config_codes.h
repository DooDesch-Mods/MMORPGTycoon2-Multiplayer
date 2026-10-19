#pragma once
#include <array>
#include <cstdint>
namespace mt2mp::game::dungeon_config_codes {
struct Code {uintptr_t rva;size_t size;const char* sha;};
inline constexpr std::array<Code,4> helpers{{
{0x81d860,147,"5ee9e98e3d2cb313e0cbe9de2384633d84e962cedd7e9d157596fa96ec0a6eec"},
{0x54d80,113,"e876162c33bdd72d1be34d4b2001369291b5d583f304e7bbcdd94af2128f9f17"},
{0x6adc90,3527,"25113285afd8eb594bf67050a63c2554405edb17fd2f1d98f38af792aeebb3f9"},
{0x6aebc0,24,"e0e8f9900dfde377d73258a11bd39bc13d3df89245a5202c93d2626e247b50cd"},
}};
}

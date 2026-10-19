#pragma once
#include <array>
#include <cstdint>
namespace mt2mp::game::dungeon_light_codes {
struct Code {uintptr_t rva;size_t size;const char* sha;};
inline constexpr std::array<Code,9> helpers{{
{0x6d4380,455,"0e741457047bfa2b023e98ee805b404f5decd287e588a731754628b0df74b78c"},
{0x7b95a0,17,"6a1c053fd5d66b35bd798028ad140017ae60f17f0d43fcdfe7fec43c7e4f323b"},
{0x7bbdd0,612,"3b44ae45b68ae31be9d9c3e90fe09bf5da31f9e93eef2b6857cb2f7137019395"},
{0x7bf9e0,17,"5c0b1940d701d2edec21e468c9fdb3bceae6a9f8dee6410d0139dca26d8da7c6"},
{0x7c2c60,356,"541bd29e30903332e7f9db685a733dba9dc7d3e6c37c505c50bb7eb83896f9b7"},
{0x839d30,553,"b27fbda9da6bf1f012092e32deed3b930b16593fc7d45e45e9bcb0b481821d8d"},
{0x9eec90,121,"619e4123b48f0c7f46ae3abbbfd2a37c63fbc9cddc2304de7382659894bae66a"},
{0x9eefa0,117,"5766b29a4c6590ec7f9198cb5a587ec364da3430fb2103057bc95a333fbc14be"},
{0x9ffd80,366,"a5d730b5db40198a08dc51475f49bc7ebc63dc7d859d04295b047eab6c10725c"},
}};
}

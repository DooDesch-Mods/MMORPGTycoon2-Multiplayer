#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::dungeon_gizmo_codes {
struct Code {uintptr_t rva;size_t size;const char* sha;};
inline constexpr std::array<Code,8> helpers{{
{0x6fc880,930,"a8a44d16ebf40fe7bb1db28c93e1d70a0b299c32e32e76262c544f5980b9e4a6"},
{0x6fac90,2041,"f16bf84505d322516d4e92a0ac455516e2fdd626769a9ee17361efa5984d650a"},
{0x6faa80,82,"85bfaa7e15e06fc5e59b6cc6487de1afa7b27a76cf6d08b1a50396b06bdd0226"},
{0x146590,38,"cd1def127ff461cfccd56ad5c12b07e201cd625757245b02f98c41e472a0bb1e"},
{0x7110b0,722,"0ae5363d0ab5e83d6a731b083c230a6e6bb9718429a74f0ba524ab45640b2776"},
{0x970960,431,"9a71ac041e79839aeb4d2eb5bbe05a04e52267ec78d389b751ebabec515803e6"},
{0x9764e0,54,"f42089c0083326ad060af284f4f32cf9679297ee163c31a7efa854c2508123ba"},
{0x823080,5,"c4912a247de8df24b27eab95ed89771676318e7e4efe5555cc96e6ebeb58b386"},
}};
}

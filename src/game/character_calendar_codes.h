#pragma once
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::character_calendar::codes {
struct Pin {uintptr_t rva;size_t bytes;const char* sha;};
inline constexpr Pin values[]{
    {0xe8080,288,"6d7c8326e01c886f5fd3115cb51b04fe002baed6c687c3fc27ee356b126fa9f9"},
    {0x25cb30,256,"138be116cd9f7b06ed31d6ccc8c3122252961c00a5df710ccf94a51fa05c5042"},
    {0x25d480,224,"ff41a9dd20019b04b491d57692838cf938c6a1a4120f1c0f215de9f0a474ef27"},
    {0x1280690,2048,"afba02c358bddecc762c68a28cfeac8ce4d903a3f21cef396669ab5783e16923"},
    {0x12faa40,2096,"0934b2e71b2bb5631158d474de62ceac9fcc3faf0552903ddf0ac57aa2990373"},
    {0x859560,128,"c2149a557a8ae05f43b4905e413cc47f584ded1caeb1568ee81268b8380fbb84"},
};
}

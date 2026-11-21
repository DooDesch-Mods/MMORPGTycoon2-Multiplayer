#pragma once
#include "npc_quest_list_ui_codes.h"
namespace mt2mp::game::building_status_codes {
inline constexpr npc_quest_list_ui_codes::Code entry{0x40fdc0,93,"7d94e0501f88171937e7ef058469dc609711510f7c47773c35b8a3bd7c15049a",{0x56,0x53,0x48,0x83,0xec,0x38,0x48,0x89,0xcb,0x89,0xd6,0xe8,0xe0,0xfe,0xff,0xff,0x84,0xc0,0x74,0x26,0xc6,0x83,0xd0,0x0b,0x00,0x00,0x01,0x48,0x8b,0x8b,0xd8,0x0b}};
struct Code {uintptr_t rva;size_t size;const char* sha;};
inline constexpr std::array<Code,6> helpers{{
{0x59320,82,"e4ccb0db92727b9c9e5f984f3ef27b467e03b62c0764ffcac8f965b0b73c1038"},
{0x5a300,128,"131c8d5b4df3f428312f7445321c108089479b2a580fde725de23dec434d1187"},
{0x5a380,105,"d8ac876617271f735928810ed7181853616f6034e0b65e838e9a089c44b70f25"},
{0x40fcb0,172,"91deb4561137ebeef630b82a2fe394f5130c68eb73578e55362486eb37fe1959"},
{0x674b60,3042,"ae074c36032ae13991de43c768f42021e0705dc02f62aa9921e5c25c45448a27"},
{0x6dcea0,337,"e0941cab02294db789a88d1ad17c64c8e8b69c2972a1027e8f55ca49d58e8d96"},
}};
}

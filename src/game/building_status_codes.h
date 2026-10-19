#pragma once
#include "npc_quest_list_ui_codes.h"
namespace mt2mp::game::building_status_codes {
inline constexpr npc_quest_list_ui_codes::Code entry{0x40b200,93,"4118c2e27c477d85cfcf775267095f4702ecb732681829056c4b765bf4a0bcc7",{0x56,0x53,0x48,0x83,0xec,0x38,0x48,0x89,0xcb,0x89,0xd6,0xe8,0xe0,0xfe,0xff,0xff,0x84,0xc0,0x74,0x26,0xc6,0x83,0xd0,0x0b,0x00,0x00,0x01,0x48,0x8b,0x8b,0xd8,0x0b}};
struct Code {uintptr_t rva;size_t size;const char* sha;};
inline constexpr std::array<Code,6> helpers{{
{0x58e20,82,"2ee5920507cab0396a34564e4b35e435d7f71fdc5b81c99c6fa0078ff2fd727a"},
{0x59e00,128,"79425bbf72f28b5bcd3990b5d4ad425623d69b8c8d85472a74b30c1361b890e5"},
{0x59e80,105,"56254ebf987576dfa16539ff060e755518c2b1905506d9d384db7904fb1c25d1"},
{0x40b0f0,172,"c82c0b18bee46a3a653210ecbbda1f7f23cd4c911c00ef0bb3094064305baf00"},
{0x66d990,3042,"03e58cce712898efef65baa4d798734dc1a8cd24fdb1707a845482f77393654f"},
{0x6d5a80,337,"5a138dce9c60db02e1c36e04f7a8cd67aee95c057f6bb8dc01138e87384725b9"},
}};
}

#pragma once
#include <array>
#include <cstdint>
namespace mt2mp::game::costume_edit_codes {
struct Code {uintptr_t rva;size_t size;const char* sha;};
inline constexpr std::array<Code,7> helpers{{
{0x29f40,2635,"cb4a3c592829341ff3a64c1040312c6dcd9d2f451bd1fe1eaf0f3ae1755cabdf"},
{0x2c970,1373,"ca7e64eb0ee9b8d507cb0f9d672ed4aa09ba7f09e0c6e79ff31cd813b9d6fdf3"},
{0x2d020,1669,"9eb0201a0fda1dfbf56e06eca2304d705b70d0fffc4e9ab722b8180ace779f2d"},
{0x94b80,67,"ffa7e4b2ec61d7d1e5671a9287f0762dffc7bfa81592a507fe2bb4063f24b81e"},
{0x94bd0,196,"1e91bd3740b94500a84b2e907f63f89d0631ab15fee36facacccbdf93d095677"},
{0x6b4400,249,"4b036f58d41750bc6dec41e2dd7dc11c873f35b4bb622fe7c46e9f40a5fe5a2a"},
{0x9ff370,49,"ffe20a4c1edc1cea5bf07910f5132caff86a664fc4ce7e92c5f5a9fed020a72b"},
}};
}

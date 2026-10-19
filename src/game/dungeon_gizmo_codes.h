#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::dungeon_gizmo_codes {
struct Code {uintptr_t rva;size_t size;const char* sha;};
inline constexpr std::array<Code,8> helpers{{
{0x6f53f0,930,"7fb0b30f3d0f041d54bbd13f62a26111ff2afe89dfb3b9730647c955bdbf64de"},
{0x6f3800,2041,"3401ec064831a83e6e09af76061de2ba6e8b3e2fc89337858886f673baae889d"},
{0x6f35f0,82,"dfd1ff862a5e28ba14dbe1037b7c0379cdb7e898a0ee2e53136bad7373b66eaf"},
{0x145560,38,"cd1def127ff461cfccd56ad5c12b07e201cd625757245b02f98c41e472a0bb1e"},
{0x709bc0,722,"065d682204a027f11ee9467228e925c6ed864dab4b8e47f02e580236f2279a39"},
{0x967740,431,"3140e66afe2f41017893741dd2fddd3035ac5e1b07a91fa87e48e7c5fd8f52df"},
{0x96d2c0,54,"8ad06b36d8cfcdb1446f3d8731c65e345be2aa7451ff83bdb8b7c880470ebf70"},
{0x81aa90,5,"c4912a247de8df24b27eab95ed89771676318e7e4efe5555cc96e6ebeb58b386"},
}};
}

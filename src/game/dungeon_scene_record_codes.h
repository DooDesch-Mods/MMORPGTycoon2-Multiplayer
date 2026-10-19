#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::dungeon_scene_record_codes {
struct Pin {uintptr_t rva;size_t bytes;const char* sha;};
inline constexpr std::array<Pin,8> helpers{{
    {0x9667c0,181,"f061a1fe2ca42b75546e22e95981092d682869382e859ed5b10af7ecd80ba6c9"},
    {0x9676a0,148,"7f6e66c76e2be9e2d7f43ab04d8721cc060f16ddcd111a422605af135ab1bb4a"},
    {0x89e580,106,"fc9d17c2e62785b1cfd9e4bb59ae1568cb603f1e232cb30ce48d429581f73646"},
    {0x89e5f0,151,"0808e0234ca17c59c1fd755944f300a0145c52953160aa8231eef3006665e690"},
    {0xe4a3a0,87,"508a35eb96b7455b9618c2c2a91d3548b37bba5dc94b6d7ee06b1ace00b825f2"},
    {0xe4a400,409,"99435ca821aa7fa8fb7765e85cee7e60d6fcd715a34a19259d61c1724e388220"},
    {0x110ef70,87,"857746752eb931c80a3ff02f5389d3bc759a3c05cd78372e27c39c0623af66ae"},
    {0x110efd0,409,"34c021d03bdcbec3d0532bbc864bc8f38bd0539f150627d2b5eaa04710768717"},
}};
}

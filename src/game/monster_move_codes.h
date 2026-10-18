#pragma once
#include <cstddef>
#include <cstdint>
namespace monster_move_codes {
struct Code {uintptr_t rva;size_t size;const char* sha;};
inline constexpr Code values[]{
    {0x32a990,128,"e0f7fefcc9e0921a74a5a3e2d861770b66841012fd6a98a139f76c90d49cb88c"},
    {0x5089b0,144,"00982cd849efe71eac9e151443739d0ba4e29be870142aa09e4be377605fffc1"},
    {0x5f5b00,16,"46832d55abde7e97b48eb663a85e3f3fd3b2d60b256ff032b5bebeed4eba79d6"},
    {0x5f6ed0,656,"5324eb648ddd1deafeb72bc50085064239ed5ca71a4e6fbb2f156e1ec541420f"},
    {0x3b9eb0,112,"8e8d9e00efbc39ea43f72b4f3bc03415d60d11c45386aef4e5f92f66e84de948"},
    {0x39afe0,192,"f32257656e51323ebfa0a299258f7d27ee46de5795162ea995b04aa21e2caf8b"},
    {0x39a920,288,"be03b97a93fcac39ed9307ffefdd4f1410d8fa588669f8ad704a729f5a6b6abb"},
    {0x3b1c00,176,"a607be569b8737a55d1f690fa9fc42a5af43475557f68da5f40dfb96870b3885"},
};
}

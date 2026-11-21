#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::developer_staff {
struct Fingerprint {uintptr_t rva;size_t size;const char* sha256;};
inline constexpr std::array<Fingerprint,9> original_bodies={{
    {0x6cef90,2560,"63b0c84b87fcb7262ea9793b352282fd8153671426b63d531eae661707a8a634"},
    {0x970590,416,"708dc9f67619c74b41ea34856237e98209b298b0cc72613a4cf3b729999a5976"},
    {0x9708c0,160,"182327238cd9e4972b16d4d21ec53c523fce5ef393eda38318e12cc9eb727642"},
    {0x111f720,272,"646ae2ceb1adbb4b88d9e72b93f1c5cb83b4eda2a7e81c23c411c2aec65ee3d2"},
    {0x55200,32,"f743600a807eb2caf8a0e9f7bed2dae3fa941212661a1bd6505b313b2300c040"},
    {0x261b90,16,"aaf2c645a5a3dacdf7ad5adfcca2d8b39ca14610910e15150c95d83188d6df11"},
    {0x261ba0,16,"66314a3976f59576fcc57e05262fd4627533d63009ec4415df44037b0e665576"},
    {0x56d10,304,"b6eb60babc7dd378ba0edb223a469d4fc972288b8e0970a5a3ccb1aff44077d7"},
    {0x570a0,304,"e04d15adf6e5a539697265c707e313985af5ab2ad3bc839385a90645dfc0a115"}
}};
}

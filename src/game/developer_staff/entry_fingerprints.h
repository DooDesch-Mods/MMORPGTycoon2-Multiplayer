#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::developer_staff {
struct Fingerprint {uintptr_t rva;size_t size;const char* sha256;};
inline constexpr std::array<Fingerprint,9> original_bodies={{
    {0x6c7b70,2560,"d204e8e24dc14d66e170adae7b4b60433fa82baa6eecb81f65353f32d8c5a2af"},
    {0x967370,416,"2a6e4111f91e2c3cecb2aa35b99ed8d71f22a75f8300694f7d9ba6b086c01daa"},
    {0x9676a0,160,"33000088d5ae9e353738b283b76f13a641243aa996c7952714c57d6a07cc170f"},
    {0x1113d80,272,"dee54a7232d3aef27b73436213be00e802ba3cb997d4fb09a5f41f38468fafcc"},
    {0x54d00,32,"f743600a807eb2caf8a0e9f7bed2dae3fa941212661a1bd6505b313b2300c040"},
    {0x25ea90,16,"aaf2c645a5a3dacdf7ad5adfcca2d8b39ca14610910e15150c95d83188d6df11"},
    {0x25eaa0,16,"66314a3976f59576fcc57e05262fd4627533d63009ec4415df44037b0e665576"},
    {0x56810,304,"f08b8e0dc4cdf555b890a9be2d46a647e5adf05b1d6f5d9803e072700be2aa6e"},
    {0x56ba0,304,"6ea3bc55cc3fee6213830a1b65d5c9b6fe0ffbf931e6dccdff4bd171a83f5f5e"}
}};
}

#pragma once
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::dungeon_instance_native::codes {
struct Code {uintptr_t rva;size_t bytes;const char* sha;};
inline constexpr Code routes[]{
    {0xa7670,792,"50506183d19615df1ebae10c45d538120cc030d0aabb01fcc974e1e46d36fa8a"},
    {0xa28f0,152,"2ee640083a7a662c99b5868eb773d1744bb3ef1a746392d77d8ad4cbe60f29b1"},
    {0xb6b40,69,"4a99d44a3f9947070f18777b652ff2da01ad98c8ac04cc5bf114098b36332182"},
    {0xb66e0,1091,"a02bba835291a7c9026dff200783bf80dd2c9d436cc0ac34cab45b781e7e615a"},
    {0xa7a00,1320,"0cc9c9d7ada6454b0c0fdcf62a7a3791c018ca81a9876bb8d22d93b5a7c97b6e"},
    {0xa91e0,202,"89f43e3b1ce19e833b400bbba2bd6a692f00b37680591c6739d32ad4d7e79d8e"},
    {0xa2fd0,95,"e8c957072280ace98064b371f61023e688de59d998415922ac05905df94a2543"},
    {0xb7670,1203,"99617d91bea027cb10470a56061d06d4bcb81069aaa055ac0e2e56c20dadc5e8"},
    {0x50a870,633,"5c946877b83d6a4751e84c81150a0c780e7bf69281c7b22f97a195c5ba16b00f"},
};
}

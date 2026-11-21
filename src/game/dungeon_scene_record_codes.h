#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::dungeon_scene_record_codes {
struct Pin {uintptr_t rva;size_t bytes;const char* sha;};
inline constexpr std::array<Pin,8> helpers{{
    {0x96f9e0,181,"c17476a1733c67fd3eb338f89df6eeabf441b55f6478187288fa9c71cab2e532"},
    {0x9708c0,148,"bb4624bbc533d99539535f19391f65e947168a081318a35d4e7eae53082253f9"},
    {0x8a76a0,104,"84dedde177d3b1f6c787f0781bc69a770830098429da33ac063fc70fbf24eb6c"},
    {0x8a7710,151,"ad04a9466902eea3b1bb7a50a2c321e6deebeae5690738e7653efed5719777ea"},
    {0xe55e90,87,"4014656f89d5f5d60337fcbca21e5bfea9fd9585c94946edd0eb0a9752ea7832"},
    {0xe55ef0,409,"09613ff1537e8c328e2890266a36d9a20f85869ad0f9c983bc9b0848e1ea0565"},
    {0x111a910,87,"2a6ed141294685891aa9449f4c41e697fee592e84aeb87d7ae6f868c08422e47"},
    {0x111a970,409,"383f560ccc800d5c033c706ceb09effc6790399bacedd7f47c8efe26e0de7274"},
}};
}

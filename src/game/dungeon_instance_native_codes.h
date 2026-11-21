#pragma once
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::dungeon_instance_native::codes {
struct Code {uintptr_t rva;size_t bytes;const char* sha;};
inline constexpr Code routes[]{
    {0xa8370,792,"9f34d7520f3c8a876913ecd07e7dd590ebee992ebbf908b23bbc04081d0c7d22"},
    {0xa35f0,152,"31811cc8b9bac756a5e91fee16d64853761fec56c9cb1ed578052e113b7f7965"},
    {0xb7840,69,"213645604eed0f7b6676b627d397ca8098a41bd80401b307e9cafd79db8a8276"},
    {0xb73e0,1091,"a8017d81e266a8558b65b1f29ea8a09b0b128cc248cdbdfeeb8c280343576eba"},
    {0xa8700,1320,"51870dc178c8e46f5b28f552b3ab96b6e6b2af4b64ac5c09b238adf3bc1c27bc"},
    {0xa9ee0,202,"51ee37e784129be636e2bf57d12cd531d5c036e5b58c141b60a8eebf5ab4113c"},
    {0xa3cd0,95,"56aee5bd096e756af550104f827aaae75dbd9dec6519df179d87ef9c713b989f"},
    {0xb8370,1203,"21249b53db9e5c1cbbc3f72eb7cdf5af46d1030b5692ab704a148ccf72eaa94f"},
    {0x5109c0,633,"b05e57922cfb70b4701e396e9f6028c9b3bf23c7263189e0ecc9b42bf3d67a80"},
};
}

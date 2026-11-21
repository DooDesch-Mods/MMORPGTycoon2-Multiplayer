#pragma once
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::marketing_native_probe {
struct NativePin { uintptr_t rva; size_t size; const char* sha; };
inline constexpr NativePin native_pins[]={
    {0x520d30,208,"38c8090427bb5d60b76606a2c0771507c3e908858ed9ca1c5fd153c0009bfb6c"},
    {0xcf62d0,256,"c88bc665eb4dcbc34896a6ac8c45db5b2e807aca1b0532cef43322be165df521"},
    {0xb6a130,416,"0ac6baf24f12bc1b0717ee1c9623857861ec4bc74d4ccc946556c8dd50d2f1ae"},
    {0x66a990,4592,"abbb41cbf01d0c825476e08771519da58589997415fc3a38465f15fb4f6b1bb4"},
    {0xc886b0,256,"2bcad6d1595f0e217427a9765890f905bfe745c0c670f128de33564b0bd4136f"},
    {0x51fcc0,80,"64091442c430b2b9e530c7c7731f4714961b44efbda57813b3d247e0ae80e718"},
    {0x51e570,5968,"4efa96ef47c05a3b3bbe150a1a622ab4582c6273fa09bfa05516471b9d8b09d1"},
    {0x5229c0,656,"1920ee6cb1264af2bba2b78101f9d4fa0cd1a1336d063611cf608350db8f0ada"},
};
}

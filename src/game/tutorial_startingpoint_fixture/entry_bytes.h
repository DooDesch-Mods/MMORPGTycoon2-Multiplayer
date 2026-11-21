#pragma once
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::tutorial_startingpoint_fixture {
inline constexpr char exact_sha[]="7efd8ee7b97f78362e7a2f8116b327fa6b756bdce23114b68143c4c9d2dd50eb";
struct Entry {uintptr_t rva;size_t size;const char* sha256;};
inline constexpr Entry host_begin_pay_entry{0x265fc0,0x10,"fc49725b7197e68a74987ec8d36dbab9dd98ff8b88c788b228a86993f01f422f"};
inline constexpr Entry entries[]={
{0x261d50,0x10,"cc4e402cb0146ba76474fe7f05c7bec7eafa9675187e5e54b4190d1c5c2dfb63"},
{0x261e50,0x10,"4b43567bf3359cff608bb04a5de1af65e583922cb44b96bbcaeeb2d74535576f"},
{0x825e50,0x10,"a776318e9c494cb89d414c4fbefeac29250045e61beab28447c555ea0f411d9e"},
{0x67b950,0x10,"622ce4d76b1a7205085595dfdfc9fcbd1178de08957d3fd28fc23eae8175619c"},
{0x6cef90,0x10,"ace6bb19ed79235b1c69f807274c07417fa3c486720711a9f0514b3e1d97f18d"},
{0x262c10,0x10,"2a8a1b829dcaf0165e4efff07139e583c2dd27f03948f0dda751da1f6d23ee89"},
{0x5347e0,0x10,"c95894ec726f2f58cf46793a76103ba2ec2ffc23cd344b062c458d9b58bce978"},
{0x827215,0x3b,"da7863e908e6339fed3c2e8f1afb4d431455b4c67ce77a7960b676117b251983"},
{0x826ad0,0x740,"3aa31017dd104e5012d34bf7c86e93b516e46ab6bbc9079caaa7b8ee09eba1ef"}
};
}

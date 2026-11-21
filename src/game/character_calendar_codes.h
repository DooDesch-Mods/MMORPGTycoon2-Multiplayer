#pragma once
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::character_calendar::codes {
struct Pin {uintptr_t rva;size_t bytes;const char* sha;};
inline constexpr Pin values[]{
    {0xe90b0,288,"94e6ee002731ce9c82c745053887811cb1090991147f29690983d73721d45355"},
    {0x25fbf0,256,"fd26ddf55e28fcf20ab3e7716871e1b4b257787e345a5f893e3874412ef584ae"},
    {0x260540,224,"b6b9a2ac13bb232234da163cecef8e8af2df14a42aa791f49f96586c2cd4baf8"},
    {0x128c050,2048,"a63746a4e735a2d72b267876abecfe676c12aee5d4dc06a312a0355d4dcb4724"},
    {0x13062f0,2096,"032615dc53e819b7dfbd5b4f5704f2e72584399f681085f6eb027317356ee909"},
    {0x862280,128,"9a32987ccd89f34e76eebfbf830703bc1ba5072d7fc5a18da4936b1de7bbd792"},
};
}

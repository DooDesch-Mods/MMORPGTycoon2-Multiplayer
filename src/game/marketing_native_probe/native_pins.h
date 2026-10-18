#pragma once
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::marketing_native_probe {
struct NativePin { uintptr_t rva; size_t size; const char* sha; };
inline constexpr NativePin native_pins[]={
    {0x51a9b0,208,"0b1b493bfd7d71c6f28183c3933cff43012443b28c234b318be78f48d029178d"},
    {0xceabb0,256,"d2e8e7a77796417d55c2b2af969bd82e07d609b4fd185f924782ee87cc917479"},
    {0xb5d8d0,416,"e7f96ddf7ca57d05ccc14e74103463d751604c8c4d4113292dcbb3918977cd99"},
    {0x663880,4512,"77283df4065d71c42e339435077a772eb9418698b4985ea4f64fda732faf8c3c"},
    {0xc7cf80,256,"af749a0adcf24c37f93b0d2d2bcfcd6e5d265395240166f28f3c60281bfec507"},
    {0x5199e0,80,"99e600fc428393d6edb1d1f0ea3cb63891cc2c7295685d9fd405acfa402eada6"},
    {0x518290,5968,"7ac283fbadada906dd43eb1b8ea92646e3bfecafa06f192d41953b0a450f663b"},
    {0x51c640,656,"5c1626817461f4dece5e2b11c6ad90999d8a0f12771359f0999647cdf0372c96"},
};
}

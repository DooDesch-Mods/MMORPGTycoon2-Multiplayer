#pragma once
#include "mt2mp/core/hash.h"
#include <cstdint>
namespace mt2mp::game::tutorial_movement {
inline constexpr const char* sdl_sha256="34fdfba213c5d6265ebcc840ca7af966815854336c98e9d2d9c998440424c363";
template<class Memory>bool verify_contract(Memory& memory,uintptr_t image,uintptr_t sdl,std::string& error){
    if(!image||!sdl||image>UINTPTR_MAX-0x1800000||sdl>UINTPTR_MAX-0x400000){error="tutorial movement native image range invalid";return false;}
    struct Entry{uintptr_t base,rva;size_t size;const char* sha;};
    const Entry entries[]={
        {image,0x81a350,0x90,"e9a78adebe76a7b6d0b263943d4ea7adfb042fd6936ffe2e9e8d963ab4654e08"},
        {image,0x9ead30,0x5c0,"eb0835543f68f51786b697cb8c43596f13b312b72c6040ee801f442fa7c4d95b"},
        {image,0x9e7f20,0x200,"da7d0429b687dcbfc5ea6fef4a2bc2f6374decaa04b2fd2dbb44e751172c0eef"},
        {image,0x9e8120,0x130,"95c94e764e64f0ca1e75ea18ab70195bbb0592af252d69b4fa51828e053d3028"},
        {sdl,0x21c9c0,0x2b20,"a3ea932a8fc706f940b21c9d574a0a645bbd9f6d4ca23b0f89b09ffa35384f96"},
        {sdl,0x56ea0,0x350,"6bc02e965bfefc345ce2dfe2b212aee43343759b4d947fd14dd493869c16b68a"},
    };
    for(const auto& entry:entries){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(entry.base+entry.rva,bytes.data(),bytes.size(),error))return false;
        try{if(mt2mp::sha256(bytes)!=entry.sha){error="tutorial movement original code differs RVA="+std::to_string(entry.rva);return false;}}
        catch(const std::exception& cause){error=cause.what();return false;}
    }error.clear();return true;
}
}

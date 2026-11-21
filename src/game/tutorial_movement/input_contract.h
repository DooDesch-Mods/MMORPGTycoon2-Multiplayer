#pragma once
#include "mt2mp/core/hash.h"
#include <cstdint>
namespace mt2mp::game::tutorial_movement {
inline constexpr const char* sdl_sha256="34fdfba213c5d6265ebcc840ca7af966815854336c98e9d2d9c998440424c363";
template<class Memory>bool verify_contract(Memory& memory,uintptr_t image,uintptr_t sdl,std::string& error){
    if(!image||!sdl||image>UINTPTR_MAX-0x1800000||sdl>UINTPTR_MAX-0x400000){error="tutorial movement native image range invalid";return false;}
    struct Entry{uintptr_t base,rva;size_t size;const char* sha;};
    const Entry entries[]={
        {image,0x822940,0x90,"b2df4441e88f69579a404d8160fc1e67b815e72ce80ce8a299569791b00470d9"},
        {image,0x9f5130,0x61b,"40e3c8d0944484fb8f1ceface90fa55f4cd7f9a7c4d27a12827162ea377505d4"},
        {image,0x9f2320,0x200,"924b4c4f61d82850bd25bad97e1128479f138ab438d0dba84901866345a26184"},
        {image,0x9f2520,0x130,"99dd08d200276eb1b835d465e6ad261fc3e6ce4a900513f4fd765c4b0afd5bc4"},
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

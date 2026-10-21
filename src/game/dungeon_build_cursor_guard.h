#pragma once
#include "mt2mp/core/hash.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace mt2mp::game::dungeon_build_cursor {
struct Code {uintptr_t rva;size_t size,copied;const char* sha;std::array<unsigned char,8> prefix;};
inline constexpr std::array<Code,2> codes{{
    {0x5854e0,272,8,"999f3e95bab378665becdfd1dc7ab25df5a0a74cb644b7f3a4ff7e3998d51c84",{0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x28}},
    {0x585270,32,7,"929883407d459e01bce38129a5c17e30a7ace31205c325752bf768e0dfbe1303",{0x48,0x8b,0x89,0x48,0x01,0x00,0x00,0}}
}};
template<class Memory>
bool guard(Memory& memory,uintptr_t image,const Code& code,uintptr_t original,uintptr_t detour,bool enabled,std::string& error){
    const auto fail=[&](const char* cause){error=std::string(cause)+" RVA="+std::to_string(code.rva);return false;};
    std::vector<unsigned char> body(code.size);
    if(!image||!detour||!memory.copy(image+code.rva,body.data(),body.size(),error))return false;
    if(original){
        std::vector<unsigned char> trampoline(code.copied+14);uintptr_t resume=0;
        constexpr std::array<unsigned char,6> absolute{0xff,0x25,0,0,0,0};
        if(!memory.copy(original,trampoline.data(),trampoline.size(),error))return false;
        std::memcpy(&resume,trampoline.data()+code.copied+6,8);
        if(!std::equal(code.prefix.begin(),code.prefix.begin()+code.copied,trampoline.begin())||
           !std::equal(absolute.begin(),absolute.end(),trampoline.begin()+code.copied)||resume!=image+code.rva+code.copied)
            return fail("Dungeon Build Cursor original instruction boundary differs");
        if(enabled){
            if(body[0]!=0xe9)return fail("Dungeon Build Cursor enabled entry is not its owned near jump");
            int32_t displacement=0;std::memcpy(&displacement,body.data()+1,4);
            const auto relay=uintptr_t(int64_t(image+code.rva+5)+displacement);
            std::array<unsigned char,14> jump{};uintptr_t target=0;
            if(!memory.copy(relay,jump.data(),jump.size(),error))return false;
            std::memcpy(&target,jump.data()+6,8);
            if(!std::equal(absolute.begin(),absolute.end(),jump.begin())||target!=detour)
                return fail("Dungeon Build Cursor relay belongs to another detour");
            std::copy_n(trampoline.begin(),5,body.begin());
        }
    }else if(enabled)return fail("Dungeon Build Cursor enabled route lacks its original");
    if(mt2mp::sha256(body)!=code.sha)return fail("Dungeon Build Cursor complete original body differs");
    error.clear();return true;
}
}

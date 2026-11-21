#pragma once
#include <native_routes.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace mt2mp::game::npc_quest_list_ui_codes {
inline constexpr char game_sha[]="7efd8ee7b97f78362e7a2f8116b327fa6b756bdce23114b68143c4c9d2dd50eb";
struct Code {
    uintptr_t rva;
    size_t bytes;
    const char* sha;
    std::array<unsigned char,32> prefix;
};
inline constexpr Code npc{0x761550,1980,"5d6b40cb936d0f36973d15e2e50f71e11a6cef97092d08a139a36667fc113747",
    {0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x48,0x02,0x00,0x00,0x48,
     0x89,0xcb,0x48,0x89,0xd1,0x48,0x8d,0x74,0x24,0x50,0x48,0x89,0xd7,0xe8,0x2e,0x55}};
inline constexpr Code selector{0x796cb0,2152,"024063183d2560e168275c63da76acaa1b25efd76de3ea7e382a817f957e4bbe",
    {0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xc4,0x80,0x48,0x89,0xcb,0x48,0x89,0xd1,
     0x48,0x89,0xd6,0xe8,0xd8,0xfd,0x1d,0x00,0x48,0x8d,0x15,0x68,0xad,0xda,0x00,0x48}};
inline constexpr Code selector_with_alignment{selector.rva,2160,
    "ffa9c083d2b2cb40c5dfb09eb258ae3262e30a556bca6ebebb01c1282ddc815d",selector.prefix};
struct Owned {
    uintptr_t detour=0,original=0;
    uint64_t generation=0;
};
inline bool fail(std::string& e,const char* reason){e=reason;return false;}
template<size_t N>bool text(const char (&value)[N],const char* expected){
    const auto end=static_cast<const char*>(std::memchr(value,0,N));
    return end&&size_t(end-value)==std::strlen(expected)&&!std::memcmp(value,expected,size_t(end-value));
}
inline bool receipt(const MT2FW_MT2_RouteReceipt& route,uintptr_t entry,const Code& code,
                    const Owned& owned,std::string& e){
    if(route.size!=sizeof route||route.state!=MT2FW_MT2_ROUTE_ENABLED||route.entry!=entry||route.rva!=code.rva||
       !owned.detour||!owned.original||!owned.generation||route.detour!=owned.detour||route.trampoline!=owned.original||
       route.generation!=owned.generation||!text(route.owner,"doodesch.multiplayer")||!text(route.game_sha256,game_sha))
        return fail(e,"Quest menu exact owned route identity differs");
    if(!std::equal(code.prefix.begin(),code.prefix.end(),route.pristine))
        return fail(e,"Quest menu pristine route prefix differs");
    return true;
}

// read_route and verify_code must call the current Framework backend. The exact
// retained Q01 detour, original and generation come from that hook's installation.
// No bytes from an unauthenticated jump are restored before hashing the body.
template<class Memory,class ReadRoute,class VerifyCode,class Hash>
bool verify(Memory& m,uintptr_t image,const Code& code,const Owned& owned,
            ReadRoute&& read_route,VerifyCode&& verify_code,Hash&& hash,std::string& e){
    if(!image||image>UINTPTR_MAX-code.rva||code.bytes<32||code.bytes>4096)
        return fail(e,"Quest menu code window invalid");
    const auto entry=image+code.rva;
    if(entry>UINTPTR_MAX-(code.bytes-1))return fail(e,"Quest menu code window overflows");
    std::string bytes(code.bytes,'\0'),sha;
    if(!m.copy(entry,bytes.data(),bytes.size(),e)||!hash(bytes,sha,e))return false;
    if(sha==code.sha){
        if(owned.detour||owned.original||owned.generation)return fail(e,"Quest menu retained hook is unexpectedly pristine");
        return true;
    }
    MT2FW_MT2_RouteReceipt route{sizeof route};
    if(!read_route(entry,route,e)||!receipt(route,entry,code,owned,e))return false;
    std::array<unsigned char,19> original{};
    constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
    uintptr_t resume=0;
    if(!m.copy(owned.original,original.data(),original.size(),e))return false;
    std::memcpy(&resume,original.data()+11,sizeof resume);
    if(!std::equal(code.prefix.begin(),code.prefix.begin()+5,original.begin())||
       !std::equal(continuation.begin(),continuation.end(),original.begin()+5)||resume!=entry+5)
        return fail(e,"Quest menu retained original instructions or return continuation differ");
    if(std::memcmp(bytes.data(),route.current,32))return fail(e,"Quest menu live route bytes changed after readback");
    std::memcpy(bytes.data(),route.pristine,32);
    if(!hash(bytes,sha,e)||sha!=code.sha)return fail(e,"Quest menu immutable original body or alignment differs");
    return verify_code(entry,bytes.data(),bytes.size(),e);
}
template<class Memory,class ReadRoute,class VerifyCode,class Hash>
bool verify_npc_command(Memory& m,uintptr_t image,uintptr_t command,const Owned& owned,
                        ReadRoute&& read_route,VerifyCode&& verify_code,Hash&& hash,std::string& e){
    if(!image||image>UINTPTR_MAX-npc.rva||command!=image+npc.rva)
        return fail(e,"Quest menu NPC InfoWindow vtable command differs");
    return verify(m,image,npc,owned,read_route,verify_code,hash,e);
}
}

#pragma once
#include <native_routes.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace mt2mp::game::npc_quest_list_ui_codes {
inline constexpr char game_sha[]="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
struct Code {
    uintptr_t rva;
    size_t bytes;
    const char* sha;
    std::array<unsigned char,32> prefix;
};
inline constexpr Code npc{0x759ad0,1980,"af55e30d4239d069ca3663f63add803bc517ef93804478732595a93689b84555",
    {0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x48,0x02,0,0,0x48,
     0x89,0xcb,0x48,0x89,0xd1,0x48,0x8d,0x74,0x24,0x50,0x48,0x89,0xd7,0xe8,0x8e,0x3d}};
inline constexpr Code selector{0x78ed30,2152,"d3ec8dabcbbfad1149203fe03b4fcaec9a7b9e8adb1f6c66a11c2cb8543ec9e7",
    {0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xc4,0x80,0x48,0x89,0xcb,0x48,0x89,0xd1,
     0x48,0x89,0xd6,0xe8,0x38,0xeb,0x1d,0,0x48,0x8d,0x15,0x48,0xca,0xda,0,0x48}};
inline constexpr Code selector_with_alignment{selector.rva,2160,
    "ec2fa4a4576e5c3d2de04d8034cac5fddc8005b0636556f444ca3dbc1db31819",selector.prefix};
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

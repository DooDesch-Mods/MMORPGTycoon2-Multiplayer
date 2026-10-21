#include "native_contract.h"
#include "native_contract_bytes.h"
#include "mt2mp/core/hash.h"
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace mt2mp::action_session::ui_capture {
namespace {
struct Call {uintptr_t pc,target;Origin origin;};
constexpr std::array ui_calls{
    Call{0x42d89f,0x2d5520,Origin::NetworkWindow},Call{0x434117,0x2d5520,Origin::NetworkWindow},
    Call{0x59bc43,0x2d5520,Origin::DestroyCursor},Call{0x66de50,0x2d5520,Origin::BuildingWindow},
    Call{0x6cb617,0x2d5520,Origin::DistrictWindow},Call{0x6f4f82,0x2d5520,Origin::OtherWindow},
    Call{0x71900b,0x2d5520,Origin::OtherWindow},Call{0x74f583,0x2d5520,Origin::NetworkWindow},
    Call{0x759c4e,0x2d5520,Origin::NpcWindow},Call{0x7b3f7c,0x2d5520,Origin::OtherWindow},
    Call{0x82bbf6,0x2d5520,Origin::ZoneWindow},Call{0x74bf48,0x2d4d90,Origin::SelectionWindow}
};
bool bytes(Memory& memory,uintptr_t address,const uint8_t* expected,size_t count,std::string& cause) {
    std::vector<uint8_t> actual(count);
    if(!memory.copy(address,actual.data(),count,cause)){if(cause.empty())cause="UI Remove native bytes read failed without cause";return false;}
    if(std::memcmp(actual.data(),expected,count)){cause="UI Remove native full code bytes differ address="+std::to_string(address);return false;}
    return true;
}
bool executable(Memory& memory,uintptr_t address,Region& region,std::string& cause) {
    if(!address||!memory.region(address,region,cause)||!region.committed||!region.executable){if(cause.empty())cause="UI Remove code is not committed executable memory";return false;}
    return true;
}
bool relative(uintptr_t next,int32_t displacement,uintptr_t& target) {
    if(displacement<0){const auto offset=uint64_t(-int64_t(displacement));if(next<offset)return false;target=next-uintptr_t(offset);}
    else {if(next>UINTPTR_MAX-uint32_t(displacement))return false;target=next+uintptr_t(displacement);}
    return true;
}
bool monster_frames(Memory& memory,uintptr_t image,std::string& cause){
    struct Code {uintptr_t rva;size_t size;const char* sha;};
    constexpr Code codes[]{
        {0x7490c0,2144,"e6242e22cca51685d273afcf28ad8d9cfb8e6fc17046f4d56ea444125a743c6a"},
        {0x3bc9a0,768,"e6277d4d6740da834e76de74780af680e656cdd86461f8771bd9ddc72e03925d"}};
    for(const auto& code:codes){std::vector<unsigned char> body(code.size);
        if(!memory.copy(image+code.rva,body.data(),body.size(),cause))return false;
        if(mt2mp::sha256(body)!=code.sha){cause="Monster Remove exact original UI/Command frame body differs RVA="+std::to_string(code.rva);return false;}}
    uintptr_t target=0;
    if(!memory.copy(image+0x16c0770+0xb0,&target,sizeof target,cause))return false;
    if(target!=image+0x3bc9a0){cause="Monster Remove original Command virtual target differs";return false;}
    return true;
}
bool frames(Memory& memory,uintptr_t image,std::string& cause) {
    if(!bytes(memory,image+contract::destroy_prop_rva,contract::destroy_prop_bytes,sizeof contract::destroy_prop_bytes,cause)||
       !bytes(memory,image+contract::destroy_selection_rva,contract::destroy_selection_bytes,sizeof contract::destroy_selection_bytes,cause))return false;
    for(const auto& call:ui_calls) {
        std::array<uint8_t,5> actual{};
        if(!memory.copy(image+call.pc,actual.data(),actual.size(),cause))return false;
        int32_t displacement=0;std::memcpy(&displacement,actual.data()+1,4);uintptr_t target=0;
        if(actual[0]!=0xe8||!relative(image+call.pc+5,displacement,target)||target!=image+call.target){cause="UI Remove exact direct UI origin call differs pc="+std::to_string(call.pc);return false;}
    }
    return true;
}
}
bool validate_unpatched(Memory& memory,uintptr_t image,std::string_view sha,std::string& cause) {
    if(sha!=contract::exe_sha256||!image||image>UINTPTR_MAX-0x1748c68){cause="UI Remove exact native executable/image identity differs";return false;}
    Region target;
    if(!executable(memory,image+contract::destroy_props_rva,target,cause)||!target.image||target.writable||target.allocation_base!=image){if(cause.empty())cause="UI Remove target is not immutable native image code";return false;}
    return frames(memory,image,cause)&&bytes(memory,image+contract::destroy_props_rva,contract::destroy_props_bytes,sizeof contract::destroy_props_bytes,cause);
}
bool validate_trampoline(Memory& memory,uintptr_t image,uintptr_t trampoline,std::string& cause) {
    Region actual;if(!executable(memory,trampoline,actual,cause))return false;
    if(actual.image||actual.allocation_base==image){cause="UI Remove trampoline is not a private executable allocation";return false;}
    std::array<uint8_t,20> route{};
    if(!memory.copy(trampoline,route.data(),route.size(),cause))return false;
    uintptr_t continuation=0;std::memcpy(&continuation,route.data()+12,8);
    // Three complete two-byte pushes precede MinHook's absolute continuation.
    if(std::memcmp(route.data(),contract::destroy_props_bytes,6)||route[6]!=0xff||route[7]!=0x25||
       route[8]||route[9]||route[10]||route[11]||continuation!=image+contract::destroy_props_rva+6) {
        cause="UI Remove trampoline does not contain exact relocated pushes and original continuation";return false;
    }
    return true;
}
bool validate_own_enabled_route(Memory& memory,uintptr_t image,uintptr_t trampoline,uintptr_t own_entry,std::string& cause) {
    if(!validate_trampoline(memory,image,trampoline,cause)||!frames(memory,image,cause))return false;
    Region tramp,entry,target;
    if(!executable(memory,trampoline,tramp,cause)||!executable(memory,own_entry,entry,cause)||
       !executable(memory,image+contract::destroy_props_rva,target,cause))return false;
    if(!target.image||target.writable||target.allocation_base!=image){cause="UI Remove enabled target lost immutable native image protection";return false;}
    std::array<uint8_t,5> patch{};if(!memory.copy(image+contract::destroy_props_rva,patch.data(),patch.size(),cause))return false;
    int32_t displacement=0;std::memcpy(&displacement,patch.data()+1,4);uintptr_t relay=0;
    if(patch[0]!=0xe9||!relative(image+contract::destroy_props_rva+5,displacement,relay)){cause="UI Remove native target is not an owned E9 route";return false;}
    if(relay!=own_entry) {
        Region actual;std::array<uint8_t,14> jump{};
        if(!executable(memory,relay,actual,cause))return false;
        if(actual.allocation_base!=tramp.allocation_base){cause="UI Remove relay allocation differs actual="+std::to_string(actual.allocation_base)+" required="+std::to_string(tramp.allocation_base);return false;}
        if(!memory.copy(relay,jump.data(),jump.size(),cause))return false;
        uintptr_t destination=0;std::memcpy(&destination,jump.data()+6,8);
        if(jump[0]!=0xff||jump[1]!=0x25||jump[2]||jump[3]||jump[4]||jump[5]||destination!=own_entry){cause="UI Remove relay is not exact owner absolute hook route";return false;}
    }
    // The physical owned route is established before comparing every untouched byte.
    return bytes(memory,image+contract::destroy_props_rva+5,contract::destroy_props_bytes+5,sizeof contract::destroy_props_bytes-5,cause);
}
bool classify_origin(Memory& memory,uintptr_t image,uintptr_t return_slot,Origin& out,std::string& cause) {
    out=Origin::Foreign;uintptr_t immediate=0;
    if(!return_slot||!memory.copy(return_slot,&immediate,sizeof immediate,cause))return false;
    uintptr_t offset=0,target=0;
    if(immediate==image+0x2d55a8){offset=0xa0;target=0x2d5520;}
    else if(immediate==image+0x2d4e25){offset=0x60;target=0x2d4d90;}
    else {cause.clear();return true;}
    if(return_slot>UINTPTR_MAX-offset){cause="UI Remove native frame slot overflows address range";return false;}
    uintptr_t outer=0;if(!memory.copy(return_slot+offset,&outer,sizeof outer,cause))return false;
    if(target==0x2d5520&&outer==image+0x3bcb34){
        if(return_slot>UINTPTR_MAX-0x2d0){cause="Monster Remove InfoWindow frame slot overflows";return false;}
        uintptr_t window=0;
        if(!memory.copy(return_slot+0x2d0,&window,sizeof window,cause))return false;
        if(window==image+0x749454){
            if(!monster_frames(memory,image,cause))return false;
            out=Origin::MonsterWindow;cause.clear();return true;
        }
    }
    for(const auto& call:ui_calls)if(call.target==target&&outer==image+call.pc+5){out=call.origin;break;}
    cause.clear();return true;
}
bool read_native_array(Memory& memory,uintptr_t image,NativePropArray* native,ArrayObservation& out,std::string& cause) {
    out={};NativePropArray actual;
    if(!native||!memory.copy(uintptr_t(native),&actual,sizeof actual,cause)){if(cause.empty())cause="UI Remove native array address/read unavailable";return false;}
    if(actual.addresspoint!=image+contract::array_addresspoint_rva||actual.count<0||actual.capacity<actual.count||
       (actual.count&&(!actual.data||uintptr_t(actual.data)>UINTPTR_MAX-sizeof(uintptr_t)))) {
        cause="UI Remove native vsArray addresspoint/count/capacity/data differs";return false;
    }
    out={actual.addresspoint,uintptr_t(actual.data),0,actual.count,actual.capacity};
    if(actual.count==1&&!memory.copy(uintptr_t(actual.data),&out.object,sizeof out.object,cause))return false;
    cause.clear();return true;
}
}

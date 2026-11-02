#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mt2mp::game::toon_time_fence::camera_gate {
inline bool absolute_jump(const std::array<unsigned char,64>& code,size_t offset,uintptr_t expected){
    if(offset>code.size()-14||code[offset]!=0xff||code[offset+1]!=0x25||
       code[offset+2]||code[offset+3]||code[offset+4]||code[offset+5])return false;
    uintptr_t destination=0;std::memcpy(&destination,code.data()+offset+6,sizeof destination);
    return destination==expected;
}
inline bool camera_relocation(const std::array<unsigned char,30>& before,const std::array<unsigned char,64>& code,uintptr_t image){
    return std::equal(before.begin(),before.begin()+7,code.begin())&&absolute_jump(code,7,image+0x3ea469);
}
inline bool travel_relocation(const std::array<unsigned char,64>& code,uintptr_t image){
    return code[0]==0x75&&code[1]==0x0e&&absolute_jump(code,2,image+0x3b8140)&&absolute_jump(code,16,image+0x3b7b3d);
}
inline bool attribute_relocation(const std::array<unsigned char,64>& code,uintptr_t image){
    return code[0]==0x74&&code[1]==0x0e&&absolute_jump(code,2,image+0x3a1480)&&absolute_jump(code,16,image+0x3a0948);
}
inline bool npc_relocation(const std::array<unsigned char,64>& code,uintptr_t image,uintptr_t detour){
    return code[0]==0x80&&code[1]==0x38&&code[2]==0&&code[3]==0x74&&code[4]==0x0e&&
        absolute_jump(code,5,image+0x3c8e78)&&absolute_jump(code,19,image+0x3c8dc0)&&absolute_jump(code,33,detour);
}
}

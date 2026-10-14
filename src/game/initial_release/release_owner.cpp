#include "release_owner.h"
#include "release_mode_bytes.h"
#include <cstring>
#include <vector>
namespace mt2mp::game::initial_release {
bool owned_mode_route(Memory& m,uintptr_t image,uintptr_t detour,uintptr_t trampoline,std::string& e){
    e.clear();auto fail=[&](const char* cause){e=cause;return false;};
    if(!image||image>UINTPTR_MAX-0x4a2f96||!detour||!trampoline)return fail("release actual Mode hook owner absent");
    const uintptr_t entry=image+0x4a2f90;std::vector<uint8_t> actual(sizeof mode_bytes::body);
    if(!m.copy(entry,actual.data(),actual.size(),e))return false;
    if(actual[0]!=0xe9||std::memcmp(actual.data()+5,mode_bytes::body+5,actual.size()-5))return fail("release owned Mode physical entry/tail differs");
    int32_t displacement=0;std::memcpy(&displacement,actual.data()+1,4);
    uintptr_t relay=entry+5;
    if(displacement<0){const auto distance=uint64_t(-int64_t(displacement));if(distance>relay)return fail("release owned Mode relay underflow");relay-=uintptr_t(distance);}
    else {if(uintptr_t(displacement)>UINTPTR_MAX-relay)return fail("release owned Mode relay overflow");relay+=uintptr_t(displacement);}
    uint8_t jump[14]{};uintptr_t target=0;
    constexpr uint8_t absolute_jump[6]={0xff,0x25,0,0,0,0};
    if(!m.copy(relay,jump,sizeof jump,e))return false;std::memcpy(&target,jump+6,8);
    if(std::memcmp(jump,absolute_jump,6)||target!=detour)return fail("release Mode relay is not actual owned detour");
    uint8_t stolen[20]{};
    if(!m.copy(trampoline,stolen,sizeof stolen,e))return false;std::memcpy(&target,stolen+12,8);
    if(std::memcmp(stolen,mode_bytes::body,6)||std::memcmp(stolen+6,absolute_jump,6)||target!=entry+6)
        return fail("release owned Mode trampoline/stolen bytes/return differ");return true;
}
}

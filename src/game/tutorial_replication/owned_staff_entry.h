#pragma once
#include "owned_sequence_entry.h"
namespace mt2mp::game::tutorial_replication::staff {
inline constexpr std::array<unsigned char,16> allocate_entry={0x89,0x91,0xc4,0,0,0,0xc3,0x90,0x0f,0x1f,0x84,0,0,0,0,0};
template<class Copy>
bool verify_owned_allocate(const tutorial_sync::OwnedSequenceEntry& owner,uintptr_t target,Copy&& copy,std::string& error){
    auto fail=[&](const char* cause){error=cause;return false;};
    if(!owner.original_verified||!owner.enabled||!owner.target||!owner.detour||!owner.trampoline||owner.target!=target)
        return fail("Startingpoint worker allocation hook has no matching enabled owner");
    if(target>UINTPTR_MAX-16||owner.trampoline>UINTPTR_MAX-34)return fail("Startingpoint worker owned hook range overflow");
    std::array<unsigned char,16> entry{};std::array<unsigned char,34> trampoline{};
    if(!copy(target,entry.data(),entry.size(),error)||!copy(owner.trampoline,trampoline.data(),trampoline.size(),error))return false;
    if(entry[0]!=0xe9||std::memcmp(entry.data()+5,allocate_entry.data()+5,11))return fail("Startingpoint worker owned entry patch/suffix differs");
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,sizeof(displacement));
    const auto relay=owner.trampoline+20;
    if(displacement>=0){if(target+5>UINTPTR_MAX-uintptr_t(displacement)||target+5+uintptr_t(displacement)!=relay)return fail("Startingpoint worker owned entry relay differs");}
    else {const auto distance=uintptr_t(-int64_t(displacement));if(target+5<distance||target+5-distance!=relay)return fail("Startingpoint worker owned entry relay differs");}
    constexpr unsigned char jump[]={0xff,0x25,0,0,0,0};uintptr_t continuation=0,detour=0;
    std::memcpy(&continuation,trampoline.data()+12,8);std::memcpy(&detour,trampoline.data()+26,8);
    if(std::memcmp(trampoline.data(),allocate_entry.data(),6)||std::memcmp(trampoline.data()+6,jump,6)||continuation!=target+6)
        return fail("Startingpoint worker owned trampoline original/continuation differs");
    if(std::memcmp(trampoline.data()+20,jump,6)||detour!=owner.detour)return fail("Startingpoint worker owned trampoline detour differs");
    error.clear();return true;
}
}

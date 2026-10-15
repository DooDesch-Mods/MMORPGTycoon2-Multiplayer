#pragma once
#include "owned_sequence_entry.h"
namespace mt2mp::game::tutorial_sync {
inline constexpr std::array<unsigned char,16> advance_entry={0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xa0,0x00,0x00,0x00,0x48,0x89,0xcb};
template<class Copy>
bool verify_owned_advance_entry(const OwnedSequenceEntry& owner,uintptr_t target,Copy&& copy,std::string& error){
    auto fail=[&](const char* cause){error=cause;return false;};
    if(!owner.original_verified||!owner.enabled||!owner.target||!owner.detour||!owner.trampoline||owner.target!=target)
        return fail("tutorial Advance hook has no matching enabled owner");
    if(target>UINTPTR_MAX-16||owner.trampoline>UINTPTR_MAX-33)return fail("tutorial Advance hook range overflow");
    std::array<unsigned char,16> entry{};std::array<unsigned char,33> trampoline{};
    if(!copy(target,entry.data(),entry.size(),error)||!copy(owner.trampoline,trampoline.data(),trampoline.size(),error))return false;
    if(entry[0]!=0xe9||std::memcmp(entry.data()+5,advance_entry.data()+5,11))return fail("tutorial owned Advance entry patch/suffix differs");
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,4);const auto relay=owner.trampoline+19;
    if(displacement>=0){if(target+5>UINTPTR_MAX-uintptr_t(displacement)||target+5+uintptr_t(displacement)!=relay)return fail("tutorial owned Advance relay differs");}
    else {const auto distance=uintptr_t(-int64_t(displacement));if(target+5<distance||target+5-distance!=relay)return fail("tutorial owned Advance relay differs");}
    constexpr unsigned char jump[]={0xff,0x25,0,0,0,0};uintptr_t continuation=0,detour=0;
    std::memcpy(&continuation,trampoline.data()+11,8);std::memcpy(&detour,trampoline.data()+25,8);
    if(std::memcmp(trampoline.data(),advance_entry.data(),5)||std::memcmp(trampoline.data()+5,jump,6)||continuation!=target+5)
        return fail("tutorial owned Advance trampoline original/continuation differs");
    if(std::memcmp(trampoline.data()+19,jump,6)||detour!=owner.detour)return fail("tutorial owned Advance trampoline detour differs");
    error.clear();return true;
}
}

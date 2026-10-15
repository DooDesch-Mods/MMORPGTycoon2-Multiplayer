#pragma once
#include "owned_sequence_entry.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace mt2mp::game::tutorial_replication::staff_display {
static_assert(sizeof(uintptr_t)==8,"Staff display owned entry requires x64");
// This receipt is populated by the owner after checking the original entry,
// creating its own MinHook hook, and receiving that hook's trampoline pointer.
// It is not reconstructed from whatever jump happens to exist in game memory.
inline constexpr std::array<unsigned char,16> display_entry={
    0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xd8};

// The bundled MinHook copies three two-byte push instructions, then appends
// FF25 [RIP+0] back to target+6 and another FF25 relay to the registered detour.
template<class Copy>
bool verify_owned_display_entry(const tutorial_sync::OwnedSequenceEntry& owner,uintptr_t target,Copy&& copy,std::string& error){
    auto fail=[&](const char* cause){error=cause;return false;};
    if(!owner.original_verified||!owner.enabled||!owner.target||!owner.detour||!owner.trampoline||owner.target!=target)
        return fail("Staff display UpdateUI hook has no matching enabled owner");
    if(target>UINTPTR_MAX-16||owner.trampoline>UINTPTR_MAX-34)return fail("Staff display owned hook range overflow");
    std::array<unsigned char,16> entry{};std::array<unsigned char,34> trampoline{};
    if(!copy(target,entry.data(),entry.size(),error)||!copy(owner.trampoline,trampoline.data(),trampoline.size(),error))return false;
    if(entry[0]!=0xe9||std::memcmp(entry.data()+5,display_entry.data()+5,11))return fail("Staff display owned entry patch/suffix differs");
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,sizeof(displacement));
    const auto relay=owner.trampoline+20;
    if(displacement>=0){if(target+5>UINTPTR_MAX-uintptr_t(displacement)||target+5+uintptr_t(displacement)!=relay)return fail("Staff display owned entry relay differs");}
    else {const auto distance=uintptr_t(-int64_t(displacement));if(target+5<distance||target+5-distance!=relay)return fail("Staff display owned entry relay differs");}
    constexpr unsigned char jump[]={0xff,0x25,0,0,0,0};uintptr_t continuation=0,detour=0;
    std::memcpy(&continuation,trampoline.data()+12,8);std::memcpy(&detour,trampoline.data()+26,8);
    if(std::memcmp(trampoline.data(),display_entry.data(),6)||std::memcmp(trampoline.data()+6,jump,6)||continuation!=target+6)
        return fail("Staff display owned trampoline original/continuation differs");
    if(std::memcmp(trampoline.data()+20,jump,6)||detour!=owner.detour)return fail("Staff display owned trampoline detour differs");
    error.clear();return true;
}
}

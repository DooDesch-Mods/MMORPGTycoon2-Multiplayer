#pragma once
#include "owned_sequence_entry.h"
namespace mt2mp::game::tutorial_sync {
inline constexpr std::array<unsigned char,16> commit_entry={0x53,0x48,0x83,0xec,0x20,0x48,0x89,0xcb,0x48,0x8b,0x89,0xa8,0,0,0,0xe8};
template<class Copy>
bool verify_owned_commit_entry(const OwnedSequenceEntry& owner,uintptr_t target,Copy&& copy,std::string& error){
    auto fail=[&](const char* cause){error=cause;return false;};
    if(!owner.original_verified||!owner.enabled||!owner.target||!owner.detour||!owner.trampoline||owner.target!=target)
        return fail("tutorial Commit hook has no matching enabled owner");
    if(target>UINTPTR_MAX-16||owner.trampoline>UINTPTR_MAX-33)return fail("tutorial Commit hook range overflow");
    std::array<unsigned char,16> entry{};std::array<unsigned char,33> trampoline{};
    if(!copy(target,entry.data(),entry.size(),error)||!copy(owner.trampoline,trampoline.data(),trampoline.size(),error))return false;
    if(entry[0]!=0xe9||std::memcmp(entry.data()+5,commit_entry.data()+5,11))return fail("tutorial owned Commit entry patch/suffix differs");
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,4);const auto relay=owner.trampoline+19;
    if(displacement>=0){if(target+5>UINTPTR_MAX-uintptr_t(displacement)||target+5+uintptr_t(displacement)!=relay)return fail("tutorial owned Commit relay differs");}
    else {const auto distance=uintptr_t(-int64_t(displacement));if(target+5<distance||target+5-distance!=relay)return fail("tutorial owned Commit relay differs");}
    constexpr unsigned char jump[]={0xff,0x25,0,0,0,0};uintptr_t continuation=0,detour=0;
    std::memcpy(&continuation,trampoline.data()+11,8);std::memcpy(&detour,trampoline.data()+25,8);
    if(std::memcmp(trampoline.data(),commit_entry.data(),5)||std::memcmp(trampoline.data()+5,jump,6)||continuation!=target+5)
        return fail("tutorial owned Commit trampoline original/continuation differs");
    if(std::memcmp(trampoline.data()+19,jump,6)||detour!=owner.detour)return fail("tutorial owned Commit trampoline detour differs");
    error.clear();return true;
}
}

#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "mt2mp/core/hash.h"
#include "tutorial_replication/owned_sequence_entry.h"
#include <algorithm>

namespace mt2mp::game::tutorial_replication::start_intent {
inline constexpr size_t pay_size=0xb0;
inline constexpr const char* pay_sha256="2732cea4d51876f7eb3e4df2fe2d6742d3184196332d685d01034e9279886fa3";
// MinHook copies the original push/sub pair, then emits its continuation and
// owned relay. Verify that physical route before restoring the five bytes for
// the complete original-function fingerprint.
template<class Copy>
bool verify_pay(const tutorial_sync::OwnedSequenceEntry* owner,uintptr_t target,Copy&& copy,std::string& error,
                const char* expected_sha=pay_sha256){
    const auto fail=[&](const char* cause){error=cause;return false;};
    if(!target||target>UINTPTR_MAX-pay_size)return fail("Startingpoint Pay code range invalid");
    std::vector<unsigned char> body(pay_size);
    if(!copy(target,body.data(),body.size(),error))return false;
    if(owner){
        if(!owner->original_verified||!owner->enabled||owner->target!=target||!owner->detour||!owner->trampoline||owner->trampoline>UINTPTR_MAX-33)
            return fail("Startingpoint Pay has no exact enabled hook owner");
        std::array<unsigned char,33> trampoline{};
        if(!copy(owner->trampoline,trampoline.data(),trampoline.size(),error))return false;
        if(body[0]!=0xe9)return fail("Startingpoint Pay owned entry is not E9");
        int32_t displacement=0;std::memcpy(&displacement,body.data()+1,4);
        const auto relay=owner->trampoline+19;
        if(displacement>=0){if(target+5>UINTPTR_MAX-uintptr_t(displacement)||target+5+uintptr_t(displacement)!=relay)return fail("Startingpoint Pay owned relay differs");}
        else{const auto distance=uintptr_t(-int64_t(displacement));if(target+5<distance||target+5-distance!=relay)return fail("Startingpoint Pay owned relay differs");}
        constexpr unsigned char original[]={0x53,0x48,0x83,0xec,0x20},jump[]={0xff,0x25,0,0,0,0};
        uintptr_t continuation=0,detour=0;std::memcpy(&continuation,trampoline.data()+11,8);std::memcpy(&detour,trampoline.data()+25,8);
        if(std::memcmp(trampoline.data(),original,5)||std::memcmp(trampoline.data()+5,jump,6)||continuation!=target+5)
            return fail("Startingpoint Pay trampoline original/continuation differs");
        if(std::memcmp(trampoline.data()+19,jump,6)||detour!=owner->detour)return fail("Startingpoint Pay trampoline detour differs");
        std::copy_n(trampoline.data(),5,body.data());
    }
    try{if(mt2mp::sha256(body)!=expected_sha)return fail("Startingpoint Pay full original body differs");}
    catch(const std::exception& cause){error=std::string("Startingpoint Pay hash failed: ")+cause.what();return false;}
    error.clear();return true;
}
}

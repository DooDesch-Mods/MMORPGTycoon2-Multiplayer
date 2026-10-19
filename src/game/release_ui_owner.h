#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
namespace mt2mp::game::release_ui_owner {
struct Owner {uintptr_t entry=0,detour=0,trampoline=0;uint64_t generation=0;size_t stolen=0;std::vector<unsigned char> pristine;};
template<class Copy,class Hash>bool verify(const Owner& o,Copy&& copy,Hash&& hash,const std::string& sha,std::string& e){
    const auto fail=[&](const char* cause){e=cause;return false;};
    if(!o.entry||!o.detour||!o.trampoline||!o.generation||o.stolen<5||o.stolen>16||o.pristine.size()<32||o.pristine.size()>4096)return fail("Release UI physical owner/extent invalid");
    std::vector<unsigned char> body(o.pristine.size()),trampoline(o.stolen+28);
    if(!copy(o.entry,body.data(),body.size(),e)||!copy(o.trampoline,trampoline.data(),trampoline.size(),e))return false;
    if(body[0]!=0xe9||std::memcmp(body.data()+5,o.pristine.data()+5,o.stolen-5))return fail("Release UI patched prefix differs");
    int32_t delta=0;std::memcpy(&delta,body.data()+1,4);const int64_t next=int64_t(o.entry+5);if(next<0||delta< -next||next>INT64_MAX-int64_t(delta>0?delta:0))return fail("Release UI relay address overflow");
    const auto relay=uintptr_t(next+delta);constexpr unsigned char jump[]{0xff,0x25,0,0,0,0};uintptr_t continuation=0,detour=0;
    std::memcpy(&continuation,trampoline.data()+o.stolen+6,8);std::memcpy(&detour,trampoline.data()+o.stolen+20,8);
    if(relay!=o.trampoline+o.stolen+14||std::memcmp(trampoline.data(),o.pristine.data(),o.stolen)||std::memcmp(trampoline.data()+o.stolen,jump,6)||continuation!=o.entry+o.stolen||std::memcmp(trampoline.data()+o.stolen+14,jump,6)||detour!=o.detour)return fail("Release UI trampoline/continuation/relay differs");
    std::memcpy(body.data(),o.pristine.data(),o.stolen);if(body!=o.pristine||hash(body)!=sha)return fail("Release UI full restored original body differs");e.clear();return true;
}
}

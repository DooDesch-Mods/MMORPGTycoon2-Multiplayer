#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace mt2mp::game::developer_staff {
struct UiEntryOwner {uintptr_t target=0,detour=0,trampoline=0;bool original_verified=false,enabled=false;};
inline constexpr std::array<unsigned char,6> ui_prefix{0x41,0x56,0x41,0x55,0x41,0x54};
template<class Copy,class Hash>
bool verify_owned_ui(const UiEntryOwner& owner,uintptr_t target,Copy&& copy,Hash&& hash,std::string& e,
    size_t body_size=2560,const std::string& expected="d204e8e24dc14d66e170adae7b4b60433fa82baa6eecb81f65353f32d8c5a2af"){
    auto fail=[&](const char* cause){e=cause;return false;};
    if(!owner.original_verified||!owner.enabled||!target||owner.target!=target||!owner.detour||!owner.trampoline)return fail("Developer UI entry lacks exact enabled physical owner");
    if(body_size<6||body_size>4096||target>UINTPTR_MAX-body_size||owner.trampoline>UINTPTR_MAX-34)return fail("Developer UI entry body/trampoline extent invalid");
    std::vector<unsigned char> body(body_size);std::array<unsigned char,34> trampoline{};
    if(!copy(target,body.data(),body.size(),e)||!copy(owner.trampoline,trampoline.data(),trampoline.size(),e))return false;
    if(body[0]!=0xe9||body[5]!=ui_prefix[5])return fail("Developer UI entry patch/original sixth byte differs");
    int32_t delta=0;std::memcpy(&delta,body.data()+1,4);const auto next=target+5;uintptr_t relay=0;
    if(delta>=0){if(next>UINTPTR_MAX-uintptr_t(delta))return fail("Developer UI relay overflow");relay=next+uintptr_t(delta);}
    else{const auto distance=uintptr_t(-int64_t(delta));if(next<distance)return fail("Developer UI relay underflow");relay=next-distance;}
    constexpr unsigned char jump[]={0xff,0x25,0,0,0,0};uintptr_t continuation=0,detour=0;
    std::memcpy(&continuation,trampoline.data()+12,8);std::memcpy(&detour,trampoline.data()+26,8);
    if(relay!=owner.trampoline+20||std::memcmp(trampoline.data(),ui_prefix.data(),6)||std::memcmp(trampoline.data()+6,jump,6)||continuation!=target+6||
       std::memcmp(trampoline.data()+20,jump,6)||detour!=owner.detour)return fail("Developer UI registered relay/trampoline/continuation/detour differs");
    std::memcpy(body.data(),ui_prefix.data(),6);
    try{if(hash(body)!=expected)return fail("Developer UI complete original body fingerprint differs");}
    catch(const std::exception& ex){e=std::string("Developer UI body hash failed: ")+ex.what();return false;}
    e.clear();return true;
}
}

#pragma once
#include "developer_staff/window.h"
#include <array>
#include <cstring>
namespace mt2mp::game::bug_authority::manual_action {
using UiCommand=bool(*)(void*,void*);
enum class Command { Other,Button,Unreadable };
Command classify(uintptr_t,const void*,uint32_t&) noexcept;
struct Entry {uintptr_t target=0,detour=0,trampoline=0;bool enabled=false;};
inline constexpr std::array<unsigned char,8> prefix{0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x48};
template<class Copy,class Hash>
bool verify_entry(const Entry& owner,Copy&& copy,Hash&& hash,std::string& e){
    auto fail=[&](const char* cause){e=cause;return false;};
    if(!owner.enabled||!owner.target||!owner.detour||!owner.trampoline||owner.target>UINTPTR_MAX-320||owner.trampoline>UINTPTR_MAX-36)
        return fail("BugButton hook owner/extent invalid");
    std::vector<unsigned char> body(320);std::array<unsigned char,36> bridge{};
    if(!copy(owner.target,body.data(),body.size(),e)||!copy(owner.trampoline,bridge.data(),bridge.size(),e))return false;
    int32_t delta=0;std::memcpy(&delta,body.data()+1,4);const auto next=owner.target+5;
    uintptr_t relay=0;if(delta>=0){if(next>UINTPTR_MAX-uintptr_t(delta))return fail("BugButton relay overflow");relay=next+uintptr_t(delta);}
    else{const auto distance=uintptr_t(-int64_t(delta));if(next<distance)return fail("BugButton relay underflow");relay=next-distance;}
    constexpr unsigned char jump[]{0xff,0x25,0,0,0,0};uintptr_t continuation=0,detour=0;
    std::memcpy(&continuation,bridge.data()+14,8);std::memcpy(&detour,bridge.data()+28,8);
    if(body[0]!=0xe9||std::memcmp(body.data()+5,prefix.data()+5,3)||relay!=owner.trampoline+22||
       std::memcmp(bridge.data(),prefix.data(),prefix.size())||std::memcmp(bridge.data()+8,jump,6)||continuation!=owner.target+8||
       std::memcmp(bridge.data()+22,jump,6)||detour!=owner.detour)return fail("BugButton installed entry/trampoline differs");
    std::memcpy(body.data(),prefix.data(),prefix.size());
    return hash(body)=="b74314774d32edfffddf7f73ad97d9946ba868f819376459348cfb74b631a3fb"||fail("BugButton complete original body differs");
}
template<class Copy>
bool selected_bug(Copy&& copy,const developer_staff::Capture& c,uintptr_t receiver,uintptr_t& bug,std::string& e){
    bug=0;auto fail=[&](const char* cause){e=cause;return false;};
    auto at=[&](uintptr_t p,size_t offset,auto& value){
        if(!p||p>UINTPTR_MAX-offset||p+offset>UINTPTR_MAX-sizeof(value))return fail("BugButton selected owner address overflow");
        return copy(p+offset,&value,sizeof value,e);
    };
    uintptr_t window=0,view=0,ap=0,proxy=0,target=0,owner_proxy=0;
    if(!developer_staff::read_window(copy,c,window,e)||!at(window,0x558,view)||!view||view!=receiver)
        return e.empty()?fail("BugButton receiver is not current Development detail view"):false;
    if(!at(view,0,ap)||!at(view,0x460,bug)||!at(view,0x468,proxy))return false;
    if(ap!=c.image+0x172abf0||!bug||!proxy)return fail("BugButton selected detail class/weak receiver absent");
    if(!at(proxy,0,target))return false;
    if(bug>UINTPTR_MAX-0x18||target!=bug+0x18)return fail("BugButton selected weak target expired or differs");
    if(!at(bug,0x20,owner_proxy))return false;
    return owner_proxy==proxy||fail("BugButton selected Bug owns a different weak proxy");
}
// Only a genuine selected command is suppressed. Failures remain visible to
// the owner; they never fall through to a local multiplayer mutation.
template<class Calls>
bool capture_ui(bool bound,Command command,Calls& calls,std::string& e){
    if(!bound||command==Command::Other)return calls.original();
    if(command==Command::Unreadable){e="BugButton genuine command could not be read";return true;}
    if(!calls.admit(e))return true;
    if(!calls.retain(e)&&e.empty())e="BugButton admitted UI intent was not retained";return true;
}
}

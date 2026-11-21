#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "mt2mp/core/hash.h"
#include <array>

namespace mt2mp::game::tutorial_replication::start_intent {
struct Body {uintptr_t rva;size_t size;const char* sha;};
inline constexpr Body ui_bodies[]={
    {0x6cef90,0xa00,"63b0c84b87fcb7262ea9793b352282fd8153671426b63d531eae661707a8a634"},
    {0x6cac10,0x200,"7e45dc8f5664a282aa9b87feb393d408fd5a9e28017571ff4f3916ffa24fafa7"},
    {0x6ca190,0xa80,"e3affbc8bcc697f3e8769c573e48392f36f48147be9f52219d2713cca063fac5"},
    {0x759f70,0x20,"ed2ada29b3a6e0a4c7c7fe4601dbac7fd0c246dc3664a476502dbc5dc2287846"},
    {0x75def0,0x470,"2d1f0f87bd00a8df19df02c9577e4202249b6318e25640827d4d3a1ad03b51b8"}
};
template<class Copy>
bool verify_ui(uintptr_t image,Copy&& copy,std::string& error,bool(*owned)(uintptr_t,std::string&)=nullptr){
    if(!image||image>UINTPTR_MAX-0x1800000){error="Startingpoint StartTech UI code range invalid";return false;}
    for(const auto& entry:ui_bodies){if(entry.rva==0x6cef90&&owned){if(!owned(image+entry.rva,error))return false;continue;}std::vector<unsigned char> body(entry.size);if(!copy(image+entry.rva,body.data(),body.size(),error))return false;
        try{if(mt2mp::sha256(body)!=entry.sha){error="Startingpoint StartTech UI body differs RVA="+std::to_string(entry.rva);return false;}}
        catch(const std::exception& cause){error=std::string("Startingpoint StartTech UI hash failed: ")+cause.what();return false;}}
    error.clear();return true;
}
struct BorrowedString {const char* data;uint64_t size;std::array<unsigned char,16> storage{};};
static_assert(sizeof(BorrowedString)==32);
// Caller freshly rebinds the same actual Mode-owned window and tab container,
// verifies code, and confirms Host project adoption before this presentation.
// This is the original successful StartTech UI tail. It never calls Pay,
// SetCurrentDevProject or UICommand and owns no native project allocation.
inline DWORD native_present(uintptr_t image,uintptr_t window,uintptr_t tabs)noexcept{
    constexpr char staff[]="{dev_tab_staff}";const BorrowedString page{staff,sizeof(staff)-1,{}};
    __try{
        reinterpret_cast<void(*)(void*,const void*)>(image+0x759f70)(reinterpret_cast<void*>(tabs),&page);
        reinterpret_cast<void(*)(void*)>(image+0x6cac10)(reinterpret_cast<void*>(window));
        *reinterpret_cast<uintptr_t*>(window+0x610)=0;
        reinterpret_cast<void(*)(void*)>(image+0x6ca190)(reinterpret_cast<void*>(window));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}

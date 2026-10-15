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
    {0x6c7b70,0xa00,"d204e8e24dc14d66e170adae7b4b60433fa82baa6eecb81f65353f32d8c5a2af"},
    {0x6c37f0,0x200,"3a76de85376e94270199d340874d6f49f213247b6bc4f64723da965513ef668b"},
    {0x6c2d70,0xa80,"4f8f83b795d6f543ecd9559c854eaf4cdc4143d5b808593092704289a836238f"},
    {0x7524f0,0x20,"ed2ada29b3a6e0a4c7c7fe4601dbac7fd0c246dc3664a476502dbc5dc2287846"},
    {0x756470,0x470,"2dcd11e0c8ebe7fe1b3bb4a81b56a5c975b70071e8eb8b69f841c340cb7bf003"}
};
template<class Copy>
bool verify_ui(uintptr_t image,Copy&& copy,std::string& error){
    if(!image||image>UINTPTR_MAX-0x1800000){error="Startingpoint StartTech UI code range invalid";return false;}
    for(const auto& entry:ui_bodies){std::vector<unsigned char> body(entry.size);if(!copy(image+entry.rva,body.data(),body.size(),error))return false;
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
        reinterpret_cast<void(*)(void*,const void*)>(image+0x7524f0)(reinterpret_cast<void*>(tabs),&page);
        reinterpret_cast<void(*)(void*)>(image+0x6c37f0)(reinterpret_cast<void*>(window));
        *reinterpret_cast<uintptr_t*>(window+0x610)=0;
        reinterpret_cast<void(*)(void*)>(image+0x6c2d70)(reinterpret_cast<void*>(window));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}

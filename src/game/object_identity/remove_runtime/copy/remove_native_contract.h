#pragma once
#include "action_process.h"
#include "selection_witness.h"
#include <array>
#include <string_view>

namespace mt2mp::build_actions::private_native {
using ClearSelectionFn=void(*)(void*);
using UndoOpenFn=void*(*)(const void* native_loc_string);
using GraphLockFn=void(*)(void*);
using PropDestructorFn=void(*)(void*);
struct RemoveEntrypoints {
    DestroyProp destroy=nullptr;SetHighlightedProps highlights=nullptr;
    ClearSelectionFn clear=nullptr;UndoOpenFn open=nullptr;GraphLockFn lock=nullptr;PropDestructorFn prop_destructor=nullptr;
};
struct RemoveCodeGuard {uintptr_t rva;std::string_view hex;};
inline constexpr std::array remove_code_guards{
    RemoveCodeGuard{0x2d8c10,"5756534883c480488b05022711014889"},
    RemoveCodeGuard{0x2d71b0,"41554154555756534883ec28c7416000"},
    RemoveCodeGuard{0x2d77e0,"56534883ec284889cee8d27c8600"},
    RemoveCodeGuard{0x53fea0,"56534883ec2848833dd21b500100"},
    RemoveCodeGuard{0x54af60,"534883ec2080b90c010000004889cb"},
    RemoveCodeGuard{0x4d6360,"4157415641554154555756534881ec88010000"},
    RemoveCodeGuard{0x2d7c80,"e81b822600"},
    RemoveCodeGuard{0x5417ef,"e86c970000"},
    RemoveCodeGuard{0x251b5c,"ff5208"},
    RemoveCodeGuard{0x412a8a,"e861f0e3ff"},
};
bool resolve_remove(Memory&,uintptr_t image,std::string_view exact_exe_sha256,RemoveEntrypoints&,std::string&);
}

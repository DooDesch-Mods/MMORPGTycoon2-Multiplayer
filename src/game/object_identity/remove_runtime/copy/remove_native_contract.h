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
    RemoveCodeGuard{0x2d5520,"5756534883c480488b05f29d10014889"},
    RemoveCodeGuard{0x2d3960,"41554154555756534883ec28c7416000"},
    RemoveCodeGuard{0x2d3f90,"56534883ec284889cee852f28500"},
    RemoveCodeGuard{0x539990,"56534883ec2848833de210500100"},
    RemoveCodeGuard{0x5449a0,"534883ec2080b90c010000004889cb"},
    RemoveCodeGuard{0x4d05c0,"4157415641554154555756534881ec88010000"},
    RemoveCodeGuard{0x2d4448,"e843552600"},
    RemoveCodeGuard{0x53b2bf,"e8dc960000"},
    RemoveCodeGuard{0x24ec1c,"ff5208"},
    RemoveCodeGuard{0x40deca,"e8e10ce4ff"},
};
bool resolve_remove(Memory&,uintptr_t image,std::string_view exact_exe_sha256,RemoveEntrypoints&,std::string&);
}

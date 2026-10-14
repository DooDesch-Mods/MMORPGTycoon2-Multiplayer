#pragma once
#include "release_entry.h"
namespace mt2mp::game::initial_release {
bool owned_mode_route(Memory&,uintptr_t image,uintptr_t detour,uintptr_t trampoline,std::string&);
}

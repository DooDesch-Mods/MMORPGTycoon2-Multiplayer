#pragma once
#include "release_cycle_state.h"
#include <cstdint>
namespace mt2mp::game::release_cycle_native {
struct Receipt {uint32_t attempted=0,returned=0,exception=0;bool allowed=false,record_constructed=false,record_destroyed=false;};
uint32_t predicate(uintptr_t image,uintptr_t release,bool& allowed)noexcept;
uint32_t phase(uintptr_t image,uintptr_t celebration,release_cycle::Command)noexcept;
uint32_t show(uintptr_t image,uintptr_t manager)noexcept;
uint32_t script(uintptr_t image,Receipt&)noexcept;
uint32_t clock_pause(uintptr_t image,uintptr_t clock)noexcept;
uint32_t random_cas(uintptr_t target,uint64_t before,uint64_t after,bool& stored)noexcept;
}

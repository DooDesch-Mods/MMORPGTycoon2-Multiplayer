#pragma once
#include "intervention_state.h"
namespace mt2mp::game::interventions::native {
using Original=bool(*)(void*,const void*);
struct Receipt {bool attempted=false,returned=false,result=false;uint32_t exception=0;};
uint32_t matches(uintptr_t image,const void* native_string,Command,bool&)noexcept;
uint32_t matches_name(uintptr_t image,const void* native_string,const char* original_name,bool&)noexcept;
Receipt execute(Original,uintptr_t actor,Command)noexcept;
}

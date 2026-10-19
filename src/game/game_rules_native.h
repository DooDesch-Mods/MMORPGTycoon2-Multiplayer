#pragma once
#include "game_rules_state.h"
namespace mt2mp::game::game_rules::native {
struct Receipt {uint32_t exception=0,stores=0;uint8_t feature_calls=0,feature_events=0;bool attempted=false,returned=false;};
uint32_t find(uintptr_t image,uintptr_t tree,const std::string& key,uintptr_t& out)noexcept;
uint32_t parse(uintptr_t image,const std::string& text,int32_t& out,bool& parsed)noexcept;
uint32_t command(uintptr_t image,const void* record,const char* name,bool& matches)noexcept;
uint32_t buttons(uintptr_t image,void* view,bool enabled)noexcept;
Receipt change(uintptr_t image,const Pin&,const Intent&)noexcept;
}

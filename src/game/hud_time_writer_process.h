#pragma once
#include "hud_time_writer.h"
namespace mt2mp::game::hud_time {
bool process_store_item_state(void*,uintptr_t,int32_t,std::string&);
bool process_call_clock(void*,OriginalClockUpdate,void*,float,std::string&);
}

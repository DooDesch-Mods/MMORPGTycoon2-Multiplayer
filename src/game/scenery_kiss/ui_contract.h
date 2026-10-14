#pragma once
#include "scenery_state.h"
namespace mt2mp::scenery_kiss {
bool live_cursor(native::Memory&,uintptr_t image,uintptr_t receiver,bool& own,std::string&);
bool idle_single_update(native::Memory&,uintptr_t image,uintptr_t receiver,bool& allowed,std::string&);
bool single_press(native::Memory&,uintptr_t image,uintptr_t receiver,std::string&);
bool purchase_input(native::Memory&,int32_t actual_category,int32_t expected_category,uintptr_t position,const std::array<float,3>& expected,std::string&);
bool valid_effect_counts(uint32_t purchase_attempted,uint32_t purchase_returned,uint32_t add_attempted,uint32_t add_returned,uint32_t undo_attempted,uint32_t undo_returned,std::string&);
}

#pragma once
#include "move_kiss/move_kiss.h"
namespace mt2mp::game::ui_move_runtime::contract {
struct CleanupView {
    uintptr_t addresspoint=0,records=0,owned_first=0,owned_second=0,options=0,current_behaviour=0;
    int32_t count=-1,capacity=-1;
};
bool cleaned(uintptr_t image,const move_kiss::Intent&,const CleanupView&,std::string&);
bool pending(const nlohmann::json&,const action_session::Request&,uint64_t& revision,std::string&);
}

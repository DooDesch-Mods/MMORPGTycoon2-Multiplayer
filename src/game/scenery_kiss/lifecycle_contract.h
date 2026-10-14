#pragma once
#include "scenery_state.h"
#include "mt2mp/game/native_engine.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::scenery_kiss {
bool classify_scenery(uintptr_t image,uintptr_t addresspoint,uint64_t expected_uid,const game::native::BuildingState&,bool& supported,std::string&);
uintptr_t can_move_rva(uintptr_t image,uintptr_t addresspoint)noexcept;
bool place_cleanup(const nlohmann::json& receipt,bool scenery,std::string&);
bool metadata_matches(const nlohmann::json&,const game::native::BuildingState&);
}

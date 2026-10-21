#pragma once
#include "intervention_state.h"
namespace mt2mp::game::monster_move_key {
inline bool valid(std::string_view value){return monster_target_key::valid(value)||interventions::zone_monster_key(value);}
inline bool input(std::string_view value){return monster_target_key::input(value)||interventions::zone_monster_key(value);}
inline std::string canonical(const std::string& value){return interventions::zone_monster_key(value)?value:monster_target_key::canonical(value);}
inline bool request(const std::string& key,const std::string& action){return monster_target_key::request(key,action)||(action.empty()&&interventions::zone_monster_key(key));}
}

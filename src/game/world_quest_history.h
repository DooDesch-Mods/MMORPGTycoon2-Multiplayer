#pragma once
#include "typed_action/action_session.h"
#include "monster_quest_retirement.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::npc_probe::world_quest_history {
bool read(const action_session::Binding&,const std::string&,monster_quest_retirement::Witness&,std::string&,
          const monster_quest_retirement::Witness* previous=nullptr);
bool references(const action_session::Binding&,const std::string&,
                const monster_quest_retirement::Witness&,nlohmann::json&,std::string&);
}

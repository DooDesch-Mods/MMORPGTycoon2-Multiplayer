#pragma once
#include "typed_action/action_session.h"
#include "monster_quest_retirement.h"
namespace mt2mp::game::npc_probe::world_quest_history {
bool read(const action_session::Binding&,const std::string&,monster_quest_retirement::Witness&,std::string&,
          const monster_quest_retirement::Witness* previous=nullptr);
}

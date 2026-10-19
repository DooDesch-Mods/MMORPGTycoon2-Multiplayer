#pragma once
#include "typed_action/action_session.h"
#include "monster_quest_retirement.h"
namespace mt2mp::game::npc_probe::monster_action_runtime::quest_history {
bool remember(const action_session::Binding&,const action_session::Request&,uintptr_t,uintptr_t,
              const monster_quest_retirement::Witness&,const monster_quest_retirement::Witness&,std::string&);
void clear();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
}

#pragma once
#include "dungeon_instance_live.h"
#include "dungeon_scene_identity_runtime.h"
#include "typed_action/action_session.h"
namespace mt2mp::game::dungeon_instance_live {
struct Census {dungeon_scene_identity_runtime::World world;std::vector<Local> rows;};
// Frame capture holds the original stable save boundary around this batch.
bool take_current(const action_session::Context&,const action_session::Binding&,Census&,std::string&);
bool adopt_held(const action_session::Context&,const action_session::Binding&,const std::vector<State>&,
                Receipt&,std::string&);
}

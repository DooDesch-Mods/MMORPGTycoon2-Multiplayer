#pragma once
#include "typed_action/action_session.h"
#include "multiplayer_world/world_state.h"
#include <memory>

namespace mt2mp::game::ui_move_runtime::building_clearance {
struct Local;
struct Snapshot {std::shared_ptr<Local> local;};
bool capture(const action_session::Binding&,const action_session::Request&,Snapshot&,std::string&,
             const action_session::Evidence* authority=nullptr);
bool prepare_history(const action_session::Binding&,const action_session::Request&,const Snapshot& source,
                     Snapshot& before,std::string&,const action_session::Evidence* authority=nullptr);
bool settle(const action_session::Binding&,const action_session::Request&,const Snapshot& before,
            const Snapshot* source,uint64_t building_uid,uintptr_t inverse_graph,Snapshot& after,std::string&);
bool finish(const action_session::Binding&,const action_session::Request&,const Snapshot& before,
            Snapshot& after,const action_session::Evidence* authority,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,const Snapshot&,
             action_session::SceneryStrokeEffect&,std::string&);
bool publication(const Snapshot&,const world_state::Origin&,std::vector<std::string>& retired,
                 std::vector<world_state::LocalBinding>& restored,std::string&);
bool current_bindings(const Snapshot&,const world_state::Origin&,std::vector<world_state::LocalBinding>&,std::string&);
}

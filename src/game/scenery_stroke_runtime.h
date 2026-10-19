#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::ui_move_runtime::scenery_stroke {
bool enable(std::string&);
bool bulk(uintptr_t receiver,bool eraser,bool& handled,std::string&);
bool pending();void tick();void retire_session();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool publication(const action_session::Binding&,const action_session::Request&,action_session::SceneryStrokeEffect&,std::string&);
action_session::Outcome perform_history(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe_history(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire_history(const action_session::Binding&,const action_session::Request&,std::string&);
bool history_targets(const action_session::Binding&,const action_session::Request&,std::map<uint64_t,std::string>&,std::string&);
bool history_publication(const action_session::Binding&,const action_session::Request&,action_session::SceneryStrokeEffect&,std::string&);
nlohmann::json status();
}

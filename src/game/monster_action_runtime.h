#pragma once
#include "typed_action/action_session.h"
#include "ui_remove/ui_capture.h"
namespace mt2mp::move_kiss {struct Intent;}
namespace mt2mp::game::live_host_state {struct MonsterPort;}
namespace mt2mp::game::npc_probe::monster_action_runtime {
bool move_capture_binding(const std::string&,uint64_t,uint64_t,uintptr_t,uint64_t,action_session::BuildingState&,std::string&,std::string&);
bool submit_restored_move(const move_kiss::Intent&,const std::string&,std::string&);
action_session::Outcome move_perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool move_observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool move_retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool native_move_ui(const nlohmann::json&,nlohmann::json&,std::string&);
bool move_ui_captured(const move_kiss::Intent&,const std::string&,std::string&);
void move_ui_submitted(const std::string&);
void move_ui_failed(const std::string&);
bool native_remove_ui(const nlohmann::json&,nlohmann::json&,std::string&);
bool remove_local_binding(uintptr_t,uintptr_t,action_session::ui_capture::LocalBinding&,std::string&);
bool submit_remove(const action_session::ui_capture::Intent&,std::string&);
bool authorize_remove_replay(const action_session::ui_capture::Replay&,std::string&);
action_session::Outcome remove_perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool remove_observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool remove_retire(const action_session::Binding&,const action_session::Request&,std::string&);
live_host_state::MonsterPort& live_port();
bool enable(std::string&);
void tick();
bool pending_ui();
nlohmann::json status();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool native_ui(const nlohmann::json&,nlohmann::json&,std::string&);
}

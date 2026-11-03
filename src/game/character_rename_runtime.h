#pragma once
#include "typed_action/action_session.h"
#include "character_edit_ui_api.h"
struct MT2FW_MT2_ServiceV2;
namespace mt2mp::game::npc_probe::monster_action_runtime::character_rename {
bool enable(std::string&);
bool framework_bind(const MT2FW_MT2_ServiceV2&,std::string&);
bool framework_retire(std::string&);
bool retire_lobby(std::string&);
void tick();
bool pending_ui();
bool presentation(void*,void*,uintptr_t,bool&,std::string&);
bool native_ui(const nlohmann::json&,nlohmann::json&,std::string&);
bool read_native(const nlohmann::json&,nlohmann::json&,std::string&);
void tick_original_ui();
bool preview_presentation(void*,void*,void(*)(void*,void*),bool&,std::string&);
nlohmann::json status();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
}

#pragma once
#include "typed_action/action_session.h"
#include "vendor/nlohmann/json.hpp"
#include <string>

namespace mt2mp::game::ui_region_runtime {
bool enable(std::string&);
void tick();
bool enabled() noexcept;
nlohmann::json status();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,
    const action_session::Evidence* authority,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
action_session::Outcome perform_history(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe_history(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire_history(const action_session::Binding&,const action_session::Request&,std::string&);
bool retired_keys(const action_session::Binding&,const action_session::Request&,std::vector<std::string>&,std::string&);
bool settle_building_move(const action_session::Binding&,const action_session::Request&,uint64_t uid,
    const action_session::BuildingState& immediate,action_session::BuildingState& settled,nlohmann::json&,std::string&);
struct SelectionBuildingSettlement {std::string key;uint64_t uid=0;action_session::BuildingState immediate;};
bool settle_selection_buildings(const action_session::Binding&,const action_session::Request&,
    const std::vector<SelectionBuildingSettlement>&,std::vector<action_session::BuildingState>&,nlohmann::json&,std::string&);
bool query(int32_t region_id,nlohmann::json&,std::string&);
bool query_rng_census(nlohmann::json&,std::string&);
bool query_purchase_catalog(nlohmann::json&,std::string&);
bool styles_ui(const std::string& diagnostic_uuid,int32_t region_id,const std::array<std::string,4>& styles,nlohmann::json&,std::string&);
bool preset_ui(const std::string& diagnostic_uuid,int32_t region_id,const std::string& preset,nlohmann::json&,std::string&);
bool property_ui(const std::string& diagnostic_uuid,const action_session::RegionPropertyIntent&,nlohmann::json&,std::string&);
}

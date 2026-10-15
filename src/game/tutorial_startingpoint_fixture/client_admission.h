#pragma once
#include "controller_admission.h"
namespace mt2mp::game::tutorial_startingpoint_fixture {
inline bool client_controller_admission(const nlohmann::json& state,const std::string& pending_id,std::string& e){
    if(state.contains("error"))return controller_admission(state,e);
    if(!state.contains("initialized")||!state.at("initialized").is_boolean()||!state.at("initialized").get<bool>()){
        e="startingpoint Client fixture requires initialized authoritative Controller";return false;}
    if(pending_id.empty())return controller_admission(state,e);
    if(!state.contains("pending")||!state.at("pending").is_boolean()||!state.at("pending").get<bool>()||
       !state.contains("action_uuid")||!state.at("action_uuid").is_string()||state.at("action_uuid")!=pending_id||
       !state.contains("outcome")||!state.at("outcome").is_number_integer()||state.at("outcome")!=0){
        e="startingpoint Client fixture requires exact submitted pending action";return false;}
    // Only the exact newly submitted Pending receipt replaces idle here. All
    // started, retiring, locked, cause and live Unknown guards remain required.
    auto idle=state;idle["pending"]=false;return controller_admission(idle,e);
}
inline bool client_submission(const nlohmann::json& previous,const nlohmann::json& after,std::string& id,std::string& e){
    if(!after.value("client_commit_submitted",false)||!after.value("client_commit_suppressed",false)||
       !after.value("client_commit_cause",std::string{}).empty()){
        e="startingpoint Client Commit intent not submitted: "+after.value("client_commit_cause",std::string{});return false;}
    id=after.value("client_commit_action_uuid",std::string{});
    if(id.size()!=36||id==previous.value("client_commit_action_uuid",std::string{})){
        e="startingpoint Client Commit requires a fresh submitted action UUID";return false;}
    return true;
}
}

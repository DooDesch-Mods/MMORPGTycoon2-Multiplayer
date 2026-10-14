#include "admission.h"
#include <cmath>
#include <limits>
namespace mt2mp::game::placement_ui_diagnostics {
namespace {
bool fail(std::string& e,const std::string& s){e=s;return false;}
bool uuid(const std::string& s){if(s.size()!=36)return false;for(size_t i=0;i<s.size();++i){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}else if(!((s[i]>='a'&&s[i]<='f')||(s[i]>='0'&&s[i]<='9')))return false;}return true;}
bool positive(const nlohmann::json& v,uint64_t& out){if(!v.is_number_integer())return false;if(v.is_number_unsigned())out=v.get<uint64_t>();else{auto n=v.get<int64_t>();if(n<=0)return false;out=static_cast<uint64_t>(n);}return out!=0;}
bool unknown(Job& j,std::string& e,const std::string& s){j.receipt.phase=placement_ui_probe::Phase::Unknown;j.receipt.cause=s;return fail(e,s);}
}
bool parse_token(const nlohmann::json& a,std::string& token,std::string& e){if(!a.is_object()||a.size()!=1||!a.contains("acceptance_token")||!a.at("acceptance_token").is_string())return fail(e,"build_ui_token_fields_invalid");token=a.at("acceptance_token").get<std::string>();return uuid(token)||fail(e,"build_ui_token_not_canonical_uuid");}
bool parse_input(const nlohmann::json& a,Input& out,std::string& e){
    out={};if(!a.is_object()||a.size()!=9)return fail(e,"build_ui_requires_exact_nine_fields");
    for(const auto* name:{"acceptance_token","snapshot_uuid","type","variant"})if(!a.contains(name)||!a.at(name).is_string())return fail(e,std::string("build_ui_string_invalid:")+name);
    out.native.token=a.at("acceptance_token").get<std::string>();out.snapshot_uuid=a.at("snapshot_uuid").get<std::string>();
    if(!uuid(out.native.token)||!uuid(out.snapshot_uuid))return fail(e,"build_ui_uuid_not_canonical");
    for(const auto* name:{"expected_world_epoch","expected_generation"})if(!a.contains(name))return fail(e,std::string("build_ui_uint64_missing:")+name);
    if(!positive(a.at("expected_world_epoch"),out.epoch)||!positive(a.at("expected_generation"),out.generation))return fail(e,"build_ui_epoch_generation_not_positive_uint64");
    out.native.type=a.at("type").get<std::string>();out.native.variant=a.at("variant").get<std::string>();
    if(out.native.type.empty()||out.native.type.size()>256||out.native.type.find('\0')!=std::string::npos||out.native.variant.empty()||out.native.variant.size()>256||out.native.variant.find('\0')!=std::string::npos)return fail(e,"build_ui_type_variant_invalid");
    size_t i=0;for(const auto* name:{"x","y","z"}){if(!a.contains(name)||!a.at(name).is_number())return fail(e,std::string("build_ui_coordinate_invalid:")+name);const double v=a.at(name).get<double>();if(!std::isfinite(v)||std::abs(v)>std::numeric_limits<float>::max())return fail(e,std::string("build_ui_coordinate_outside_finite_float32:")+name);out.native.position[i++]=static_cast<float>(v);}
    e.clear();return true;
}
bool bind_release(Job& j,uintptr_t receiver,const placement_ui_probe::NativePose& pose,std::string& e){
    using placement_ui_probe::Phase;
    if(!j.reserved||receiver!=j.binding.behaviour||j.input.native.family!=placement_ui_probe::Family::Scenery||
       (j.receipt.phase!=Phase::Attempted&&j.receipt.phase!=Phase::ArmedAwaitingGameUpdate)||j.receipt.capture_count)return unknown(j,e,"scenery_ui_release_binding_or_phase_differs_no_replay");
    for(float v:pose.position)if(!std::isfinite(v))return unknown(j,e,"scenery_ui_release_position_not_finite");
    for(float v:pose.rotation)if(!std::isfinite(v))return unknown(j,e,"scenery_ui_release_rotation_not_finite");
    for(float v:pose.scale)if(!std::isfinite(v))return unknown(j,e,"scenery_ui_release_scale_not_finite");
    if(j.receipt.native_pose_measured&&(j.receipt.native_pose.position!=pose.position||j.receipt.native_pose.rotation!=pose.rotation||j.receipt.native_pose.scale!=pose.scale))return unknown(j,e,"scenery_ui_release_pose_changed_no_replay");
    j.receipt.native_pose=pose;j.receipt.native_pose_measured=true;j.receipt.phase=Phase::ArmedAwaitingGameUpdate;return true;
}
bool bind_capture(Job& j,uintptr_t receiver,const std::string& id,const action_session::BuildingState& s,std::string& e){
    using placement_ui_probe::Phase;
    if(!j.reserved||receiver!=j.binding.behaviour||j.receipt.phase!=Phase::ArmedAwaitingGameUpdate||j.receipt.capture_count||!uuid(id))return unknown(j,e,"build_ui_capture_binding_or_phase_differs_no_replay");
    const auto& r=j.input.native;
    if(r.family==placement_ui_probe::Family::Scenery){
        const auto& pose=j.receipt.native_pose;
        if(!j.receipt.native_pose_measured||s.type!=r.type||s.variant!=r.variant||!s.alive||std::array<float,3>{s.x,s.y,s.z}!=pose.position||s.rotation!=pose.rotation||s.scale!=pose.scale)return unknown(j,e,"scenery_ui_capture_differs_from_actual_release_pose_no_replay");
    }else if(s.type!=r.type||s.variant!=r.variant||s.x!=r.position[0]||s.y!=r.position[1]||s.z!=r.position[2]||s.rotation!=std::array<float,4>{0,0,0,1}||s.scale!=std::array<float,3>{1,1,1}||!s.alive)return unknown(j,e,"build_ui_capture_scalars_differ_no_replay");
    j.receipt.action_uuid=id;j.receipt.capture_count=1;j.receipt.phase=Phase::CapturedAwaitingHost;e.clear();return true;
}
bool observe_confirmation(Job& j,const nlohmann::json& c,const nlohmann::json& receipts,std::string& e,bool aggregate_host_acks){
    using placement_ui_probe::Phase;
    if(j.receipt.phase==Phase::Unknown)return fail(e,j.receipt.cause);
    if(j.receipt.phase==Phase::HostConfirmed){e.clear();return true;}
    if(!j.receipt.capture_count){e.clear();return true;}
    if(!c.value("started",false)||c.value("action_uuid",std::string{})!=j.receipt.action_uuid)return unknown(j,e,"build_ui_controller_action_missing_or_overwritten_no_replay");
    const int outcome=c.value("outcome",3);
    if(outcome==2||outcome==3||c.value("locked",false))return unknown(j,e,"build_ui_host_action_failed: "+c.value("cause",std::string{}));
    if(c.value("pending",true)||outcome==0){e.clear();return true;}
    const auto required=c.value("required_mask",uint32_t{}),ack=c.value("ack_mask",uint32_t{});
    // Only the Host aggregates peer ACKs; Client completion follows its validated Host packet.
    if(outcome!=1||!required||(aggregate_host_acks&&required!=ack)||!receipts.is_object()||!receipts.contains(j.receipt.action_uuid))return unknown(j,e,"build_ui_complete_ack_or_local_native_receipt_missing");
    const auto& n=receipts.at(j.receipt.action_uuid);
    if(!n.value("confirmed",false)||!n.value("undo_confirmed",false)||!n.value("native_uid",uint64_t{})||n.value("purchase",0)!=1||n.value("add",0)!=1||n.value("undo",0)!=1||n.value("exception",uint32_t{}))return unknown(j,e,"build_ui_local_native_receipt_incomplete: "+n.value("error",std::string{}));
    j.receipt.host_confirmed_count=1;j.receipt.phase=Phase::HostConfirmed;e.clear();return true;
}
bool may_read_fresh_pose(const Job& j,uint64_t completed)noexcept{return j.receipt.capture_count==1&&j.capture_frame<UINT64_MAX-1&&completed>j.capture_frame+1;}
}

#include "contract.h"
#include "../snapshot_building_key.h"
#include <cmath>
#include <limits>
#include <regex>
namespace mt2mp::game::move_ui_probe {
namespace {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool uuid(const std::string& s){static const std::regex pattern("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");return std::regex_match(s,pattern);}
bool key(const std::string& s){static const std::regex action("action:[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");return std::regex_match(s,action)||snapshot_building_key::valid(s);}
bool positive(const nlohmann::json& v,uint64_t& n){if(!v.is_number_integer())return false;if(v.is_number_unsigned())n=v.get<uint64_t>();else{auto value=v.get<int64_t>();if(value<=0)return false;n=uint64_t(value);}return n!=0;}
bool unknown(Bound& b,std::string& e,const std::string& cause){b.receipt.phase=Phase::Unknown;b.receipt.cause=cause;return fail(e,cause);}
}
bool parse_token(const nlohmann::json& a,std::string& token,std::string& e){if(!a.is_object()||a.size()!=1||!a.contains("acceptance_token")||!a.at("acceptance_token").is_string())return fail(e,"move_ui_read_exact_token_required");token=a.at("acceptance_token").get<std::string>();return uuid(token)||fail(e,"move_ui_token_not_canonical");}
bool parse(const nlohmann::json& a,Input& out,std::string& e){out={};if(!a.is_object()||a.size()!=8)return fail(e,"move_ui_exact_eight_fields_required");
    for(const auto* name:{"acceptance_token","snapshot_uuid","record_key"})if(!a.contains(name)||!a.at(name).is_string())return fail(e,std::string("move_ui_string_invalid:")+name);
    out.token=a.at("acceptance_token").get<std::string>();out.snapshot=a.at("snapshot_uuid").get<std::string>();out.key=a.at("record_key").get<std::string>();
    if(!uuid(out.token)||!uuid(out.snapshot)||!key(out.key))return fail(e,"move_ui_uuid_or_record_key_invalid");
    if(!a.contains("expected_world_epoch")||!a.contains("expected_generation")||!positive(a.at("expected_world_epoch"),out.epoch)||!positive(a.at("expected_generation"),out.generation))return fail(e,"move_ui_positive_epoch_generation_required");
    size_t index=0;for(const auto* name:{"x","y","z"}){if(!a.contains(name)||!a.at(name).is_number())return fail(e,std::string("move_ui_coordinate_invalid:")+name);const double v=a.at(name).get<double>();if(!std::isfinite(v)||std::abs(v)>std::numeric_limits<float>::max())return fail(e,std::string("move_ui_coordinate_not_finite_float32:")+name);out.position[index++]=float(v);}e.clear();return true;
}
bool bind_capture(Bound& b,const move_kiss::Intent& i,const std::string& id,std::string& e){
    if(b.receipt.phase!=Phase::Armed||b.receipt.captures||!uuid(id)||i.origin!=b.armed.origin||i.behaviour!=b.armed.behaviour||i.record!=b.armed.record||i.ghost!=b.armed.ghost||i.target!=b.armed.target||i.local_uid!=b.armed.local_uid||i.object_key!=b.input.key||i.before!=b.armed.before||i.after!=b.armed.after)return unknown(b,e,"move_ui_exact_native_capture_binding_differs_no_replay");
    b.receipt.action_uuid=id;b.receipt.captures=1;b.receipt.phase=Phase::Captured;e.clear();return true;
}
bool confirmation(Bound& b,const nlohmann::json& c,const nlohmann::json& n,bool host,std::string& e){
    if(b.receipt.phase==Phase::Unknown)return fail(e,b.receipt.cause);
    if(b.receipt.phase==Phase::Confirmed){e.clear();return true;}
    if(!b.receipt.captures||!b.submitted){e.clear();return true;}
    if(!c.value("started",false)||c.value("action_uuid",std::string{})!=b.receipt.action_uuid)return unknown(b,e,"move_ui_controller_uuid_missing_or_overwritten");
    const auto outcome=c.value("outcome",3);if(outcome==2||outcome==3||c.value("locked",false))return unknown(b,e,"move_ui_authoritative_failure: "+c.value("cause",std::string{}));
    if(c.value("pending",true)||outcome==0){e.clear();return true;}
    const auto required=c.value("required_mask",uint32_t{});if(outcome!=1||!required||(host&&c.value("ack_mask",uint32_t{})!=required))return unknown(b,e,"move_ui_host_completion_incomplete");
    if(n.value("token",std::string{})!=b.receipt.action_uuid||n.value("record_key",std::string{})!=b.input.key||n.value("native_uid",uint64_t{})!=b.armed.local_uid||!n.value("confirmed",false)||!n.value("undo_confirmed",false)||!n.value("selection_confirmed",false)||!n.value("cleanup_confirmed",false)||!n.value("move_attempted",false)||!n.value("move_returned",false)||!n.value("dtor_returned",false)||n.value("exception",uint32_t{})||n.value("cleanup_exception",uint32_t{}))return unknown(b,e,"move_ui_exact_local_native_receipt_incomplete: "+n.value("error",std::string{}));
    b.receipt.phase=Phase::Confirmed;e.clear();return true;
}
}

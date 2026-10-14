#include "tool_cancel.h"
#include <regex>
namespace mt2mp::game::native_tool_prepare {
using json=nlohmann::json;
namespace {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool uuid(const std::string& value){static const std::regex pattern("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");return std::regex_match(value,pattern);}
}
bool parse(const json& a,Context& out,std::string& e){out={};if(!a.is_object()||a.size()!=4||!a.contains("acceptance_token")||!a.contains("snapshot_uuid")||!a.contains("expected_world_epoch")||!a.contains("expected_generation")||!a.at("acceptance_token").is_string()||!a.at("snapshot_uuid").is_string())return fail(e,"tool_prepare_exact_four_scalar_fields_required");
    out.token=a.at("acceptance_token").get<std::string>();out.snapshot=a.at("snapshot_uuid").get<std::string>();if(!uuid(out.token)||!uuid(out.snapshot))return fail(e,"tool_prepare_canonical_UUIDs_required");
    size_t index=0;for(const auto* name:{"expected_world_epoch","expected_generation"}){const auto& value=a.at(name);if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()==0:value.get<int64_t>()<=0))return fail(e,"tool_prepare_positive_uint64_required");if(index++==0)out.epoch=value.get<uint64_t>();else out.generation=value.get<uint64_t>();}return true;
}
bool clean(bool build,const Readback& r,std::string& e,bool prior_tool){if(!r.actual_default)return fail(e,"tool_prepare_actual_Default_not_current");if(prior_tool&&(r.placing||r.pending||r.cancel))return fail(e,"tool_prepare_native_placement_flags_not_clear required=0 placing="+std::to_string(r.placing)+" pending="+std::to_string(r.pending)+" cancel="+std::to_string(r.cancel));if(prior_tool&&(r.owned_a||(build?r.owned_b!=0:r.records!=0)))return fail(e,"tool_prepare_native_preview_or_Move_records_retained first_present="+std::to_string(r.owned_a!=0)+" second_present="+std::to_string(r.owned_b!=0)+" records="+std::to_string(r.records));if(r.manager_progress||r.manager_queue||r.edits!=0||r.saves!=0)return fail(e,"tool_prepare_native_selection_or_world_not_idle required=0 progress="+std::to_string(r.manager_progress)+" queue="+std::to_string(r.manager_queue)+" edits="+std::to_string(r.edits)+" saves="+std::to_string(r.saves));e.clear();return true;}
bool unresolved_effect(uint32_t attempted,bool confirmed)noexcept{return attempted!=0&&!confirmed;}
}

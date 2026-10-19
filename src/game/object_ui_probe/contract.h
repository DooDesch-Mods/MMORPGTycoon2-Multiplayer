#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <regex>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::object_ui_probe {
using Json=nlohmann::json;
struct Input {std::string token,snapshot,command;uint64_t epoch=0,generation=0;std::vector<std::string> keys;std::array<float,3> position{};};
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
inline bool uuid(const std::string& value){static const std::regex pattern("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");return std::regex_match(value,pattern);}
inline bool position(const Json&,std::array<float,3>&,std::string&);
inline bool parse(const Json& args,Input& out,std::string& error)try{
    out={};if(!args.is_object())return fail(error,"object_ui_object_required");
    Input next;next.token=args.at("acceptance_token").get<std::string>();next.snapshot=args.at("snapshot_uuid").get<std::string>();next.command=args.at("command").get<std::string>();
    if(!uuid(next.token)||!uuid(next.snapshot))return fail(error,"object_ui_token_or_snapshot_uuid_invalid");
    for(const char* name:{"expected_world_epoch","expected_generation"})if(!args.at(name).is_number_unsigned()&&(!args.at(name).is_number_integer()||args.at(name).get<int64_t>()<=0))return fail(error,"object_ui_positive_native_lifetime_required");
    next.epoch=args.at("expected_world_epoch").get<uint64_t>();next.generation=args.at("expected_generation").get<uint64_t>();
    if(!next.epoch||!next.generation)return fail(error,"object_ui_positive_native_lifetime_required");
    next.keys=args.at("record_keys").get<std::vector<std::string>>();std::set<std::string> unique;
    if(next.keys.empty()||next.keys.size()>32)return fail(error,"object_ui_selected_key_count_outside_1_32");
    for(const auto& key:next.keys)if(key.empty()||key.size()>256||key.find('\0')!=std::string::npos||!unique.insert(key).second)return fail(error,"object_ui_selected_key_empty_duplicate_or_invalid");
    if(next.command!="Duplicate"&&next.command!="Group"&&next.command!="Ungroup")return fail(error,"object_ui_command_not_supported");
    if(args.size()!=(next.command=="Duplicate"?7u:6u))return fail(error,"object_ui_exact_command_fields_required");
    if(next.command=="Duplicate"&&next.keys.size()!=1)return fail(error,"object_ui_duplicate_requires_one_current_top_level_object");
    if(next.command=="Duplicate"&&!position(args.at("position"),next.position,error))return false;
    out=std::move(next);error.clear();return true;
}catch(const std::exception& cause){return fail(error,std::string("object_ui_request: ")+cause.what());}
inline bool position(const Json& args,std::array<float,3>& out,std::string& error)try{
    if(!args.is_array()||args.size()!=3)return fail(error,"object_ui_duplicate_position_requires_three_scalars");
    for(size_t n=0;n<3;++n){if(!args[n].is_number())return fail(error,"object_ui_duplicate_position_not_numeric");const double value=args[n].get<double>();if(!std::isfinite(value)||std::abs(value)>double(std::numeric_limits<float>::max()))return fail(error,"object_ui_duplicate_position_not_finite_float32");out[n]=float(value);}return true;
}catch(const std::exception& cause){return fail(error,std::string("object_ui_position: ")+cause.what());}
struct Counters {uint64_t captures=0,submitted=0,rejected=0,revision=0;};
enum class Completion {Waiting,Confirmed,Unknown};
inline Completion completion(const Counters& before,const Counters& after,size_t operations,bool native_confirmed,bool retired,bool pending,bool locked,
                             bool same_action,int outcome,uint64_t elapsed,const std::string& native_cause,std::string& error){
    if(before.captures==UINT64_MAX||before.submitted==UINT64_MAX||before.revision==UINT64_MAX||
       after.rejected!=before.rejected||after.captures>before.captures+1||after.submitted>before.submitted+1||operations>1||locked||!native_cause.empty()){
        error=native_cause.empty()?"object_ui_capture_or_authoritative_operation_ambiguous_rejected_or_locked":native_cause;return Completion::Unknown;
    }
    if(operations==1&&native_confirmed&&retired&&!pending){
        if(after.captures!=before.captures+1||after.submitted!=before.submitted+1||after.revision!=before.revision+1||!same_action||outcome!=1){
            error="object_ui_exact_capture_submission_and_authority_completion_differ";return Completion::Unknown;
        }
        error.clear();return Completion::Confirmed;
    }
    if(elapsed>30000){error="object_ui_observation_timeout_ms>30000_no_replay";return Completion::Unknown;}
    error.clear();return Completion::Waiting;
}
}

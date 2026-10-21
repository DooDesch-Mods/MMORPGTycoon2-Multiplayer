#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>
namespace mt2mp::game::scenario_intro {
struct State {bool active=false,completed=false,retiring=false;int32_t phase=0,starting_points=0;bool operator==(const State&)const=default;};
inline bool valid(const State& v,std::string& e){if((v.active&&v.completed)||(!v.active&&(v.retiring||v.phase||v.starting_points))){e="Intro Scenario active/completed/native scalar state conflicts";return false;}e.clear();return true;}
inline nlohmann::json encode(const State& v){return {{"active",v.active},{"completed",v.completed},{"retiring",v.retiring},{"phase",v.phase},{"starting_points",v.starting_points}};}
inline bool decode(const nlohmann::json& j,State& out,std::string& e)try{
    if(!j.is_object()||j.size()!=5||!j.at("active").is_boolean()||!j.at("completed").is_boolean()||!j.at("retiring").is_boolean())throw std::runtime_error("exact Intro Scenario fields required");
    const auto number=[](const nlohmann::json& v){if(!v.is_number_integer()||(v.is_number_unsigned()?v.get<uint64_t>()>uint64_t(INT32_MAX):v.get<int64_t>()<INT32_MIN||v.get<int64_t>()>INT32_MAX))throw std::runtime_error("Intro Scenario int32 required");return v.get<int32_t>();};
    State v{j.at("active").get<bool>(),j.at("completed").get<bool>(),j.at("retiring").get<bool>(),number(j.at("phase")),number(j.at("starting_points"))};if(!valid(v,e))return false;out=v;return true;
}catch(const std::exception& x){e=std::string("Intro Scenario decode: ")+x.what();return false;}
}

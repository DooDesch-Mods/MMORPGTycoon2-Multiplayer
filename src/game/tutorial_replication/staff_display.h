#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::tutorial_replication::staff_display {
struct Developer {int32_t id=0,job=0;bool on_map=false;bool operator==(const Developer&)const=default;};
struct State {int32_t active_tech=0,active_developers=0;std::vector<Developer> developers;bool operator==(const State&)const=default;};
inline bool valid(const State& s,std::string& e){
    if(s.developers.size()>50){e="Staff display Developer count exceeds50";return false;}
    std::set<int32_t> ids;int32_t tech=0,outdoor=0,previous=-1;
    for(const auto& d:s.developers){
        if(d.id<0||d.id>49||d.id<=previous||d.job<0||d.job>7||!ids.insert(d.id).second){e="Staff display IDs/order/job outside native bounds";return false;}
        previous=d.id;if(d.on_map)++outdoor;else if(d.job>=5&&d.job<=7)++tech;
    }
    if(s.active_tech!=tech||s.active_developers!=tech+outdoor){e="Staff display counts differ from actual Developer states";return false;}
    e.clear();return true;
}
inline nlohmann::json encode(const State& s){nlohmann::json rows=nlohmann::json::array();for(const auto& d:s.developers)rows.push_back({d.id,d.job,d.on_map});return {{"active_tech",s.active_tech},{"active_developers",s.active_developers},{"developers",rows}};}
inline bool decode(const nlohmann::json& j,State& out,std::string& e){out={};try{
    if(!j.is_object()||j.size()!=3||!j.contains("active_tech")||!j.contains("active_developers")||!j.contains("developers"))throw std::runtime_error("Staff display exact fields required");
    const auto integer=[](const nlohmann::json& v){if(!v.is_number_integer()||(v.is_number_unsigned()?v.get<uint64_t>()>50:v.get<int64_t>()<0||v.get<int64_t>()>50))throw std::runtime_error("Staff display integer outside0..50");return v.get<int32_t>();};
    State s;s.active_tech=integer(j.at("active_tech"));s.active_developers=integer(j.at("active_developers"));
    if(!j.at("developers").is_array()||j.at("developers").size()>50)throw std::runtime_error("Staff display bounded Developer array required");
    for(const auto& row:j.at("developers")){if(!row.is_array()||row.size()!=3||!row[2].is_boolean())throw std::runtime_error("Staff display exact ID/job/on-map row required");s.developers.push_back({integer(row[0]),integer(row[1]),row[2].get<bool>()});}
    if(!valid(s,e))return false;out=std::move(s);return true;
}catch(const std::exception& x){e=std::string("Staff display decode: ")+x.what();return false;}}
inline const char* activity_key(const Developer& d){switch(d.job){case 0:return d.on_map?"{dev_state_idleonmap}":"{dev_state_idle}";case 5:return "{dev_state_going_home}";case 6:return "{dev_state_tech_develop}";case 7:return "{dev_state_tech_debug}";default:return nullptr;}}
inline const char* project_key(bool implementation_complete,int32_t active_tech){return implementation_complete?"-- {dev_tech_state_ready} --":active_tech?"-- {dev_tech_state_implementing} --":"-- {dev_tech_state_awaitingstaff} --";}
// Receipt adoption is independent of visibility. The original UI still runs
// once, and only a matching, laid-out native display may consume the receipt.
template<class Calls>bool render(const State& target,Calls& calls,bool& attempted,std::string& e){
    attempted=false;if(!valid(target,e)||!calls.pin(target,e))return false;
    for(const auto& d:target.developers)if(!activity_key(d)){e="Staff display has no admitted formatter for Developer job="+std::to_string(d.job);return false;}
    if(!calls.prepare(target,e))return false;
    attempted=true;
    if(!calls.summary(target.active_developers,e)||!calls.project(target.active_tech,e))return false;
    for(const auto& d:target.developers)if(!calls.developer(d,e))return false;
    return calls.readback(target,e);
}
}

#pragma once
#include "scenery_stroke_transition.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::scenery_stroke_state {
namespace a=action_session;
using Objects=std::map<std::string,a::DuplicateObject>;
inline nlohmann::json row(const a::DuplicateObject& value){
    return nlohmann::json::array({value.key,value.parent,value.record_sha,uint8_t(value.family),value.region,value.pose,value.members});
}
template<class Hash>bool digest(const Objects& objects,Hash&& hash,std::string& out,std::string& e){
    auto rows=nlohmann::json::array();for(const auto& [key,value]:objects){
        if(value.key!=key){e="Scenery complete canonical object map key differs";return false;}rows.push_back(row(value));}
    return hash(rows.dump(),out,e);
}
template<class Hash>bool difference(const Objects& before,const Objects& after,const std::vector<std::string>& birth_order,
                                  Hash&& hash,a::SceneryStrokeEffect& out,std::string& e,bool history=false){
    out={};auto fail=[&](const char* cause){out={};e=cause;return false;};
    std::set<std::string> created,births;auto untouched=nlohmann::json::array();
    for(const auto& [key,prior]:before){
        if(prior.key!=key)return fail("Scenery before canonical object map key differs");const auto found=after.find(key);
        if(found==after.end()){
            if(prior.family!=a::DuplicateFamily::Scenery)return fail("Scenery stroke removed a non-Scenery owner");out.removed.push_back(prior);continue;
        }
        if(found->second.key!=key)return fail("Scenery after canonical object map key differs");
        if(prior!=found->second)out.changed.push_back({prior,found->second});else untouched.push_back(row(prior));
    }
    for(const auto& [key,current]:after)if(!before.contains(key)){
        if(current.key!=key||current.family!=a::DuplicateFamily::Scenery||(!history&&!key.starts_with("action:")))return fail("Scenery stroke created an unexpected owner or alias");
        created.insert(key);
    }
    // Some native births can be removed again by a later stamp. Keep the
    // surviving output in actual birth order while requiring every birth once.
    for(const auto& key:birth_order){
        if(!births.insert(key).second||before.contains(key)||(!history&&!key.starts_with("action:")))return fail("Scenery original birth identity repeats or aliases a source");
        if(created.erase(key))out.created.push_back(after.at(key));
    }
    if(!created.empty())return fail("Scenery complete result has a creation absent from original birth order");
    return hash(untouched.dump(),out.untouched_sha,e);
}
}

#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>

namespace mt2mp::game::tutorial_startingpoint_fixture {
inline bool controller_admission(const nlohmann::json& state,std::string& error) {
    const auto reject=[&](const std::string& cause){error=cause;return false;};
    if(!state.contains("initialized")||!state.at("initialized").is_boolean())
        return reject("startingpoint fixture Controller initialization receipt absent");
    if(state.contains("error"))
        return reject("startingpoint fixture Controller read failed: "+state.at("error").dump());
    if(!state.at("initialized").get<bool>()){error.clear();return true;}
    for(const char* field:{"started","locked","pending","lobby_retiring"})
        if(!state.contains(field)||!state.at(field).is_boolean())
            return reject(std::string("startingpoint fixture Controller receipt absent: ")+field);
    if(!state.at("started").get<bool>()||state.at("locked").get<bool>()||
       state.at("pending").get<bool>()||state.at("lobby_retiring").get<bool>())
        return reject("startingpoint fixture requires idle unlocked started Controller");
    if(!state.contains("cause")||!state.at("cause").is_string())
        return reject("startingpoint fixture Controller cause receipt absent");
    if(!state.at("cause").get<std::string>().empty())
        return reject("startingpoint fixture Controller failed: "+state.at("cause").get<std::string>());
    if(!state.contains("live_state")||!state.at("live_state").is_object())
        return reject("startingpoint fixture live authority receipt absent");
    const auto& live=state.at("live_state");
    for(const char* field:{"active","locked","unknown"})
        if(!live.contains(field)||!live.at(field).is_boolean())
            return reject(std::string("startingpoint fixture live authority receipt absent: ")+field);
    if(!live.contains("cause")||!live.at("cause").is_string())
        return reject("startingpoint fixture live authority cause receipt absent");
    if(!live.at("cause").get<std::string>().empty())
        return reject("startingpoint fixture live authority failed: "+live.at("cause").get<std::string>());
    if(!live.at("active").get<bool>()||live.at("locked").get<bool>()||live.at("unknown").get<bool>())
        return reject("startingpoint fixture requires active known live authority");
    error.clear();return true;
}
}

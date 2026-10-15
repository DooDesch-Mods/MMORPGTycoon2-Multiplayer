#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>
#include <exception>
namespace mt2mp::game::tutorial_startingpoint_fixture::activity_probe {
// Native constructors own every string/vector allocation. This orchestrator
// only controls their matched native destruction and reports each boundary.
template<class Calls>bool format(Calls& calls,nlohmann::json& out,std::string& e){
    out=nlohmann::json::object();bool name=false,arg=false,formatted=false;
    const auto cleanup=[&]{
        std::string why;
        if(formatted&&!calls.release_format(why)){e+=(e.empty()?"":"; ")+why;}formatted=false;
        why.clear();if(arg&&!calls.release_arg(why)){e+=(e.empty()?"":"; ")+why;}arg=false;
        why.clear();if(name&&!calls.release_name(why)){e+=(e.empty()?"":"; ")+why;}name=false;
    };
    try{
    if(!calls.name(e))return false;name=true;
    nlohmann::json project;if(!calls.read_name(project,e)){cleanup();return false;}
    if(!calls.arg(e)){cleanup();return false;}arg=true;
    for(const char* key:{"{dev_state_tech_develop}","{dev_state_tech_debug}"}){
        if(!calls.format(key,e)){cleanup();return false;}formatted=true;
        nlohmann::json actual;if(!calls.read_format(actual,e)){cleanup();return false;}
        const nlohmann::json expected={{"key",key},{"args",nlohmann::json::array({{{"name","techname"},{"type",0},{"value",project}}})}};
        if(actual!=expected){e="Staff native formatter result differs from original project argument";cleanup();return false;}
        out[key]=actual;
        if(!calls.release_format(e)){formatted=false;cleanup();return false;}formatted=false;
    }
    cleanup();return e.empty();
    }catch(const std::exception& x){e=std::string("Staff native format probe exception: ")+x.what();cleanup();return false;}
}
}

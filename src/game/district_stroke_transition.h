#pragma once
#include "district_stroke_proof.h"

namespace mt2mp::game::district_stroke_transition {
using Values=std::map<std::string,nlohmann::json>;
inline bool apply(const nlohmann::json& graph,Values& values,std::string& error){
    auto fail=[&](const char* cause){error=cause;return false;};
    for(const auto& node:graph){
        const auto rva=node.at(0).get<uintptr_t>();const auto& payload=node.at(1);
        if(rva==0x1786fb0){if(payload.at(0)!=0)return fail("District inverse changes the cost-free Ledger");continue;}
        const auto key=(rva==0x16de860?payload:payload.at(0)).get<std::string>();
        const auto found=values.find(key);
        if(rva==0x17132f0){if(found!=values.end())return fail("District inverse Make names an already live canonical owner");values.emplace(key,payload);}
        else if(rva==0x17132b0){if(found==values.end())return fail("District inverse Edit names an absent canonical owner");found->second=payload;}
        else if(rva==0x16de860){if(found==values.end())return fail("District inverse Destroy names an absent canonical owner");values.erase(found);}
        else return fail("District inverse transition contains an unsupported original operation");
    }
    return true;
}
// Replay the original dependency-sorted graph in its persisted field domain.
// Full current District values are checked separately before publication.
inline bool restores(const nlohmann::json& graph,const Values& after,const Values& before,std::string& error){
    auto projected=after;if(!apply(graph,projected,error))return false;
    if(projected!=before){error="District original inverse does not restore every persisted canonical owner/value";return false;}return true;
}
}

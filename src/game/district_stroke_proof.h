#pragma once
#include "district_stroke_graph.h"
#include "district_stroke_state.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::district_stroke_proof {
using json=nlohmann::json;
inline json shape(const action_session::TerrainShape& value){
    json out=json::array();for(const auto& path:value){json row=json::array();for(const auto& point:path){row.push_back(point.x);row.push_back(point.z);}out.push_back(std::move(row));}return out;
}
inline json state(const action_session::DistrictState& value){
    return json::array({value.region,value.geo,value.constructed_day,value.saved_version,value.name,value.description,value.pose,value.values,
        value.flags,value.manual_title_inside,shape(value.shape)});
}
inline json values(const district_stroke_state::Values& rows){
    json out=json::array();for(const auto& [key,row]:rows)out.push_back(json::array({key,state(row)}));return out;
}
inline json stored(const district_edit_history::Stored& value,const std::string& key,int32_t region){
    return json::array({key,region,value.geo,shape(value.shape),value.name,value.description,value.color,value.water_color,value.title_color,
        value.height_spread,value.title_size,value.center,value.grass,value.options,value.water_obstruction});
}
template<class Find>
bool graph(const district_stroke_graph::Graph& value,const std::string& operation,uintptr_t root,
           const std::map<uintptr_t,int32_t>& regions,const std::map<uint64_t,std::string>& known,Find&& find,
           json& out,std::map<uint64_t,std::string>& all_keys,std::string& error){
    auto fail=[&](const char* cause){error=cause;return false;};
    std::map<uint64_t,std::set<uintptr_t>> kinds;
    for(const auto& node:value.nodes)if(node.uid)kinds[node.uid].insert(node.rva);
    auto keys=known;size_t ordinal=0;
    for(const auto& node:value.nodes){
        if(node.stored.uid&&(node.stored.root!=root||!regions.contains(node.stored.region)))return fail("District inverse stored root/Region is not current");
        if(!node.uid||keys.contains(node.uid))continue;
        uintptr_t current=0;if(!find(node.uid,current,error))return false;
        if(current||!kinds.at(node.uid).contains(0x16de860)||!kinds.at(node.uid).contains(0x17132f0))
            return fail("District inverse unbound full UID lacks an original transient Make/Destroy pair and actual absence");
        keys.emplace(node.uid,"district-transient:"+operation+":"+std::to_string(ordinal++));
    }
    json result=json::array();
    for(const auto index:value.order){
        if(index>=value.nodes.size())return fail("District inverse execution index exceeds complete nodes");
        const auto& node=value.nodes[index];json sets=json::array();
        for(const auto& dependencies:node.dependencies){json members=json::array();
            for(auto uid:dependencies){const auto found=keys.find(uid);if(found==keys.end())return fail("District inverse dependency full UID has no complete canonical owner");members.push_back(found->second);}
            std::sort(members.begin(),members.end());sets.push_back(std::move(members));}
        json payload;
        if(node.stored.uid)payload=stored(node.stored,keys.at(node.uid),regions.at(node.stored.region));
        else if(node.uid)payload=keys.at(node.uid);
        else payload=json::array({node.cash,node.position});
        result.push_back(json::array({node.rva,std::move(payload),std::move(sets)}));
    }
    if(result.size()!=value.nodes.size())return fail("District inverse execution order omits an original node");
    out=std::move(result);all_keys=std::move(keys);return true;
}
}

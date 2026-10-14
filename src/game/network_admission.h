#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>
namespace mt2mp::game::network_admission {
template<class Graph>const typename decltype(Graph::nodes)::value_type* select(const Graph& graph,int32_t region,int32_t index,std::string& e){
    if(region<0||index<0){e="network snapshot selector requires nonnegative region and node index";return nullptr;}
    int32_t current=0;for(const auto& node:graph.nodes)if(node.region_id==region){if(current++==index)return &node;}
    e="network snapshot region-local node index is absent";return nullptr;
}
template<class Node,class Pin>bool same_identity(const Node& node,const Pin& pin){
    return node.object==pin.object&&node.uid==pin.local_uid&&node.region_id==pin.region_id&&node.manager==pin.manager&&
        node.region==pin.region&&node.definition==pin.definition&&node.type==pin.type&&node.key==pin.key;
}
template<class Graph>nlohmann::json topology(const Graph& graph){
    auto rows=graph.rows;
    for(auto& region:rows){
        for(auto& node:region[2]){node[5]=nullptr;node[6]=nullptr;}
        for(auto& pipe:region[3]){pipe[11]=nullptr;pipe[12]=nullptr;}
    }
    // Local object identities stay private. Recreated objects cannot inherit an observer pin.
    auto objects=nlohmann::json::array();
    for(const auto& node:graph.nodes)objects.push_back(nlohmann::json::array({node.object,node.uid,node.manager,node.region,node.definition}));
    if constexpr(requires{graph.pipe_values;})for(const auto& pipe:graph.pipe_values)
        objects.push_back(nlohmann::json::array({pipe.object,pipe.uid,pipe.manager,pipe.region,pipe.definition}));
    return nlohmann::json::array({rows,objects});
}
}

#pragma once
#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::path_history_graph {
using Json=nlohmann::json;
struct Snapshot {
    std::vector<uint64_t> roads,nodes;
    Json rows;
};

// Native restore can append a known UID at another manager index. Compare the
// complete road data and reciprocal edges in the retained source UID order.
inline bool rebase(const Snapshot& current,const Snapshot& order,Json& out,std::string& error){
    auto fail=[&](const std::string& cause){error="Path history topology: "+cause;return false;};
    if(current.roads.size()!=order.roads.size()||current.nodes.size()!=order.nodes.size())
        return fail("current and retained member counts differ");
    if(current.roads.size()>1000000||current.nodes.size()>1000000)return fail("member limit exceeds1000000");
    std::map<uint64_t,size_t> roads,nodes;
    std::set<uint64_t> all;
    for(size_t i=0;i<order.roads.size();++i)
        if(!order.roads[i]||!all.insert(order.roads[i]).second||!roads.emplace(order.roads[i],i).second)
            return fail("retained road UID is zero or duplicated");
    for(size_t i=0;i<order.nodes.size();++i)
        if(!order.nodes[i]||!all.insert(order.nodes[i]).second||!nodes.emplace(order.nodes[i],i).second)
            return fail("retained node UID is zero or duplicated");
    std::vector<size_t> road_index,node_index;
    all.clear();
    for(uint64_t uid:current.roads){
        const auto found=roads.find(uid);
        if(found==roads.end()||!all.insert(uid).second)return fail("current road UID is foreign or duplicated");
        road_index.push_back(found->second);
    }
    for(uint64_t uid:current.nodes){
        const auto found=nodes.find(uid);
        if(found==nodes.end()||!all.insert(uid).second)return fail("current node UID is foreign or duplicated");
        node_index.push_back(found->second);
    }
    try{
        if(!current.rows.is_object()||current.rows.size()!=2||!current.rows.contains("paths")||!current.rows.contains("nodes"))
            return fail("complete paths/nodes object required");
        const auto& paths=current.rows.at("paths");const auto& points=current.rows.at("nodes");
        if(!paths.is_array()||!points.is_array()||paths.size()!=current.roads.size()||points.size()!=current.nodes.size())
            return fail("row and UID counts differ");
        Json path_rows=Json::array(),node_rows=Json::array();
        for(size_t i=0;i<paths.size();++i)path_rows.push_back(nullptr);
        for(size_t i=0;i<points.size();++i)node_rows.push_back(nullptr);
        std::vector<std::set<size_t>> expected(points.size());
        auto index=[&](const Json& value,size_t limit,size_t& result){
            if(!value.is_number_unsigned()&&!value.is_number_integer())return false;
            if(value.is_number_integer()&&value.get<int64_t>()<0)return false;
            const auto number=value.get<uint64_t>();if(number>=limit)return false;
            result=size_t(number);return true;
        };
        for(size_t i=0;i<paths.size();++i){
            auto row=paths.at(i);size_t a=0,b=0;
            if(!row.is_array()||row.size()!=6||!index(row.at(0),points.size(),a)||!index(row.at(1),points.size(),b)||a==b)
                return fail("road endpoints or complete native row differ");
            for(size_t j=2;j<6;++j)if(!row.at(j).is_array())return fail("road waypoint/distance array missing");
            expected[a].insert(road_index[i]);expected[b].insert(road_index[i]);
            row[0]=node_index[a];row[1]=node_index[b];path_rows[road_index[i]]=std::move(row);
        }
        for(size_t i=0;i<points.size();++i){
            const auto& row=points.at(i);
            if(!row.is_array()||row.size()!=2||!row.at(0).is_array()||row.at(0).size()!=3||!row.at(1).is_array())
                return fail("complete node position/adjacency row required");
            std::set<size_t> edges;
            for(const auto& edge:row.at(1)){size_t value=0;
                if(!index(edge,paths.size(),value)||!edges.insert(road_index[value]).second)
                    return fail("node adjacency is foreign or duplicated");
            }
            if(edges!=expected[i])return fail("node adjacency is not reciprocal with road endpoints");
            node_rows[node_index[i]]=Json::array({row.at(0),Json(edges)});
        }
        out={{"paths",std::move(path_rows)},{"nodes",std::move(node_rows)}};
        error.clear();return true;
    }catch(const Json::exception& cause){return fail(std::string("invalid native row: ")+cause.what());}
}
}

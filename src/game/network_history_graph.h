#pragma once
#include "connected_network_reader.h"
#include "network_object_identity.h"
namespace mt2mp::game::network_history_graph {
using json=nlohmann::json;
template<class Key>bool canonical(const connected_network_native::Graph& graph,Key&& key,json& out,std::string& error){
    out=json::array();try{
        std::map<std::string,std::string> nodes,pipes;std::set<std::string> unique;
        for(const auto& node:graph.nodes){std::string id;if(!key(node.object,node.uid,id,error))return false;
            if(!network_object_identity::matches(id,network_object_identity::Kind::Node)||!unique.insert(id).second)throw std::runtime_error("Network history Node key aliases or has another family");nodes.emplace(graph.node_keys.at(node.object).dump(),id);}
        for(const auto& pipe:graph.pipe_values){std::string id;if(!key(pipe.object,pipe.uid,id,error))return false;
            if(!network_object_identity::matches(id,network_object_identity::Kind::Pipe)||!unique.insert(id).second)throw std::runtime_error("Network history Pipe key aliases or has another family");pipes.emplace(graph.pipe_keys.at(pipe.object).dump(),id);}
        for(const auto& region:graph.rows){if(!region.is_array()||region.size()!=5)throw std::runtime_error("Network history complete Region row invalid");
            json row=json::array({region[0],region[1],json::array(),json::array(),json::array()});
            for(size_t i=0;i<region[2].size();++i){auto node=region[2][i];const auto index=json::array({region[0],i}).dump();
                for(auto& pipe:node[7])pipe=pipes.at(pipe.dump());std::sort(node[7].begin(),node[7].end());row[2].push_back(json::array({nodes.at(index),node}));}
            for(size_t i=0;i<region[3].size();++i){auto pipe=region[3][i];const auto index=json::array({region[0],i}).dump();
                pipe[5]=nodes.at(pipe[5].dump());pipe[6]=nodes.at(pipe[6].dump());row[3].push_back(json::array({pipes.at(index),pipe}));}
            for(const auto& pipe:region[4])row[4].push_back(pipes.at(pipe.dump()));
            for(size_t part:{2,3,4})std::sort(row[part].begin(),row[part].end());out.push_back(std::move(row));
        }
        error.clear();return true;
    }catch(const std::exception& exception){error=std::string("Network history canonical complete graph: ")+exception.what();return false;}
}
// Simulation changes flow values between user edits. The original history graph
// owns membership and geometry; each actual BEFORE/AFTER digest still includes
// every current field and is compared between Host and Client before ACK.
inline json shape(json value){for(auto& region:value){
    for(auto& pair:region[2]){auto& node=pair[1];node[5]=nullptr;node[6]=nullptr;node[8]=nullptr;}
    for(auto& pair:region[3]){auto& pipe=pair[1];pipe[11]=nullptr;pipe[12]=nullptr;pipe[13]=nullptr;}
    }return value;
}
}

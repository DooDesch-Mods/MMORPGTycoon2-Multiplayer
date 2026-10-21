#pragma once
#include "mixed_path_graph.h"

namespace mt2mp::game::mixed_path_remove {
using Snapshot=mixed_path_graph::Snapshot;
using Json=mixed_path_graph::Json;
inline bool fail(std::string& error,const char* cause){error=cause;return false;}
// Original ShouldDestroyNow retires unused endpoint types 0 and 1. Other node
// types retain their identity even with no edges. Bucket order is not identity.
inline bool delta(const Snapshot& before,const Snapshot& after,const std::string& target,
                  std::set<std::string>& retired,std::string& error){
    std::map<std::string,Json> paths,nodes;
    std::map<std::string,uint64_t> path_uids,node_uids;
    auto index=[&](const Snapshot& state,std::map<std::string,Json>& ps,std::map<std::string,Json>& ns,
                   std::map<std::string,uint64_t>& pu,std::map<std::string,uint64_t>& nu){
        if(!state.rows.is_object()||!state.rows.contains("paths")||!state.rows.contains("nodes")||
           !state.rows.contains("remote")||!state.rows.at("paths").is_array()||
           !state.rows.at("nodes").is_array()||!state.rows.at("remote").is_array()||
           state.rows.at("paths").size()!=state.paths.size()||state.rows.at("nodes").size()!=state.nodes.size())return false;
        for(size_t i=0;i<state.paths.size();++i){const auto& id=state.paths[i];const auto& row=state.rows.at("paths")[i];
            if(id.key.empty()||!id.local_uid||!row.is_array()||row.size()!=9||row[0]!=id.key||
               !ps.emplace(id.key,row).second||!pu.emplace(id.key,id.local_uid).second)return false;}
        for(size_t i=0;i<state.nodes.size();++i){const auto& id=state.nodes[i];const auto& row=state.rows.at("nodes")[i];
            if(id.key.empty()||!id.local_uid||!row.is_array()||row.size()!=5||row[0]!=id.key||
               !row[4].is_array()||!ns.emplace(id.key,row).second||!nu.emplace(id.key,id.local_uid).second)return false;}
        return true;
    };
    if(!index(before,paths,nodes,path_uids,node_uids)||!paths.contains(target))
        return fail(error,"mixed Path removal before witness or target absent");
    const auto target_row=paths.at(target);
    if(!target_row[4].is_array()||target_row[4].size()!=2)
        return fail(error,"mixed Path removal target endpoints invalid");
    std::set<std::string> removed{target},endpoints;
    for(const auto& endpoint:target_row[4]){
        if(endpoint.is_null())continue;
        if(!endpoint.is_string()||!nodes.contains(endpoint.get<std::string>()))
            return fail(error,"mixed Path removal endpoint is not an owned Intersection");
        endpoints.insert(endpoint.get<std::string>());
    }
    for(const auto& key:endpoints){auto& edges=nodes.at(key)[4];Json remaining=Json::array();size_t matches=0;
        for(const auto& edge:edges){if(edge==target)++matches;else remaining.push_back(edge);}
        if(matches!=1)return fail(error,"mixed Path removal reciprocal target edge count differs");
         if(!nodes.at(key)[2].is_number_integer())return fail(error,"mixed Path removal endpoint type invalid");
         const auto type=nodes.at(key)[2].get<int32_t>();
         if(type<0||type>=6)return fail(error,"mixed Path removal endpoint type outside original domain");
         if(remaining.empty()&&type<=1){removed.insert(key);nodes.erase(key);node_uids.erase(key);}
        else edges=std::move(remaining);
    }
    paths.erase(target);path_uids.erase(target);
    Json remote=Json::array();
    for(const auto& row:before.rows.at("remote")){
        if(!row.is_array()||row.size()!=3)return fail(error,"mixed Path removal remote row invalid");
        if(row[2]!=target)remote.push_back(row);
    }
    std::map<std::string,Json> actual_paths,actual_nodes;
    std::map<std::string,uint64_t> actual_path_uids,actual_node_uids;
    if(!index(after,actual_paths,actual_nodes,actual_path_uids,actual_node_uids))
        return fail(error,"mixed Path removal after witness invalid");
    auto actual_remote=after.rows.at("remote");std::sort(remote.begin(),remote.end());std::sort(actual_remote.begin(),actual_remote.end());
    if(paths!=actual_paths||nodes!=actual_nodes||remote!=actual_remote||
       path_uids!=actual_path_uids||node_uids!=actual_node_uids)
        return fail(error,"mixed Path removal changed unrelated topology, values or full identities");
    retired=std::move(removed);error.clear();return true;
}
}

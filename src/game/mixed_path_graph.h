#pragma once
#include "mixed_path_reader.h"
#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <bit>
#include <map>
#include <set>

namespace mt2mp::game::mixed_path_graph {
using Json=nlohmann::json;
using Census=mixed_path_native::Census;
using Ref=mixed_path_native::Ref;

struct Identity {
    std::string key;
    uint64_t local_uid=0;
    Ref local_ref;
};
struct Snapshot {
    std::vector<Identity> paths,nodes;
    Json rows;
};
inline bool reject(std::string& error,const std::string& why){error="mixed path graph: "+why;return false;}
inline Json bits(float value){return std::bit_cast<uint32_t>(value);}
inline Json point(const action_session::PathPoint& value){return Json::array({bits(value[0]),bits(value[1]),bits(value[2])});}
inline Json points(const std::vector<action_session::PathPoint>& values){Json out=Json::array();for(const auto& value:values)out.push_back(point(value));return out;}
inline Json distances(const std::vector<float>& values){Json out=Json::array();for(float value:values)out.push_back(bits(value));return out;}

// Keys are supplied by the canonical object table. A birth uses
// action:<uuid>:path:<ordinal> or action:<uuid>:node:<ordinal>; native UIDs
// stay in this local witness and never enter the portable rows.
inline bool build(const Census& census,const std::map<uint64_t,std::string>& keys,Snapshot& out,std::string& error){
    Snapshot value;
    std::map<uintptr_t,std::string> paths,nodes;
    std::map<uintptr_t,int32_t> regions;std::set<int32_t> region_ids;
    for(const auto& region:census.regions)
        if(region.id<0||!region.object||!regions.emplace(region.object,region.id).second||
           !region_ids.insert(region.id).second)
            return reject(error,"Region identity absent or duplicated");
    std::set<std::string> unique;
    std::set<uint64_t> uids;
    auto identity=[&](const mixed_path_native::Member& member,std::vector<Identity>& dest,std::map<uintptr_t,std::string>& objects){
        const auto found=keys.find(member.uid);
        if(!member.object||!member.uid||found==keys.end()||found->second.empty()||
           !uids.insert(member.uid).second||!unique.insert(found->second).second||
           !objects.emplace(member.object,found->second).second)return reject(error,"missing or duplicate canonical object identity");
        dest.push_back({found->second,member.uid,member.ref});return true;
    };
    for(const auto& row:census.paths)if(!identity(row.member,value.paths,paths))return false;
    for(const auto& row:census.nodes)if(!identity(row.member,value.nodes,nodes))return false;
    Json path_rows=Json::array(),node_rows=Json::array(),remote_rows=Json::array();
    std::map<uintptr_t,std::set<std::string>> expected_edges;
    std::map<uintptr_t,const mixed_path_native::Node*> node_by_pointer;
    for(const auto& node:census.nodes)node_by_pointer.emplace(node.member.object,&node);
    std::set<std::pair<int32_t,std::string>> expected_remote,actual_remote;
    for(const auto& row:census.paths){
        if(!row.member.object||!paths.contains(row.member.object)||!regions.contains(row.owner)||
           regions.at(row.owner)!=row.member.ref.region||row.member.ref.type<0||row.member.ref.type>=11)
            return reject(error,"owned Path identity/type/Region absent");
        if(row.second){if(!regions.contains(row.second)||row.second==row.owner)
                return reject(error,"second Region is foreign or equals owner");
            expected_remote.emplace(regions.at(row.second),paths.at(row.member.object));}
        Json endpoints=Json::array();
        for(uintptr_t endpoint:{row.start,row.end}){
            if(!endpoint)endpoints.push_back(nullptr);
            else {const auto found=nodes.find(endpoint);if(found==nodes.end())return reject(error,"Path endpoint lacks canonical Intersection");
                const auto native=node_by_pointer.find(endpoint);
                if(native==node_by_pointer.end()||
                   (native->second->owner!=row.owner&&native->second->owner!=row.second))
                    return reject(error,"Path endpoint belongs to another Region");
                endpoints.push_back(found->second);expected_edges[endpoint].insert(paths.at(row.member.object));}
        }
        path_rows.push_back(Json::array({paths.at(row.member.object),row.member.ref.region,row.member.ref.type,
            nullptr,endpoints,points(row.points),distances(row.distances),
            points(row.walks),distances(row.walk_distances)}));
        if(row.second){
            const auto region=std::find_if(census.regions.begin(),census.regions.end(),[&](const auto& v){return v.object==row.second;});
            if(region==census.regions.end())return reject(error,"second Region absent from complete census");
            path_rows.back()[3]=region->id;
        }
    }
    for(const auto& row:census.nodes){
        if(!nodes.contains(row.member.object)||!regions.contains(row.owner)||
           regions.at(row.owner)!=row.member.ref.region||row.member.ref.type<0||row.member.ref.type>=6)
            return reject(error,"owned Intersection identity/type/Region absent");
        std::set<std::string> edges;
        for(uintptr_t edge:row.edges){const auto found=paths.find(edge);
            if(found==paths.end()||!edges.insert(found->second).second)return reject(error,"Intersection edge lacks unique canonical Path");}
        if(edges!=expected_edges[row.member.object])return reject(error,"canonical edge membership differs from Path endpoints");
        node_rows.push_back(Json::array({nodes.at(row.member.object),row.member.ref.region,row.member.ref.type,
            point(row.position),Json(edges)}));
    }
    std::map<uintptr_t,int32_t> path_types;
    for(const auto& row:census.paths)path_types.emplace(row.member.object,row.member.ref.type);
    for(const auto& row:census.remote){const auto found=paths.find(row.path);
        if(found==paths.end()||!region_ids.contains(row.region)||path_types.at(row.path)!=row.type||
           !actual_remote.emplace(row.region,found->second).second)
            return reject(error,"remote membership lacks unique canonical Path");
        remote_rows.push_back(Json::array({row.region,row.type,found->second}));}
    if(actual_remote!=expected_remote)return reject(error,"second Region remote membership differs");
    std::sort(remote_rows.begin(),remote_rows.end());
    value.rows={{"paths",std::move(path_rows)},{"nodes",std::move(node_rows)},{"remote",std::move(remote_rows)}};
    out=std::move(value);error.clear();return true;
}

// Native Undo may reorder buckets and allocate different local UIDs. Rebase
// both complete witnesses to the source key order before comparing effects.
inline bool rebase(const Snapshot& current,const Snapshot& order,Json& out,std::string& error){
    auto index=[&](const std::vector<Identity>& values,std::map<std::string,size_t>& result){
        for(size_t i=0;i<values.size();++i)if(values[i].key.empty()||!values[i].local_uid||
            !result.emplace(values[i].key,i).second)return false;return true;};
    std::map<std::string,size_t> current_paths,current_nodes,order_paths,order_nodes;
    if(!index(current.paths,current_paths)||!index(current.nodes,current_nodes)||
       !index(order.paths,order_paths)||!index(order.nodes,order_nodes)||
       current_paths.size()!=order_paths.size()||current_nodes.size()!=order_nodes.size())
        return reject(error,"incomplete or duplicate local UID order");
    if(!current.rows.is_object()||current.rows.size()!=3||!current.rows.contains("paths")||
       !current.rows.contains("nodes")||!current.rows.contains("remote")||
       !current.rows.at("paths").is_array()||!current.rows.at("nodes").is_array()||
       !current.rows.at("remote").is_array()||current.rows.at("paths").size()!=current.paths.size()||
       current.rows.at("nodes").size()!=current.nodes.size())return reject(error,"complete mixed topology rows absent");
    Json path_rows=Json::array(),node_rows=Json::array();
    for(const auto& identity:order.paths){const auto found=current_paths.find(identity.key);
        if(found==current_paths.end())return reject(error,"Path canonical key missing after native history");
        const auto& row=current.rows.at("paths").at(found->second);
        const auto& local=current.paths.at(found->second);
        if(!row.is_array()||row.size()!=9||row.at(0)!=identity.key||row.at(1)!=identity.local_ref.region||
           row.at(2)!=identity.local_ref.type||local.local_ref.region!=identity.local_ref.region||
           local.local_ref.type!=identity.local_ref.type)return reject(error,"Path canonical row/type/Region differs");
        path_rows.push_back(row);
    }
    for(const auto& identity:order.nodes){const auto found=current_nodes.find(identity.key);
        if(found==current_nodes.end())return reject(error,"Intersection canonical key missing after native history");
        const auto& row=current.rows.at("nodes").at(found->second);
        const auto& local=current.nodes.at(found->second);
        if(!row.is_array()||row.size()!=5||row.at(0)!=identity.key||row.at(1)!=identity.local_ref.region||
           row.at(2)!=identity.local_ref.type||local.local_ref.region!=identity.local_ref.region||
           local.local_ref.type!=identity.local_ref.type)return reject(error,"Intersection canonical row/type/Region differs");
        node_rows.push_back(row);
    }
    Json remote=current.rows.at("remote");std::sort(remote.begin(),remote.end());
    out={{"paths",std::move(path_rows)},{"nodes",std::move(node_rows)},{"remote",std::move(remote)}};
    error.clear();return true;
}
}

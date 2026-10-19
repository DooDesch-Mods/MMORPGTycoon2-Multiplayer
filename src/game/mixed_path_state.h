#pragma once
#include "mixed_path_graph.h"
#include "mixed_path_records.h"
#include "mt2mp/core/hash.h"

namespace mt2mp::game::mixed_path_state {
using Json=nlohmann::json;
struct Snapshot {
    mixed_path_native::Census native;
    mixed_path_graph::Snapshot graph;
    mixed_path_records::Witness records;
};
inline bool reject(std::string& error,const std::string& cause){error="mixed Path complete state: "+cause;return false;}
template<class Memory>
bool read(Memory& memory,uintptr_t image,uintptr_t map,uintptr_t root,
          const std::map<uint64_t,std::string>& keys,Snapshot& out,std::string& error){
    Snapshot value;
    if(!mixed_path_native::read_world(memory,image,map,root,value.native,error)||
       !mixed_path_graph::build(value.native,keys,value.graph,error)||
       !mixed_path_records::read_all(memory,image,value.native,keys,value.records,error))return false;
    out=std::move(value);return true;
}
inline bool canonical(const Snapshot& current,const Snapshot& order,Json& out,std::string& error){
    Json graph;
    if(!mixed_path_graph::rebase(current.graph,order.graph,graph,error))return false;
    Json records=Json::array();
    auto append=[&](const mixed_path_graph::Identity& member){
        const auto found=current.records.objects.find(member.key);
        if(found==current.records.objects.end()||found->second.sha.size()!=64)return false;
        const auto& record=found->second;
        records.push_back(Json::array({member.key,record.sha,record.records,record.tokens,record.links}));
        return true;};
    for(const auto& member:order.graph.paths)if(!append(member))return reject(error,"Path original Record is absent");
    for(const auto& member:order.graph.nodes)if(!append(member))return reject(error,"Intersection original Record is absent");
    if(records.size()!=current.records.objects.size())return reject(error,"foreign original Record remains in complete census");
    out={{"graph",std::move(graph)},{"records",std::move(records)}};return true;
}
inline bool digest(const Snapshot& current,const Snapshot& order,std::string& out,std::string& error){
    Json value;if(!canonical(current,order,value,error))return false;
    const auto bytes=value.dump();out=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));
    return out.size()==64||reject(error,"complete original Record graph digest failed");
}
inline bool same(const Snapshot& current,const Snapshot& expected,std::string& error){
    Json actual,original;
    return canonical(current,expected,actual,error)&&canonical(expected,expected,original,error)&&
           (actual==original||reject(error,"complete mixed native topology/Record effects differ"));
}
}

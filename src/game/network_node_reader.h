#pragma once
#include "path_reader.h"
#include "typed_action/network_action.h"
#include "vendor/nlohmann/json.hpp"
#include <map>
#include <set>
namespace mt2mp::game::network_node_native {
using json=nlohmann::json;
namespace read {
using path_native::array;
template<class M,class T>bool at(M& m,uintptr_t p,size_t off,T& value,std::string& e){return path_native::field(m,p,off,value,e);}
template<class M>bool ap(M& m,uintptr_t p,uintptr_t image,uintptr_t expected,std::string& e){
    uintptr_t actual=0;if(!at(m,p,0,actual,e))return false;
    if(actual!=image+expected){e="network native AddressPoint differs required RVA="+std::to_string(expected);return false;}return true;
}
template<class M>bool text(M& memory,uintptr_t p,std::string& out,std::string& e) {
    uintptr_t data=0;uint64_t count=0;
    if(!at(memory,p,0,data,e)||!at(memory,p,8,count,e))return false;
    if(!data||!count||count>128){e="network GNU string length requires 1..128";return false;}
    out.resize(size_t(count));if(!memory.copy(data,out.data(),out.size(),e))return false;
    if(out.find('\0')!=std::string::npos){e="network GNU string contains NUL";return false;}return true;
}
}
struct Node {
    uintptr_t object=0,manager=0,region=0,definition=0;
    uint64_t uid=0;
    int32_t region_id=-1,cost=-1;
    std::string type,key;
    std::array<float,10> transform{};
    std::array<float,5> definition_values{};
    std::array<float,4> state{};
    std::array<uint8_t,3> flags{};
    json row;
};
struct Graph {std::vector<Node> nodes;json rows=json::array();uint64_t pipes=0,connections=0;};
template<class Memory>bool graph(Memory& memory,uintptr_t image,uintptr_t root,Graph& out,std::string& e) {
    namespace p=read;
    out={};uintptr_t map=0;std::vector<uintptr_t> regions;
    if(!p::at(memory,image,0x13df8c0,map,e)||!p::ap(memory,map,image,0x1742860,e)||map+0x1c0!=root||
       !p::array(memory,map,0x358,0x360,0x364,16384,regions,e))return false;
    std::map<int32_t,json> canonical;std::set<uintptr_t> managers,objects;std::set<uint64_t> uids;
    for(auto region:regions){
        int32_t id=-1;uintptr_t shard=0,manager=0,owner=0;
        if(!p::ap(memory,region,image,0x1787930,e)||!p::at(memory,region,0x834,id,e)||!p::at(memory,region,0x70,shard,e)||
           !p::at(memory,region,0x978,manager,e))return false;
        if(id<0||shard!=root||canonical.contains(id)){e="network region ID/shard membership differs";return false;}
        json nodes=json::array();
        if(manager){
            if(!managers.insert(manager).second||!p::ap(memory,manager,image,0x17141e0,e)||!p::at(memory,manager,0x168,owner,e))return false;
            if(owner!=region){e="network manager region owner differs";return false;}
            std::vector<uintptr_t> native_nodes,pipes,pending;
            if(!p::array(memory,manager,0x20,0x28,0x2c,65536,native_nodes,e)||!p::array(memory,manager,0x50,0x58,0x5c,65536,pipes,e)||
               !p::array(memory,manager,0x80,0x88,0x8c,65536,pending,e))return false;
            if(!pipes.empty()||!pending.empty()||native_nodes.size()+out.nodes.size()>1){e="first network node witness requires no pipes/pending pipes and at most one world node";return false;}
            for(auto object:native_nodes){
                Node n;n.object=object;n.manager=manager;n.region=region;n.region_id=id;uintptr_t actual_region=0,actual_shard=0;uint8_t preview=1;
                if(!objects.insert(object).second||!p::ap(memory,object,image,0x16fc700,e)||!p::at(memory,object,0x18,n.uid,e)||
                   !p::at(memory,object,0x178,actual_region,e)||!p::at(memory,object,0x170,actual_shard,e)||!p::at(memory,object,0x258,n.definition,e)||
                   !p::ap(memory,n.definition,image,0x1738010,e)||!p::text(memory,n.definition+0x18,n.type,e)||!p::text(memory,n.definition+0x38,n.key,e)||
                   !p::at(memory,n.definition,0x78,n.cost,e)||!p::at(memory,n.definition,0xc0,n.definition_values,e)||!p::at(memory,object,0x100,n.transform,e)||
                   !p::at(memory,object,0x250,n.state[0],e)||!p::at(memory,object,0x2a0,n.state[1],e)||!p::at(memory,object,0x2a4,n.state[2],e)||
                   !p::at(memory,object,0x2e4,n.state[3],e)||!p::at(memory,object,0x2e8,n.flags[0],e)||!p::at(memory,object,0x2ea,n.flags[1],e)||
                   !p::at(memory,object,0x2ec,n.flags[2],e)||!p::at(memory,object,0x2eb,preview,e))return false;
                if(!n.uid||!uids.insert(n.uid).second||actual_region!=region||actual_shard!=root||preview||n.cost<0){e="network node UID/owner/preview/cost differs";return false;}
                for(const auto& values:{std::vector<float>(n.transform.begin(),n.transform.end()),std::vector<float>(n.definition_values.begin(),n.definition_values.end()),std::vector<float>(n.state.begin(),n.state.end())})
                    for(float value:values)if(!std::isfinite(value)){e="network node scalar is not finite";return false;}
                for(auto flag:n.flags)if(flag>1){e="network node flag outside bool domain";return false;}
                std::vector<uintptr_t> node_pipes,connections;
                if(!p::array(memory,object,0x268,0x270,0x274,65536,node_pipes,e)||!p::array(memory,object,0x2b0,0x2b8,0x2bc,65536,connections,e))return false;
                if(!node_pipes.empty()||!connections.empty()){e="first network node original placement has unexpected attached pipe/connection";return false;}
                n.row=json::array({n.type,n.key,n.transform,n.definition_values,n.cost,n.state,n.flags,json::array(),json::array()});nodes.push_back(n.row);out.nodes.push_back(n);
            }
        }
        canonical.emplace(id,json::array({id,bool(manager),nodes,json::array(),json::array()}));
    }
    for(const auto& [id,row]:canonical){(void)id;out.rows.push_back(row);}return true;
}
inline bool delta(const Graph& before,const Graph& after,const action_session::NetworkIntent& request,uintptr_t created,std::string& e) {
    if(!before.nodes.empty()||after.nodes.size()!=1||before.rows.size()!=after.rows.size()){e="network original node count delta differs";return false;}
    const auto& n=after.nodes.front();
    if(n.object!=created||n.region_id!=request.region||n.type!=request.type||n.key!=request.key||n.cost!=request.quoted_cost||
       std::array<float,3>{n.transform[4],n.transform[5],n.transform[6]}!=request.position){e="network actual created node identity/definition/pose differs";return false;}
    auto expected=before.rows;bool found=false;
    for(auto& row:expected)if(row[0]==request.region){if(!row[1].get<bool>()){e="network target manager absent";return false;}row[2].push_back(n.row);found=true;}
    if(!found||expected!=after.rows){e="network original placement changed unrelated topology";return false;}
    return true;
}
}

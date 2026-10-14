#pragma once
#include "topology_fields.h"
#include "typed_action/network_pipe_action.h"
namespace mt2mp::game::connected_network_native {
namespace read=network_node_native::read;
namespace fields=network_topology_native;
using json=nlohmann::json;
using Node=network_node_native::Node;
using Pipe=fields::Pipe;
struct Graph {
    std::vector<Node> nodes;std::vector<Pipe> pipe_values;
    std::map<uintptr_t,std::vector<uintptr_t>> node_pipes,node_connections;
    std::map<uintptr_t,json> node_keys,pipe_keys;
    json rows=json::array();uint64_t pipes=0,connections=0;
};
template<class M>bool node(M& m,uintptr_t image,uintptr_t root,uintptr_t object,uintptr_t region,uintptr_t manager,int32_t id,Node& n,std::string& e){
    namespace r=read;n={};n.object=object;n.region=region;n.manager=manager;n.region_id=id;
    uintptr_t actual_region=0,shard=0;uint8_t preview=1;
    if(!r::ap(m,object,image,0x16fc700,e)||!r::at(m,object,0x18,n.uid,e)||!r::at(m,object,0x178,actual_region,e)||
       !r::at(m,object,0x170,shard,e)||!r::at(m,object,0x258,n.definition,e)||!r::ap(m,n.definition,image,0x1738010,e)||
       !r::text(m,n.definition+0x18,n.type,e)||!r::text(m,n.definition+0x38,n.key,e)||!r::at(m,n.definition,0x78,n.cost,e)||
       !r::at(m,n.definition,0xc0,n.definition_values,e)||!r::at(m,object,0x100,n.transform,e)||
       !r::at(m,object,0x250,n.state[0],e)||!r::at(m,object,0x2a0,n.state[1],e)||!r::at(m,object,0x2a4,n.state[2],e)||
       !r::at(m,object,0x2e4,n.state[3],e)||!r::at(m,object,0x2e8,n.flags[0],e)||!r::at(m,object,0x2ea,n.flags[1],e)||
       !r::at(m,object,0x2ec,n.flags[2],e)||!r::at(m,object,0x2eb,preview,e))return false;
    if(!n.uid||actual_region!=region||shard!=root||preview||n.cost<0)return fields::reject(e,"network node owner/UID/preview/cost differs");
    for(auto flag:n.flags)if(flag>1)return fields::reject(e,"network node flag outside bool domain");
    if(!fields::finite(n.transform,e)||!fields::finite(n.definition_values,e)||!fields::finite(n.state,e))return false;
    n.row=json::array({n.type,n.key,n.transform,n.definition_values,n.cost,n.state,n.flags,json::array(),json::array()});return true;
}
template<class M>bool graph(M& m,uintptr_t image,uintptr_t root,Graph& out,std::string& e){
    namespace r=read;out={};uintptr_t map=0;std::vector<uintptr_t> regions;
    if(!r::at(m,image,0x13df8c0,map,e)||!r::ap(m,map,image,0x1742860,e)||map+0x1c0!=root||!r::array(m,map,0x358,0x360,0x364,16384,regions,e))return false;
    std::map<int32_t,json> rows;std::map<uintptr_t,int32_t> region_ids;
    std::map<int32_t,std::vector<uintptr_t>> local_pipes;
    std::set<uintptr_t> managers,objects;std::set<uint64_t> uids;
    for(auto region:regions){
        int32_t id=-1;uintptr_t shard=0,manager=0,owner=0;
        if(!r::ap(m,region,image,0x1787930,e)||!r::at(m,region,0x834,id,e)||!r::at(m,region,0x70,shard,e)||!r::at(m,region,0x978,manager,e))return false;
        if(id<0||shard!=root||rows.contains(id))return fields::reject(e,"network region ID/shard membership differs");
        region_ids.emplace(region,id);rows.emplace(id,json::array({id,bool(manager),json::array(),json::array(),json::array()}));
        if(!manager)continue;
        if(!managers.insert(manager).second||!r::ap(m,manager,image,0x17141e0,e)||!r::at(m,manager,0x168,owner,e))return false;
        if(owner!=region)return fields::reject(e,"network manager region owner differs");
        std::vector<uintptr_t> nodes,pipes;
        if(!r::array(m,manager,0x20,0x28,0x2c,65536,nodes,e)||!r::array(m,manager,0x50,0x58,0x5c,65536,pipes,e)||
           !r::array(m,manager,0x80,0x88,0x8c,65536,local_pipes[id],e))return false;
        for(size_t index=0;index<nodes.size();++index){
            Node n;if(!node(m,image,root,nodes[index],region,manager,id,n,e))return false;
            if(!objects.insert(n.object).second||!uids.insert(n.uid).second)return fields::reject(e,"network duplicate node identity");
            if(!r::array(m,n.object,0x268,0x270,0x274,65536,out.node_pipes[n.object],e)||!r::array(m,n.object,0x2b0,0x2b8,0x2bc,65536,out.node_connections[n.object],e))return false;
            out.node_keys.emplace(n.object,json::array({id,index}));out.nodes.push_back(n);
        }
        for(size_t index=0;index<pipes.size();++index){
            Pipe p;if(!fields::pipe(m,image,pipes[index],region,manager,id,p,e))return false;
            if(!objects.insert(p.object).second||!uids.insert(p.uid).second)return fields::reject(e,"network duplicate pipe identity");
            out.pipe_keys.emplace(p.object,json::array({id,index}));out.pipe_values.push_back(p);
        }
    }
    std::set<uintptr_t> connections;
    auto connected=[&](const std::vector<uintptr_t>& native,uintptr_t owner,bool pipe,json& result){
        result=json::array();
        for(auto object:native){
            fields::Connection c;if(!connections.insert(object).second||!fields::connection(m,image,object,c,e))return false;
            if((pipe?c.pipe:c.node)!=owner||(pipe?c.node:c.pipe)||!region_ids.contains(c.region))return fields::reject(e,"network connection adjacency/owner differs");
            if(pipe){const auto it=std::find_if(out.pipe_values.begin(),out.pipe_values.end(),[&](const Pipe& p){return p.object==owner;});
                if(it==out.pipe_values.end()||c.waypoint<0||size_t(c.waypoint)>=it->points.size())return fields::reject(e,"network connection waypoint outside actual pipe");}
            result.push_back(json::array({region_ids.at(c.region),c.source_position,c.position,c.waypoint,c.pending,c.dynamic,c.connected}));
        }
        std::sort(result.begin(),result.end());return true;
    };
    for(auto& n:out.nodes){
        auto& edges=n.row[7];std::set<uintptr_t> seen;
        for(auto p:out.node_pipes.at(n.object)){
            if(!seen.insert(p).second||!out.pipe_keys.contains(p))return fields::reject(e,"network node pipe adjacency is duplicate or foreign");
            const auto it=std::find_if(out.pipe_values.begin(),out.pipe_values.end(),[&](const Pipe& value){return value.object==p;});
            if(it==out.pipe_values.end()||(it->start!=n.object&&it->end!=n.object))return fields::reject(e,"network node/pipe reciprocal endpoint differs");
            edges.push_back(out.pipe_keys.at(p));
        }
        std::sort(edges.begin(),edges.end());
        if(!connected(out.node_connections.at(n.object),n.object,false,n.row[8]))return false;
        rows.at(n.region_id)[2].push_back(n.row);
    }
    for(auto& p:out.pipe_values){
        if(!out.node_keys.contains(p.start)||!out.node_keys.contains(p.end))return fields::reject(e,"network pipe endpoint outside actual node graph");
        for(auto endpoint:{p.start,p.end}){
            const auto& edges=out.node_pipes.at(endpoint);
            if(std::count(edges.begin(),edges.end(),p.object)!=1)return fields::reject(e,"network pipe missing reciprocal node adjacency");
        }
        json linked;if(!connected(p.connections,p.object,true,linked))return false;
        p.row=json::array({p.type,p.key,p.cost,p.definition_values,p.cross_region,out.node_keys.at(p.start),out.node_keys.at(p.end),p.start_position,p.end_position,p.points,p.distances,p.current,p.last_current,linked});
        rows.at(p.region_id)[3].push_back(p.row);
    }
    for(const auto& [id,native]:local_pipes){std::set<uintptr_t> seen;
        for(auto p:native){if(!seen.insert(p).second||!out.pipe_keys.contains(p))return fields::reject(e,"network region local-pipe membership duplicate or foreign");rows.at(id)[4].push_back(out.pipe_keys.at(p));}
        std::sort(rows.at(id)[4].begin(),rows.at(id)[4].end());
    }
    out.pipes=out.pipe_values.size();out.connections=connections.size();
    for(const auto& [id,row]:rows){(void)id;out.rows.push_back(row);}return true;
}
inline bool delta(const Graph& before,const Graph& after,const action_session::NetworkIntent& request,uintptr_t created,std::string& e){
    if(after.nodes.size()!=before.nodes.size()+1||before.pipes!=after.pipes||before.connections!=after.connections)return fields::reject(e,"network original node count delta differs");
    const auto it=std::find_if(after.nodes.begin(),after.nodes.end(),[&](const Node& n){return n.object==created;});
    if(it==after.nodes.end()||before.node_keys.contains(created))return fields::reject(e,"network original new node absent or reused");const auto& n=*it;
    if(n.region_id!=request.region||n.type!=request.type||n.key!=request.key||n.cost!=request.quoted_cost||
       std::array<float,3>{n.transform[4],n.transform[5],n.transform[6]}!=request.position)return fields::reject(e,"network actual created node identity/definition/pose differs");
    for(const auto& old:before.nodes){const auto found=std::find_if(after.nodes.begin(),after.nodes.end(),[&](const Node& value){return value.object==old.object;});
        if(found==after.nodes.end()||found->uid!=old.uid)return fields::reject(e,"network existing private node identity changed");}
    auto expected=before.rows;for(auto& row:expected)if(row[0]==request.region)row[2].push_back(n.row);
    return expected==after.rows||fields::reject(e,"network original placement changed unrelated topology");
}
inline const Node* selected(const Graph& graph,int32_t region,int32_t index){
    int32_t current=0;for(const auto& n:graph.nodes)if(n.region_id==region&&current++==index)return &n;return nullptr;
}
inline bool pipe_delta(const Graph& before,const Graph& after,const action_session::NetworkPipeIntent& request,uintptr_t created,uint64_t& pipe_uid,uint64_t& new_uid,std::string& e){
    if(after.nodes.size()!=before.nodes.size()+size_t(request.end_node<0)||after.pipes!=before.pipes+1||after.connections!=before.connections)return fields::reject(e,"network Pipe native count delta differs");
    const auto start=selected(before,request.region,request.start_node);const auto end=selected(before,request.region,request.end_node);
    const auto it=std::find_if(after.pipe_values.begin(),after.pipe_values.end(),[&](const Pipe& p){return p.object==created;});
    if(!start||it==after.pipe_values.end()||before.pipe_keys.contains(created))return fields::reject(e,"network Pipe actual created identity is absent or reused");const auto& pipe=*it;
    if(pipe.region_id!=request.region||pipe.type!=request.type||pipe.key!=request.key||pipe.start!=start->object||pipe.start_position!=request.start||pipe.end_position!=request.end||pipe.points!=request.preview||(end&&pipe.end!=end->object))return fields::reject(e,"network Pipe actual type/endpoints/waypoints differ");
    pipe_uid=pipe.uid;new_uid=0;
    auto expected=before.rows;const auto key=after.pipe_keys.at(created);
    if(request.end_node<0){const auto node=std::find_if(after.nodes.begin(),after.nodes.end(),[&](const Node& n){return n.object==pipe.end;});
        if(node==after.nodes.end()||before.node_keys.contains(node->object)||node->region_id!=request.region||node->type!=request.node_type||node->key!="item_junction")return fields::reject(e,"network original Junction identity differs");
        const std::array<float,3> position{node->transform[4],node->transform[5],node->transform[6]};
        if(position!=request.end)return fields::reject(e,"network original Junction settled position differs actual_y="+std::to_string(position[1])+" required_y="+std::to_string(request.end[1]));
        new_uid=node->uid;for(auto& row:expected)if(row[0]==request.region)row[2].push_back(node->row);
    }
    for(const auto& old:before.nodes){const auto found=std::find_if(after.nodes.begin(),after.nodes.end(),[&](const Node& n){return n.object==old.object;});
        if(found==after.nodes.end()||found->uid!=old.uid||found->manager!=old.manager||found->definition!=old.definition)return fields::reject(e,"network Pipe changed existing private node identity");
        if(old.object==pipe.start||old.object==pipe.end){const auto index=before.node_keys.at(old.object)[1].get<size_t>();
            for(auto& row:expected)if(row[0]==old.region_id){row[2][index][7].push_back(key);std::sort(row[2][index][7].begin(),row[2][index][7].end());}}
    }
    for(const auto& old:before.pipe_values){const auto found=std::find_if(after.pipe_values.begin(),after.pipe_values.end(),[&](const Pipe& p){return p.object==old.object;});
        if(found==after.pipe_values.end()||found->uid!=old.uid)return fields::reject(e,"network Pipe changed existing private pipe identity");}
    for(auto& row:expected)if(row[0]==request.region){row[3].push_back(pipe.row);row[4].push_back(key);std::sort(row[4].begin(),row[4].end());}
    return expected==after.rows||fields::reject(e,"network Pipe changed unrelated canonical topology");
}
}

#include "live_host_state/memory_query_timing.h"
#include "node_native.h"
#include "scalar_membership.h"
#include "connected_network_reader.h"
#include "native_open_runtime.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/hash.h"
#include "placement_ui_probe/ui_probe.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <map>
namespace mt2mp::game::live_host_state::network {
namespace {
using Node=connected_network_native::Node;
using Pipe=connected_network_native::Pipe;
using Connection=network_topology_native::Connection;
struct Census {State state;std::vector<Node> nodes;std::vector<Pipe> pipes;std::vector<Connection> connections;};
bool fail(std::string& e,const std::string& text){e=text;return false;}
bool owner(const Node& a,const Node& b){return a.object==b.object&&a.uid==b.uid&&a.manager==b.manager&&a.region==b.region&&a.definition==b.definition&&a.type==b.type&&a.key==b.key&&a.transform==b.transform&&a.definition_values==b.definition_values;}
bool owner(const Pipe& a,const Pipe& b){return a.object==b.object&&a.uid==b.uid&&a.manager==b.manager&&a.region==b.region&&a.definition==b.definition&&a.start==b.start&&a.end==b.end;}
bool owner(const Connection& a,const Connection& b){return a.object==b.object&&a.region==b.region&&a.node==b.node&&a.pipe==b.pipe&&a.waypoint==b.waypoint&&a.position==b.position&&a.source_position==b.source_position&&a.connected==b.connected&&a.dynamic[6]==b.dynamic[6];}
bool same_owners(const Census& a,const Census& b){
    if(a.state.other_graph_sha256!=b.state.other_graph_sha256||a.nodes.size()!=b.nodes.size()||a.pipes.size()!=b.pipes.size()||a.connections.size()!=b.connections.size())return false;
    for(size_t i=0;i<a.nodes.size();++i)if(!owner(a.nodes[i],b.nodes[i]))return false;
    for(size_t i=0;i<a.pipes.size();++i)if(!owner(a.pipes[i],b.pipes[i]))return false;
    for(size_t i=0;i<a.connections.size();++i)if(!owner(a.connections[i],b.connections[i]))return false;
    return true;
}
bool world(const Pin& pin,std::string& e){
    uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,e))return false;
    if(image!=pin.image||thread!=pin.thread||thread!=GetCurrentThreadId()||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"network actual resolver/Mainthread differs");
    building_mixed_identity::opened_file::CurrentWorld actual;
    if(!native_open_runtime::read_current_world(actual,e))return false;
    if(uintptr_t(actual.world)!=pin.world||actual.world_epoch!=pin.binding.native_epoch||actual.generation!=pin.binding.native_generation||!actual.map)return fail(e,"network actual NativeOpen world differs");
    return true;
}
bool once(const Pin& pin,Census& out,std::string& e){
    apply_timing::PageReadScope pages;
    out={};if(!world(pin,e))return false;
    building_mixed_identity::opened_file::CurrentWorld actual;
    if(!native_open_runtime::read_current_world(actual,e))return false;
    placement_ui_probe::ProcessMemory memory;connected_network_native::Graph graph;
    if(!connected_network_native::graph(memory,pin.image,uintptr_t(actual.map)+0x1c0,graph,e))return false;
    auto other=graph.rows;
    for(auto& region:other)for(auto& node:region[2]){node[5]=nullptr;node[6]=nullptr;}
    for(auto& region:other)for(auto& pipe:region[3]){pipe[11]=nullptr;pipe[12]=nullptr;}
    // Re-sort after removing only transferred scalars, so identity never depends on timers.
    auto mask=[](auto& rows){for(auto& c:rows){c[4]=nullptr;for(size_t i=0;i<6;++i)c[5][i]=nullptr;}std::sort(rows.begin(),rows.end());};
    for(auto& region:other){for(auto& node:region[2])mask(node[8]);for(auto& pipe:region[3])mask(pipe[13]);}
    const auto text=other.dump();out.state.other_graph_sha256=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));
    std::map<std::pair<int32_t,int32_t>,Node> ordered;
    for(const auto& node:graph.nodes){const auto& key=graph.node_keys.at(node.object);ordered.emplace(std::pair{key[0].get<int32_t>(),key[1].get<int32_t>()},node);}
    for(const auto& [key,node]:ordered){out.nodes.push_back(node);out.state.nodes.push_back({key.first,key.second,node.state,node.flags});}
    std::map<std::pair<int32_t,int32_t>,Pipe> pipes;
    for(const auto& pipe:graph.pipe_values){const auto& key=graph.pipe_keys.at(pipe.object);pipes.emplace(std::pair{key[0].get<int32_t>(),key[1].get<int32_t>()},pipe);}
    for(const auto& [key,pipe]:pipes){out.pipes.push_back(pipe);out.state.pipes.push_back({key.first,key.second,pipe.current,pipe.last_current});}
    std::map<std::array<int32_t,4>,Connection> connections;
    auto add_connections=[&](const std::vector<uintptr_t>& objects,int32_t region,int32_t kind,int32_t index){
        std::map<nlohmann::json,Connection> sorted;
        for(auto object:objects){
            Connection c;int32_t actual_region=-1;
            if(!network_topology_native::connection(memory,pin.image,object,c,e)||!connected_network_native::read::at(memory,c.region,0x834,actual_region,e))return false;
            auto key=nlohmann::json::array({actual_region,c.source_position,c.position,c.waypoint,c.dynamic[6],c.connected});
            if(!sorted.emplace(key,c).second)return fail(e,"network connection static adjacency is ambiguous");
        }
        int32_t rank=0;for(const auto& [key,c]:sorted){(void)key;connections.emplace(std::array<int32_t,4>{region,kind,index,rank++},c);}return true;
    };
    for(const auto& [key,node]:ordered)if(!add_connections(graph.node_connections.at(node.object),key.first,0,key.second))return false;
    for(const auto& [key,pipe]:pipes)if(!add_connections(pipe.connections,key.first,1,key.second))return false;
    for(const auto& [key,c]:connections){
        ConnectionRow row;row.key=key;row.values[0]=c.pending;
        for(size_t i=0;i<6;++i)row.values[i+1]=c.dynamic[i];
        out.connections.push_back(c);out.state.connections.push_back(row);
    }
    return true;
}
bool read(const Pin& pin,Census& out,std::string& e){
    Census first,second;if(!once(pin,first,e)||!once(pin,second,e))return false;
    if(!same_owners(first,second)||first.state!=second.state)return fail(e,"network actual graph/owners changed during double read");
    out=std::move(second);return true;
}
DWORD store_raw(uintptr_t image,const Node* node,size_t field,uint32_t before,uint32_t after)noexcept{
    constexpr size_t offsets[]={0x250,0x2a0,0x2a4,0x2e4,0x2e8,0x2ea};
    __try{
        auto* p=reinterpret_cast<unsigned char*>(node->object);
        if(*reinterpret_cast<uintptr_t*>(p)!=image+0x16fc700||*reinterpret_cast<uint64_t*>(p+0x18)!=node->uid)return ERROR_INVALID_DATA;
        if(field<4){uint32_t actual=0;std::memcpy(&actual,p+offsets[field],4);if(actual!=before)return ERROR_INVALID_DATA;std::memcpy(p+offsets[field],&after,4);}
        else {if(before>1||after>1||p[offsets[field]]!=before)return ERROR_INVALID_DATA;p[offsets[field]]=uint8_t(after);}
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD notify_raw(uintptr_t image,uintptr_t manager)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(image+0x2a4fc0)(reinterpret_cast<void*>(manager));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
// Both vectors belong to native vsArray<float>. The second is BlurCurrent's scratch result.
DWORD pipe_raw(uintptr_t image,const Pipe* pipe,const PipeRow* before,const PipeRow* target,PipeEffect* effect)noexcept{
    __try{
        auto* p=reinterpret_cast<unsigned char*>(pipe->object);
        if(*reinterpret_cast<uintptr_t*>(p)!=image+0x16fca60||*reinterpret_cast<uint64_t*>(p+0x18)!=pipe->uid)return ERROR_INVALID_DATA;
        const std::vector<float>* expected[]={&before->current,&before->smoothing};
        const std::vector<float>* desired[]={&target->current,&target->smoothing};
        // Check both arrays before the first allocation or store.
        for(size_t field=0;field<2;++field){
            auto* array=p+(field?0x248:0x230);
            const auto count=*reinterpret_cast<int32_t*>(array+0x10),capacity=*reinterpret_cast<int32_t*>(array+0x14);
            const auto* data=*reinterpret_cast<float**>(array+8);
            if(count<0||count>65536||capacity<count||capacity>65536||size_t(count)!=expected[field]->size()||(count&&!data))return ERROR_INVALID_DATA;
            if(count&&std::memcmp(data,expected[field]->data(),size_t(count)*sizeof(float)))return ERROR_INVALID_DATA;
            if(desired[field]->size()>65536||(!field&&desired[field]->size()!=size_t(count)))return ERROR_INVALID_DATA;
        }
        for(size_t field=0;field<2;++field){
            if(*expected[field]==*desired[field])continue;
            auto* array=p+(field?0x248:0x230);const auto count=int32_t(desired[field]->size());
            if(count>*reinterpret_cast<int32_t*>(array+0x14)){
                ++effect->resize_attempts;
                reinterpret_cast<void(*)(void*,int32_t)>(image+0x42f1a0)(array,count);
                ++effect->resizes;
            }
            auto* data=*reinterpret_cast<float**>(array+8);
            if(count&&!data)return ERROR_INVALID_DATA;
            if(count)std::memcpy(data,desired[field]->data(),size_t(count)*sizeof(float));
            *reinterpret_cast<int32_t*>(array+0x10)=count;
            ++effect->arrays_written;
        }
        if(before->current!=target->current){
            // Same original renderer used by NetworkFinishUpdate, with normalized native values.
            effect->display_attempted=true;
            reinterpret_cast<void(*)(void*)>(image+0x431130)(p);
            effect->display_completed=true;
            float old_total=0,new_total=0;
            for(float value:before->current)old_total+=value;
            for(float value:target->current)new_total+=value;
            if(pipe->definition_values[2]>0&&old_total!=new_total&&(old_total<=0||new_total<=0)){
                effect->queue_attempted=true;
                reinterpret_cast<void(*)(void*)>(image+0x2a4fc0)(reinterpret_cast<void*>(pipe->manager));
                effect->queue_completed=true;
            }
        }
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

DWORD connection_raw(uintptr_t image,const Connection* connection,size_t field,uint32_t before,uint32_t after)noexcept{
    constexpr size_t offsets[]={0x178,0x188,0x18c,0x190,0x194,0x198,0x19c};
    __try{
        auto* p=reinterpret_cast<unsigned char*>(connection->object);
        if(field>=7||*reinterpret_cast<uintptr_t*>(p)!=image+0x17251d0||
           *reinterpret_cast<uintptr_t*>(p+0x158)!=connection->node||*reinterpret_cast<uintptr_t*>(p+0x160)!=connection->pipe||
           *reinterpret_cast<uintptr_t*>(p+0x180)!=connection->region||*reinterpret_cast<int32_t*>(p+0x168)!=connection->waypoint||p[0x1a4]!=1)return ERROR_INVALID_DATA;
        uint32_t actual=0;std::memcpy(&actual,p+offsets[field],4);if(actual!=before)return ERROR_INVALID_DATA;
        std::memcpy(p+offsets[field],&after,4);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

class Native final:public Port {
    live_host_state::Port& root_;Pin pin_;Census pin_nodes_;bool planned_=false,sampled_=false,scalar_active_=false;
    ScalarMembership scalar_members_;std::vector<std::pair<bool,size_t>> scalar_parents_;
    bool scalar_root(std::string& e){
        Pin actual;if(!root_.read(actual,e))return false;
        auto expected=pin_;expected.values={};actual.values={};
        return (actual.context.role==action_session::Role::Client&&actual.selected==0&&actual==expected)||fail(e,"network scalar actual Client Clock0/epoch/owners differ");
    }
    bool scalar_region(uintptr_t region,uintptr_t manager,int32_t id,std::string& e){
        namespace r=network_node_native::read;placement_ui_probe::ProcessMemory memory;
        uintptr_t map=0,shard=0,actual_manager=0,owner=0;int32_t actual_id=-1;
        if(!r::at(memory,pin_.image,0x13df8c0,map,e)||!r::ap(memory,map,pin_.image,0x1742860,e)||!r::ap(memory,region,pin_.image,0x1787930,e)||
           !r::at(memory,region,0x70,shard,e)||!r::at(memory,region,0x834,actual_id,e)||!r::at(memory,region,0x978,actual_manager,e)||
           !r::ap(memory,manager,pin_.image,0x17141e0,e)||!r::at(memory,manager,0x168,owner,e))return false;
        return (shard==map+0x1c0&&actual_id==id&&actual_manager==manager&&owner==region)||fail(e,"network scalar Region/root/manager reciprocity differs");
    }
    bool scalar_node(size_t index,std::string& e){
        placement_ui_probe::ProcessMemory memory;const auto& expected=pin_nodes_.nodes[index];
        if(!scalar_members_.check(memory,expected.object,e)||!scalar_region(expected.region,expected.manager,expected.region_id,e))return false;
        uintptr_t root=0;if(!network_node_native::read::at(memory,expected.region,0x70,root,e))return false;
        Node actual;if(!connected_network_native::node(memory,pin_.image,root,expected.object,expected.region,expected.manager,expected.region_id,actual,e))return false;
        return owner(actual,expected)||fail(e,"network scalar native Node owner differs");
    }
    bool scalar_pipe(size_t index,std::string& e){
        placement_ui_probe::ProcessMemory memory;const auto& expected=pin_nodes_.pipes[index];
        if(!scalar_members_.check(memory,expected.object,e)||!scalar_region(expected.region,expected.manager,expected.region_id,e))return false;
        Pipe actual;if(!network_topology_native::pipe(memory,pin_.image,expected.object,expected.region,expected.manager,expected.region_id,actual,e))return false;
        return owner(actual,expected)||fail(e,"network scalar native Pipe owner differs");
    }
    bool scalar_connection(size_t index,std::string& e){
        placement_ui_probe::ProcessMemory memory;const auto& expected=pin_nodes_.connections[index];
        if(!scalar_members_.check(memory,expected.object,e))return false;
        Connection actual;if(!network_topology_native::connection(memory,pin_.image,expected.object,actual,e))return false;
        if(!owner(actual,expected))return fail(e,"network scalar Connection reciprocal owner/key differs");
        const auto [node,parent]=scalar_parents_[index];return node?scalar_node(parent,e):scalar_pipe(parent,e);
    }
    activation::Owner activation_owner(size_t i)const{const auto& n=pin_nodes_.nodes[i];return {pin_.image,n.object,n.region,n.manager,n.definition,n.uid,pin_.thread};}
    bool current(Census& out,std::string& e){
        Pin p;if(!root_.read(p,e))return false;
        auto expected=pin_;expected.values={};auto identity=p;identity.values={};
        if(p.context.role!=action_session::Role::Client||p.selected!=0||identity!=expected)return fail(e,"network current Client Clock0/epoch/owners differ");
        if(!network::read(p,out,e))return false;
        if(!same_owners(pin_nodes_,out))return fail(e,"network current private graph ownership differs");return true;
    }
public:
    explicit Native(live_host_state::Port& root):root_(root){}
    bool read(State& out,std::string& e)override{
        sampled_=false;Pin actual;Census nodes;if(!root_.read(actual,e)||!network::read(actual,nodes,e))return false;
        if(planned_){auto p=actual,expected=pin_;p.values={};expected.values={};if(p!=expected||!same_owners(pin_nodes_,nodes))return fail(e,"network final native owners changed");}
        if(!planned_){pin_=actual;pin_nodes_=std::move(nodes);out=pin_nodes_.state;sampled_=true;}
        else out=std::move(nodes.state);
        return true;
    }
    std::optional<Receipt> confirm_no_effect(const State& before,const State& target)override{
        const bool sampled=sampled_;sampled_=false;
        if(!exact_bits(before,target))return std::nullopt;
        Receipt r;
        if(!sampled||planned_||!exact_bits(before,pin_nodes_.state)){r.cause="network no-effect requires fresh local census";return r;}
        if(pin_.context.role!=action_session::Role::Client||pin_.selected!=0){r.cause="network no-effect requires actual Client Clock0";return r;}
        for(size_t i=0;i<target.nodes.size();++i){
            const auto& t=target.nodes[i];const double capacity=pin_nodes_.nodes[i].definition_values[1];
            if(capacity<0||capacity>INT32_MAX||t.values[0]<0||t.values[0]>float(int32_t(capacity))||t.values[0]!=t.values[1]){r.cause="network Host current violates original Finish clamp/completion";return r;}
        }
        r.actual=before;r.actual_present=true;r.confirmed=true;return r;
    }
    bool preflight(const State& before,const State& target,std::string& e)override{
        if(!root_.read(pin_,e)||pin_.context.role!=action_session::Role::Client||pin_.selected!=0)return fail(e,e.empty()?"network stores require actual Client Clock0":e);
        if(!network::read(pin_,pin_nodes_,e)||pin_nodes_.state!=before)return fail(e,e.empty()?"network actual expected-before changed":e);
        if(before.other_graph_sha256!=target.other_graph_sha256||before.nodes.size()!=target.nodes.size()||before.pipes.size()!=target.pipes.size()||before.connections.size()!=target.connections.size())return fail(e,"network complete non-Node graph differs");
        placement_ui_probe::ProcessMemory memory;unsigned char bytes[8]{};
        constexpr unsigned char expected[]={0xc6,0x81,0x70,0x01,0,0,0x01,0xc3};
        if(!memory.copy(pin_.image+0x2a4fc0,bytes,sizeof bytes,e)||std::memcmp(bytes,expected,sizeof bytes))return fail(e,e.empty()?"network native bandwidth notification bytes differ":e);
        for(size_t i=0;i<target.nodes.size();++i){const auto& t=target.nodes[i];const auto& a=before.nodes[i];
            if(t.region!=a.region||t.index!=a.index||(a.flags[2]&&!t.flags[2]))return fail(e,"network key/Activated reversal differs before stores");
            if(!a.flags[2]&&t.flags[2]&&!activation::preflight(activation_owner(i),false,true,e))return false;
            const double capacity=pin_nodes_.nodes[i].definition_values[1];
            if(capacity<0||capacity>INT32_MAX||t.values[0]<0||t.values[0]>float(int32_t(capacity))||t.values[0]!=t.values[1])return fail(e,"network Host current violates original Finish clamp/completion");
        }
        for(size_t i=0;i<target.pipes.size();++i){const auto& a=before.pipes[i];const auto& b=target.pipes[i];
            if(a.region!=b.region||a.index!=b.index||a.current.size()!=b.current.size())return fail(e,"network pipe key/current length differs before stores");
        }
        for(size_t i=0;i<target.connections.size();++i)if(before.connections[i].key!=target.connections[i].key)return fail(e,"network connection owner/rank differs before stores");
        planned_=true;return true;
    }
    bool begin_scalar_stores(std::string& e)override{
        if(scalar_active_)return fail(e,"network scalar block already active; no reentrant batch");
        if(!planned_)return fail(e,"network scalar block requires actual preflight");
        Census actual;if(!current(actual,e))return false;
        ScalarMembership members;std::vector<std::pair<bool,size_t>> parents;
        placement_ui_probe::ProcessMemory memory;
        std::map<uintptr_t,size_t> nodes,pipes;
        for(size_t i=0;i<pin_nodes_.nodes.size();++i){const auto& n=pin_nodes_.nodes[i];nodes.emplace(n.object,i);if(!members.add(memory,n.manager,0x20,n.object,pin_nodes_.state.nodes[i].index,e))return false;}
        for(size_t i=0;i<pin_nodes_.pipes.size();++i){const auto& p=pin_nodes_.pipes[i];pipes.emplace(p.object,i);if(!members.add(memory,p.manager,0x50,p.object,pin_nodes_.state.pipes[i].index,e))return false;}
        for(const auto& c:pin_nodes_.connections){
            const bool node=c.node!=0;const auto& list=node?nodes:pipes;const auto found=list.find(node?c.node:c.pipe);
            if(found==list.end())return fail(e,"network scalar Connection parent absent from actual graph");
            if(!members.add(memory,node?c.node:c.pipe,node?0x2b0:0x2c0,c.object,-1,e))return false;
            parents.emplace_back(node,found->second);
        }
        scalar_members_=std::move(members);scalar_parents_=std::move(parents);scalar_active_=true;return true;
    }
    void end_scalar_stores()noexcept override{scalar_active_=false;scalar_members_={};scalar_parents_.clear();}
    activation::Receipt activate(size_t node)override{
        end_scalar_stores();
        activation::Receipt r;
        if(!planned_||node>=pin_nodes_.nodes.size()){r.cause="network activation plan absent";return r;}
        Census actual;if(!current(actual,r.cause))return r;
        if(actual.state.nodes[node].flags[2]){r.cause="network activation already occurred; no replay";return r;}
        return activation::invoke(activation_owner(node),false,true);
    }
    bool store(size_t node,size_t field,uint32_t before,uint32_t target,std::string& e)override{
        if(!planned_||node>=pin_nodes_.nodes.size()||field>=6)return fail(e,"network fixed scalar plan/field absent");
        if(scalar_active_){if(!scalar_root(e)||!scalar_node(node,e))return false;}
        else {Census actual;if(!current(actual,e))return false;}
        const auto code=store_raw(pin_.image,&pin_nodes_.nodes[node],field,before,target);
        return !code||fail(e,"network native scalar SEH/compare code="+std::to_string(code));
    }
    bool store_pipe(size_t index,const PipeRow& before,const PipeRow& target,PipeEffect& effect,std::string& e)override{
        end_scalar_stores();
        if(!planned_||index>=pin_nodes_.pipes.size())return fail(e,"network pipe plan absent");
        Census actual;if(!current(actual,e))return false;
        if(actual.state.pipes[index]!=before)return fail(e,"network pipe expected arrays changed");
        const auto code=pipe_raw(pin_.image,&pin_nodes_.pipes[index],&before,&target,&effect);
        return !code||fail(e,"network pipe native array/display SEH/compare code="+std::to_string(code));
    }
    bool store_connection(size_t index,size_t field,uint32_t before,uint32_t target,std::string& e)override{
        if(!planned_||index>=pin_nodes_.connections.size()||field>=7)return fail(e,"network connection scalar plan absent");
        if(scalar_active_){if(!scalar_root(e)||!scalar_connection(index,e))return false;}
        else {Census actual;if(!current(actual,e))return false;}
        const auto code=connection_raw(pin_.image,&pin_nodes_.connections[index],field,before,target);
        return !code||fail(e,"network connection native compare/SEH code="+std::to_string(code));
    }
    bool notify_supply(size_t node,std::string& e)override{
        end_scalar_stores();
        if(!planned_||node>=pin_nodes_.nodes.size())return fail(e,"network notification plan absent");
        Census actual;if(!current(actual,e))return false;
        const auto code=notify_raw(pin_.image,pin_nodes_.nodes[node].manager);
        return !code||fail(e,"network native notification SEH="+std::to_string(code));
    }
};
}
bool read_native(const Pin& pin,State& out,std::string& e){Census nodes;if(!read(pin,nodes,e)||!valid(nodes.state,e))return false;out=std::move(nodes.state);return true;}
bool preflight_native(live_host_state::Port& root,const State& target,std::string& e){
    Native port(root);State before;if(!valid(target,e)||!port.read(before,e)||!valid(before,e))return false;if(auto unchanged=port.confirm_no_effect(before,target)){e=unchanged->cause;return unchanged->confirmed;}return port.preflight(before,target,e);
}
Receipt apply_native(live_host_state::Port& root,const State& target){Native port(root);return apply(port,target);}
}

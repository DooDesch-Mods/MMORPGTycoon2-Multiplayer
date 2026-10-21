#pragma once
#include "district_edit_history.h"
#include "dungeon_cell_graph.h"
#include "duplicate_history_reader.h"

namespace mt2mp::game::district_stroke_graph {
namespace q=district_saved_identity;
struct Node {
    uintptr_t pointer=0,rva=0;
    uint64_t uid=0;
    district_edit_history::Stored stored;
    int32_t cash=0;
    std::array<uint32_t,3> position{};
    std::array<std::vector<uint64_t>,3> dependencies;
    bool operator==(const Node&)const=default;
};
struct Graph {
    uintptr_t pointer=0;
    std::vector<Node> nodes;
    std::vector<size_t> order;
    bool operator==(const Graph&)const=default;
};
template<class Memory>
bool read(Memory& memory,uintptr_t image,uintptr_t graph,Graph& out,std::string& error){
    auto once=[&](Graph& value){
        value.pointer=graph;q::Bounds children,node_span,deleting;uint8_t locked=0;
        if(!q::typed(memory,image,graph,0x16e32d0,error)||!q::bounds(memory,graph,0,children,error)||
           !q::typed(memory,image,graph+0x18,0x16e2dc0,error)||!q::bounds(memory,graph+0x18,100000,node_span,error)||
           !q::typed(memory,image,graph+0x48,0x1748c50,error)||!q::bounds(memory,graph+0x48,0,deleting,error)||
           !q::at(memory,graph,0x10c,locked,error))return false;
        if(!node_span.count||locked!=1)return q::fail(error,"District stroke inverse is empty or not locked");
        duplicate_history::Graph order_source;std::map<uintptr_t,size_t> indices;
        auto at=[&](uintptr_t pointer,size_t offset,auto& result,std::string& cause){return q::at(memory,pointer,offset,result,cause);};
        for(int32_t index=0;index<node_span.count;++index){
            Node node;uintptr_t ap=0,method=0;
            if(!q::at(memory,node_span.data,size_t(index)*8,node.pointer,error)||!q::at(memory,node.pointer,0,ap,error))return false;
            if(ap<image||!indices.emplace(node.pointer,value.nodes.size()).second)return q::fail(error,"District inverse repeats an operation or has a foreign addresspoint");
            node.rva=ap-image;uintptr_t expected=0;
            if(node.rva==0x17132b0)expected=0x54b5b0;
            else if(node.rva==0x17132f0)expected=0x549fc0;
            else if(node.rva==0x16de860)expected=0x53e1c0;
            else if(node.rva==0x1786fb0)expected=0x53ccf0;
            else return q::fail(error,"District inverse contains an unbound original operation");
            if(!q::at(memory,ap,0x10,method,error)||method!=image+expected)return q::fail(error,"District inverse original Do method differs");
            for(size_t set=0;set<3;++set)if(!dungeon_cell_graph::uid_set(at,node.pointer+8+set*0x28,node.dependencies[set],error))return false;
            if(node.rva==0x17132b0||node.rva==0x17132f0){
                if(!district_edit_history::read_node(memory,image,node.pointer,node.stored,error,node.rva))return false;
                node.uid=node.stored.uid;
            }else if(node.rva==0x16de860){
                if(!q::at(memory,node.pointer,0xb8,node.uid,error)||!node.uid)return q::fail(error,"District original Destroy full UID absent");
            }else{
                if(!q::at(memory,node.pointer,0xb8,node.cash,error)||!q::at(memory,node.pointer,0xbc,node.position,error))return false;
                for(auto bits:node.position)if(!std::isfinite(std::bit_cast<float>(bits)))return q::fail(error,"District original Cash position is not finite");
                if(node.cash)return q::fail(error,"District cost-free original stroke has a nonzero Cash inverse");
            }
            order_source.top_operations.push_back(node.pointer);value.nodes.push_back(std::move(node));
        }
        std::vector<uintptr_t> order;
        if(!duplicate_history::execution_order(memory,image,graph,order_source,order,error))return false;
        for(auto pointer:order)value.order.push_back(indices.at(pointer));
        return q::unchanged(memory,graph,0,children,error)&&q::unchanged(memory,graph+0x18,100000,node_span,error)&&
            q::unchanged(memory,graph+0x48,0,deleting,error);
    };
    Graph before,after;if(!once(before)||!once(after))return false;
    if(before!=after)return q::fail(error,"District complete original inverse changed during double read");
    out=std::move(after);return true;
}
}

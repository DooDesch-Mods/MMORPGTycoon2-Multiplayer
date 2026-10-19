#pragma once
#include "duplicate_history_reader.h"
#include <array>
#include <algorithm>
namespace mt2mp::game::dungeon_cell_graph {
namespace q=npc_quest_targets;
enum class Kind:uint8_t {Paint,Layout,Scenery,Monster,Zone,EditZone,Gizmo,Door,Destroy,Quests};
struct Descriptor {Kind kind;uintptr_t ap,method;size_t end;};
inline constexpr std::array<Descriptor,10> descriptors{{
    {Kind::Paint,0x1713270,0x53eaf0,0xd8},{Kind::Layout,0x1719190,0x53f490,0x158},
    {Kind::Scenery,0x1706590,0x55a3c0,0x141},{Kind::Monster,0x1706550,0x5507c0,0x178},
    {Kind::Zone,0x16e4a00,0x55bd50,0x150},{Kind::EditZone,0x16e4980,0x55c710,0x150},
    {Kind::Gizmo,0x16fb990,0x54c680,0x18c},{Kind::Door,0x17191d0,0x54c920,0x180},
    {Kind::Destroy,0x16de860,0x53e1c0,0xc0},{Kind::Quests,0x1713330,0x563f50,0xe0}}};
struct Node {
    uintptr_t pointer=0;Kind kind=Kind::Paint;uint64_t uid=0;
    std::array<std::vector<uint64_t>,3> dependencies;
    std::vector<uintptr_t> regions;std::vector<unsigned char> payload;std::vector<std::string> values;
    bool operator==(const Node&)const=default;
};
struct Graph {uintptr_t pointer=0;std::vector<Node> nodes;std::vector<uintptr_t> execution;bool operator==(const Graph&)const=default;};
// Original mmoDo stores a hash set and an insertion-order vector for each UID
// relation. Validate both representations rather than trusting either count.
template<class Read>bool uid_set(Read& at,uintptr_t object,std::vector<uint64_t>& out,std::string& error){
    out.clear();uintptr_t table=0,values=0;int32_t buckets=0,shift=0,count=0,capacity=0;
    if(!at(object,0,table,error)||!at(object,8,buckets,error)||!at(object,0xc,shift,error)||!at(object,0x18,values,error)||
       !at(object,0x20,count,error)||!at(object,0x24,capacity,error))return false;
    if(!table||!values||buckets<1||buckets>1048576||(buckets&(buckets-1))||shift<0||shift>31||
       uint64_t(buckets)!=(uint64_t(1)<<(32-shift))||count<0||count>100000||capacity<count||capacity>1000000||
       table>UINTPTR_MAX-size_t(buckets)*24||values>UINTPTR_MAX-size_t(count)*8)return q::fail(error,"Dungeon original Do dependency set header/span differs");
    std::set<uint64_t> expected;for(int32_t i=0;i<count;++i){uint64_t uid=0;if(!at(values,size_t(i)*8,uid,error))return false;
        if(!uid||!expected.insert(uid).second)return q::fail(error,"Dungeon original Do dependency UID vector repeats or is zero");out.push_back(uid);}
    std::set<uintptr_t> nodes;std::set<uint64_t> seen;
    for(int32_t i=0;i<buckets;++i){uintptr_t link=0;if(!at(table,size_t(i)*24+0x10,link,error))return false;
        while(link){uint64_t uid=0;uint32_t hash=0;uintptr_t next=0;
            if(!nodes.insert(link).second||nodes.size()>expected.size())return q::fail(error,"Dungeon original Do dependency chain cycles or exceeds its vector");
            if(!at(link,0,uid,error)||!at(link,8,hash,error)||!at(link,0x10,next,error))return false;
            if(!expected.contains(uid)||!seen.insert(uid).second||hash!=uint32_t(uid)||int32_t((hash*0x9e3779ffu)>>shift)!=i)
                return q::fail(error,"Dungeon original Do dependency hash bucket/full UID differs");link=next;
        }
    }
    if(seen!=expected)return q::fail(error,"Dungeon original Do dependency hash omitted an ordered UID");
    uintptr_t final_table=0,final_values=0;int32_t final_count=0,final_buckets=0,final_shift=0,final_capacity=0;
    if(!at(object,0,final_table,error)||!at(object,8,final_buckets,error)||!at(object,0xc,final_shift,error)||!at(object,0x18,final_values,error)||
       !at(object,0x20,final_count,error)||!at(object,0x24,final_capacity,error))return false;
    if(table!=final_table||values!=final_values||count!=final_count||buckets!=final_buckets||shift!=final_shift||capacity!=final_capacity)
        return q::fail(error,"Dungeon original Do dependency set changed during complete read");
    for(size_t i=0;i<out.size();++i){uint64_t uid=0;if(!at(values,i*8,uid,error)||uid!=out[i])return q::fail(error,error.empty()?"Dungeon original Do dependency vector changed during read":error);}
    return true;
}
template<class Memory,class Read,class Copy,class Values,class Owner>
bool read(Memory& memory,Read& at,Copy& copy,Values& values,Owner& owner,uintptr_t image,uintptr_t pointer,Graph& out,std::string& error){
    out={};q::Bounds array;if(!q::typed(memory,image,pointer+0x18,0x16e2dc0,error)||!q::bounds(memory,pointer+0x18,100000,array,error)||!array.count)
        return q::fail(error,error.empty()?"Dungeon original complete DoGraph node array is empty":error);
    Graph next;next.pointer=pointer;duplicate_history::Graph membership;std::set<uintptr_t> unique;size_t paints=0,layouts=0;
    for(int32_t i=0;i<array.count;++i){Node node;uintptr_t ap=0,method=0;
        if(!at(array.data,size_t(i)*8,node.pointer,error)||!node.pointer||!unique.insert(node.pointer).second||!at(node.pointer,0,ap,error))return q::fail(error,error.empty()?"Dungeon original DoGraph node is absent or repeated":error);
        const auto descriptor=std::find_if(descriptors.begin(),descriptors.end(),[&](const auto& d){return ap==image+d.ap;});
        if(descriptor==descriptors.end())return q::fail(error,"Dungeon original DoGraph contains an unsupported original node addresspoint="+std::to_string(ap-image));
        if(!at(ap,0x10,method,error)||method!=image+descriptor->method||!at(node.pointer,0xb8,node.uid,error)||!node.uid)
            return q::fail(error,error.empty()?"Dungeon original Do virtual method/full target UID differs":error);
        node.kind=descriptor->kind;paints+=node.kind==Kind::Paint;layouts+=node.kind==Kind::Layout;
        for(size_t set=0;set<3;++set)if(!uid_set(at,node.pointer+8+set*0x28,node.dependencies[set],error))return false;
        q::Bounds regions;if(!q::typed(memory,image,node.pointer+0xa0,0x1749040,error)||!q::bounds(memory,node.pointer+0xa0,4096,regions,error))return false;
        for(int32_t r=0;r<regions.count;++r){uintptr_t region=0;if(!at(regions.data,size_t(r)*8,region,error)||!owner(node,region,error))return false;node.regions.push_back(region);}
        if(!q::unchanged(memory,node.pointer+0xa0,4096,regions,error))return false;
        node.payload.resize(descriptor->end-0x80);if(!copy(node.pointer+0x80,node.payload.data(),node.payload.size(),error)||!values(node,error))return false;
        membership.top_operations.push_back(node.pointer);next.nodes.push_back(std::move(node));
    }
    if(paints!=1||layouts!=1)return q::fail(error,"Dungeon original graph requires exactly one complete Paint and Layout node");
    if(!q::unchanged(memory,pointer+0x18,100000,array,error)||!duplicate_history::execution_order(memory,image,pointer,membership,next.execution,error))return false;
    out=std::move(next);error.clear();return true;
}
}

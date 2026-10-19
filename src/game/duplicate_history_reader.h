#pragma once
#include "duplicate_history.h"
#include "npc_quest_targets.h"
#include <tuple>
namespace mt2mp::game::duplicate_history {
struct Known {
    uintptr_t object=0,root=0,region=0;uint64_t uid=0;
    a::DuplicateObject row;std::array<uint8_t,0x6c> transform{};
    std::array<int32_t,2> scenery{};uint32_t height=0;uint8_t automatic=0;std::string name;
};
struct Graph {
    std::vector<Cash> cash;std::vector<std::string> targets;std::vector<uintptr_t> operations,top_operations;
};
template<class Memory>
bool execution_order(Memory& memory,uintptr_t image,uintptr_t graph,const Graph& saved,std::vector<uintptr_t>& out,std::string& e){
    namespace q=npc_quest_targets;out.clear();q::Bounds sorted;
    if(!q::typed(memory,image,graph+0x30,0x1748aa0,e)||!q::bounds(memory,graph+0x30,100000,sorted,e))return false;
    std::set<uintptr_t> expected(saved.top_operations.begin(),saved.top_operations.end()),seen;
    if(expected.size()!=saved.top_operations.size()||size_t(sorted.count)!=expected.size())return q::fail(e,"Duplicate history original sorted graph count differs");
    for(int32_t i=0;i<sorted.count;++i){uintptr_t pointer=0;
        if(!q::at(memory,sorted.data,size_t(i)*8,pointer,e))return false;
        if(!expected.contains(pointer)||!seen.insert(pointer).second)return q::fail(e,"Duplicate history original sorted graph member repeats or is foreign");
        out.push_back(pointer);}
    return q::unchanged(memory,graph+0x30,100000,sorted,e);
}
template<class Memory,class Find,class Text,class Building,class Related>
bool read(Memory& m,uintptr_t image,uintptr_t graph,bool undo,const std::map<uint64_t,Known>& known,
          Find&& find,Text&& text,Building&& building,Related&& related,Graph& out,std::string& e){
    namespace q=npc_quest_targets;out={};q::Bounds nodes;std::set<uintptr_t> operations;std::set<uint64_t> targets;
    auto method=[&](uintptr_t op,uintptr_t addresspoint,uintptr_t rva){uintptr_t actual=0;
        return q::typed(m,image,op,addresspoint,e)&&q::at(m,image+addresspoint,0x10,actual,e)&&
            (actual==image+rva||q::fail(e,"Duplicate history original Do virtual method differs"));};
    auto identity=[&](uintptr_t op,const Known*& saved,bool nested){uint64_t uid=0;uintptr_t actual=0;
        if(!q::at(m,op,0xb8,uid,e))return false;const auto found=known.find(uid);
        if(!uid||found==known.end()||!targets.insert(uid).second||!operations.insert(op).second)
            return q::fail(e,"Duplicate history original target is unknown or repeated");
        saved=&found->second;
        if(saved->uid!=uid||nested!=!saved->row.parent.empty())return q::fail(e,"Duplicate history original top/child ownership differs");
        if(!find(uid,actual,e))return false;
        if(undo?actual!=saved->object:bool(actual))return q::fail(e,"Duplicate history original full UID alive/absent state differs key="+saved->row.key);
        out.operations.push_back(op);return true;};
    auto scenery=[&](uintptr_t op,const Known& saved){
        uintptr_t root=0;std::array<int32_t,2> type{};std::array<uint8_t,0x6c> pose{};uint32_t height=0;uint8_t automatic=0;
        if(!method(op,0x1706590,0x55a3c0)||!q::at(m,op,0xc0,type,e)||!q::at(m,op,0xc8,root,e)||
           !q::at(m,op,0xd0,pose,e)||!q::at(m,op,0x13c,height,e)||!q::at(m,op,0x140,automatic,e))return false;
        return (saved.row.family==a::DuplicateFamily::Scenery&&type==saved.scenery&&root==saved.root&&pose==saved.transform&&
                height==saved.height&&automatic==saved.automatic)||q::fail(e,"Duplicate history original Scenery full stored type/root/transform/height differs");};
    if(!q::typed(m,image,graph+0x18,0x16e2dc0,e)||!q::bounds(m,graph+0x18,100000,nodes,e)||!nodes.count)
        return q::fail(e,e.empty()?"Duplicate history original graph has no complete nodes":e);
    for(int32_t i=0;i<nodes.count;++i){uintptr_t op=0,ap=0;
        if(!q::at(m,nodes.data,size_t(i)*8,op,e)||!q::at(m,op,0,ap,e))return false;
        out.top_operations.push_back(op);
        if(ap==image+0x1786fb0){Cash cash;
            if(!operations.insert(op).second||!method(op,0x1786fb0,0x53ccf0)||!q::at(m,op,0xb8,cash.amount,e)||!q::at(m,op,0xbc,cash.position,e))
                return q::fail(e,e.empty()?"Duplicate history cash node repeats":e);
            for(auto value:cash.position)if(!std::isfinite(std::bit_cast<float>(value)))return q::fail(e,"Duplicate history cash position nonfinite");
            out.cash.push_back(cash);out.operations.push_back(op);continue;
        }
        if(ap==image+0x1737830||ap==image+0x1713330||ap==image+0x16de8a0){
            if(undo)return q::fail(e,"Duplicate history original Destroy graph has a related Make inverse");
            if(!operations.insert(op).second)return q::fail(e,"Duplicate history related Service/Quest/NPC inverse repeats");
            if(!related(op,ap,e))return q::fail(e,e.empty()?"Duplicate history related Service/Quest/NPC inverse is not bound":e);
            out.operations.push_back(op);continue;
        }
        const Known* saved=nullptr;if(!identity(op,saved,false))return false;out.targets.push_back(saved->row.key);
        if(undo){if(!method(op,0x16de860,0x53e1c0))return false;
            if(saved->row.family==a::DuplicateFamily::Group)for(const auto& key:saved->row.members){
                const auto child=std::find_if(known.begin(),known.end(),[&](const auto& row){return row.second.row.key==key;});uintptr_t current=0;
                if(child==known.end()||child->second.row.parent!=saved->row.key||!targets.insert(child->first).second||
                   !find(child->first,current,e)||current!=child->second.object)return q::fail(e,e.empty()?"Duplicate history original Group live member differs":e);}
            continue;
        }
        if(saved->row.family==a::DuplicateFamily::Scenery){if(!scenery(op,*saved))return false;continue;}
        if(saved->row.family==a::DuplicateFamily::Building){if(!building(op,*saved,e))return false;continue;}
        if(saved->row.family!=a::DuplicateFamily::Group)return q::fail(e,"Duplicate history Building Make/service/Quest contract is not bound");
        uintptr_t root=0,region=0;std::string name;std::array<uint8_t,0x6c> pose{};q::Bounds children,members;
        if(!method(op,0x16fb9d0,0x54e100)||!q::at(m,op,0xc0,root,e)||!q::at(m,op,0xc8,region,e)||
           !text(op+0xd0,name,e)||!q::at(m,op,0xf0,pose,e)||
           !q::typed(m,image,op+0x160,0x16e2dc0,e)||!q::bounds(m,op+0x160,100000,children,e)||
           !q::typed(m,image,op+0x178,0x1749370,e)||!q::bounds(m,op+0x178,100000,members,e))return false;
        if(root!=saved->root||region!=saved->region||name!=saved->name||pose!=saved->transform||
           size_t(children.count)!=saved->row.members.size()||children.count!=members.count)
            return q::fail(e,"Duplicate history original Group full stored owner/name/transform/count differs");
        for(int32_t n=0;n<children.count;++n){uintptr_t child=0;int32_t member=0;const Known* value=nullptr;
            if(!q::at(m,children.data,size_t(n)*8,child,e)||!q::at(m,members.data,size_t(n)*4,member,e)||
               !identity(child,value,true)||!scenery(child,*value))return false;
            if(member<=0||uint64_t(member)!=value->uid||value->row.key!=saved->row.members[size_t(n)]||value->row.parent!=saved->row.key)
                return q::fail(e,"Duplicate history original Group ordered member full UID differs");
        }
        if(!q::unchanged(m,op+0x160,100000,children,e)||!q::unchanged(m,op+0x178,100000,members,e))return false;
    }
    if(targets.size()!=known.size())return q::fail(e,"Duplicate history original graph does not cover complete created set");
    return q::unchanged(m,graph+0x18,100000,nodes,e);
}
template<class Memory,class Find,class Text>
bool read(Memory& m,uintptr_t image,uintptr_t graph,bool undo,const std::map<uint64_t,Known>& known,
          Find&& find,Text&& text,Graph& out,std::string& e){
    auto building=[](uintptr_t,const Known&,std::string& why){return npc_quest_targets::fail(why,"Duplicate history Building Make/service/Quest contract is not bound");};
    auto related=[](uintptr_t,uintptr_t,std::string& why){return npc_quest_targets::fail(why,"Duplicate history related Service/Quest/NPC inverse is not bound");};
    return read(m,image,graph,undo,known,find,text,building,related,out,e);
}
inline bool cash_equal(std::vector<Cash> first,std::vector<Cash> second){
    const auto less=[](const Cash& a,const Cash& b){return std::tie(a.amount,a.position)<std::tie(b.amount,b.position);};
    std::sort(first.begin(),first.end(),less);std::sort(second.begin(),second.end(),less);return first==second;
}
}

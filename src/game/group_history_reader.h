#pragma once
#include "group_history.h"
#include "group_reader.h"
namespace mt2mp::game::group_history {
struct Known {uint64_t uid=0;action_session::GroupRow row;std::string name;};
struct LocalNode {Node value;uintptr_t operation=0;uint64_t group_uid=0;std::string saved_name;};
template<class Memory>bool code(Memory& m,uintptr_t image,std::string& e){
    struct Guard{uintptr_t rva;std::array<uint8_t,16> prefix;};
    const Guard guards[]={{0x546700,{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x58}},
        {0x545830,{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xb8}},
        {0x4ce730,{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x48}},
        {0x4cd3f0,{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x98}}};
    for(const auto& guard:guards){std::array<uint8_t,16> actual{};
        if(!npc_quest_targets::at(m,image,guard.rva,actual,e))return false;
        if(actual!=guard.prefix)return npc_quest_targets::fail(e,"Group history original method entry differs RVA="+std::to_string(guard.rva));}
    return true;
}
// Read the exact original inverse graph. Scenery IDs in both original node
// arrays are signed int32; every one must resolve to its complete local UID.
template<class Memory,class Find,class Region,class Text>
bool read(Memory& m,uintptr_t image,uintptr_t root,uintptr_t graph,const group_reader::Local& current,
          const std::map<uint64_t,Known>& known,Find&& find,Region&& region,Text&& text,
          std::vector<LocalNode>& out,std::string& e){
    namespace q=npc_quest_targets;out.clear();q::Bounds nodes;
    if(!q::typed(m,image,graph+0x18,0x16e2dc0,e)||!q::bounds(m,graph+0x18,100000,nodes,e)||!nodes.count)
        return q::fail(e,e.empty()?"Group history original graph has no complete nodes":e);
    std::set<uintptr_t> unique;std::set<uint64_t> groups;
    for(int32_t i=0;i<nodes.count;++i){LocalNode row;uintptr_t ap=0,actual=0;
        if(!q::at(m,nodes.data,size_t(i)*8,row.operation,e)||!q::at(m,row.operation,0,ap,e)||!q::at(m,row.operation,0xb8,row.group_uid,e))return false;
        const auto remembered=known.find(row.group_uid);
        if(!unique.insert(row.operation).second||!row.group_uid||!groups.insert(row.group_uid).second||remembered==known.end())return q::fail(e,"Group history original target UID is not owned by source action");
        row.value.key=remembered->second.row.key;row.value.region=remembered->second.row.region;row.value.saved_pose=remembered->second.row.pose;
        if(ap==image+0x16ff380)row.value.kind=Kind::Add;else if(ap==image+0x1723370)row.value.kind=Kind::Remove;
        else return q::fail(e,"Group history graph contains foreign original operation class");
        uintptr_t method=0;if(!q::at(m,ap,0x10,method,e))return false;
        if(method!=image+(row.value.kind==Kind::Add?0x546700:0x545830))return q::fail(e,"Group history original node Do vslot differs");
        if(!find(row.group_uid,actual,e))return false;
        const auto live=current.handles.find(row.value.key);
        if(actual){
            if(live==current.handles.end()||live->second!=group_reader::Handle{actual,row.group_uid}||!q::typed(m,image,actual,0x1749de0,e))return q::fail(e,"Group history current complete Group UID differs");
            const auto g=std::find_if(current.state.groups.begin(),current.state.groups.end(),[&](const auto& value){return value.key==row.value.key;});
            if(g==current.state.groups.end()||g->region!=row.value.region)return q::fail(e,"Group history current Group Region differs");
        }else if(row.value.kind==Kind::Remove||live!=current.handles.end())return q::fail(e,"Group history Remove Group absent or Add UID aliases live key");
        if(row.value.kind==Kind::Add){uintptr_t saved_root=0,saved_region=0,expected_region=0;
            if(!q::at(m,row.operation,0xc0,saved_root,e)||!q::at(m,row.operation,0xc8,saved_region,e)||
               !q::at(m,row.operation,0xf0,row.value.saved_pose,e)||!text(row.operation+0xd0,row.saved_name,e)||
               !region(row.value.region,expected_region,e))return false;
            if(saved_root!=root||saved_region!=expected_region||row.saved_name!=remembered->second.name)return q::fail(e,"Group history original saved root/Region/name differs");
            if(!group_transition::finite(row.value.saved_pose))return q::fail(e,"Group history original saved transform nonfinite");
        }
        const size_t offset=row.value.kind==Kind::Add?0x160:0xc0;q::Bounds members;
        if(!q::typed(m,image,row.operation+offset,0x1749370,e)||!q::bounds(m,row.operation+offset,100000,members,e))return false;
        if(!members.count)return q::fail(e,"Group history original node has no members");
        std::set<std::string> member_keys;
        for(int32_t n=0;n<members.count;++n){int32_t stored=0;uintptr_t object=0;
            if(!q::at(m,members.data,size_t(n)*4,stored,e)||stored<=0||!find(uint64_t(stored),object,e))return q::fail(e,e.empty()?"Group history original member int32 UID invalid":e);
            const auto member=std::find_if(current.handles.begin(),current.handles.end(),[&](const auto& entry){return entry.second==group_reader::Handle{object,uint64_t(stored)};});
            if(!object||member==current.handles.end()||member->first.starts_with("group:")||!member_keys.insert(member->first).second)
                return q::fail(e,"Group history original member does not resolve to exact full current Scenery identity");
            row.value.members.push_back(member->first);
        }
        if(!q::unchanged(m,row.operation+offset,100000,members,e))return false;out.push_back(std::move(row));
    }
    return q::unchanged(m,graph+0x18,100000,nodes,e);
}
}

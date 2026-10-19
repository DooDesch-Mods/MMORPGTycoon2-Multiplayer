#pragma once
#include "release_edit_state.h"
#include <set>
#include <vector>
namespace mt2mp::game::release_edit {
struct Local {
    uintptr_t manager=0,release=0,software=0,random=0;
    std::array<uintptr_t,4> data{};
    std::array<int32_t,4> capacity{};
    std::array<std::vector<uintptr_t>,4> members;
    bool operator==(const Local&)const=default;
};
template<class Read>bool members(Read& read,uintptr_t owner,size_t offset,uintptr_t image,uintptr_t expected_ap,
                                uintptr_t& data,int32_t& capacity,std::vector<uintptr_t>& out,std::string& e){
    int32_t count=0;
    if(!read(owner,offset,data,e)||!read(owner,offset+8,count,e)||!read(owner,offset+12,capacity,e))return false;
    if(count<0||count>4096||capacity<count||capacity>65536||(!data&&capacity)||data>UINTPTR_MAX-size_t(capacity)*8){e="Release edit actual collection extent invalid";return false;}
    out.clear();std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){uintptr_t object=0,ap=0;
        if(!read(data,size_t(i)*8,object,e)||!object||!seen.insert(object).second||!read(object,0,ap,e)||ap!=image+expected_ap){if(e.empty())e="Release edit actual collection member class/uniqueness differs";return false;}out.push_back(object);
    }return true;
}
template<class Read,class Text>
bool sample(Read& read,Text& text,uintptr_t image,Local& out,State& state,std::string& e){
    out={};state={};uintptr_t ap=0;uint8_t custom=0;
    if(!read(image,0x13df4f0,out.manager,e)||!read(out.manager,0,ap,e)||ap!=image+0x1714ed0||
       !read(image,0x13df500,out.software,e)||!read(out.software,0,ap,e)||ap!=image+0x1715450||
       !read(out.manager,0x38,out.release,e)||!read(out.release,0,ap,e)||ap!=image+0x16c12b0||
       !read(image,0x164b250,out.random,e)||!read(out.random,0,state.random,e)){if(e.empty())e="Release edit actual singleton/current Release class differs";return false;}
    if(!read(out.software,0x88,state.version,e)||!read(out.release,0x50,state.planned,e)||!read(out.release,0x60,state.type,e)||!read(out.release,0x64,state.state,e)||
       !read(out.manager,0xb8,state.allocated_funds,e)||!read(out.release,0x100,custom,e)||custom>1){if(e.empty())e="Release edit actual version/type/state/custom flag invalid";return false;}
    state.custom=custom;
    for(size_t i=0;i<4;++i)if(!text(out.release+0x80+i*0x20,state.text[i],e))return false;
    const std::array<uintptr_t,4> owner{out.release,out.release,out.manager,out.manager};
    const std::array<size_t,4> offset{0x150,0x180,0x48,0x90};
    const std::array<uintptr_t,4> classes{0x1786d10,0x1786d10,0x1708b20,0x16ddbe0};
    for(size_t i=0;i<4;++i)if(!members(read,owner[i],offset[i],image,classes[i],out.data[i],out.capacity[i],out.members[i],e))return false;
    state.changes=int32_t(out.members[0].size());state.features=int32_t(out.members[1].size());state.demands=int32_t(out.members[2].size());state.history=int32_t(out.members[3].size());
    if(!valid(state)){e="Release edit complete scalar/text state invalid";return false;}e.clear();return true;
}
}

#pragma once
#include "district_reader.h"

namespace mt2mp::game::district_edit_history {
namespace q=district_saved_identity;
struct Stored {
    uint64_t uid=0;
    uintptr_t region=0,root=0;
    int32_t geo=0;
    action_session::TerrainShape shape;
    std::string name,description;
    std::array<uint32_t,4> color{},water_color{},title_color{},height_spread{};
    std::array<uint32_t,3> title_size{},center{};
    uint8_t grass=0;
    std::array<uint8_t,5> options{};
    std::array<uint8_t,4> water_obstruction{};
    bool operator==(const Stored&)const=default;
};
inline Stored stored(uint64_t uid,uintptr_t region,uintptr_t root,const action_session::DistrictState& value){
    Stored out;out.uid=uid;out.region=region;out.root=root;out.geo=value.geo;out.shape=value.shape;
    out.name=value.name;out.description=value.description;out.grass=value.flags[0];
    for(size_t i=0;i<4;++i){out.color[i]=value.values[i];out.water_color[i]=value.values[4+i];out.title_color[i]=value.values[11+i];out.height_spread[i]=value.values[23+i];}
    for(size_t i=0;i<3;++i){out.title_size[i]=value.values[8+i];out.center[i]=value.values[27+i];}
    out.options={value.flags[2],value.flags[3],value.flags[4],value.flags[5],value.flags[7]};
    out.water_obstruction={value.flags[6],value.flags[8],value.flags[9],value.flags[10]};return out;
}
template<class Memory>
bool read_node(Memory& memory,uintptr_t image,uintptr_t node,Stored& out,std::string& error,uintptr_t expected_ap=0x17132b0){
    if(expected_ap!=0x17132b0&&expected_ap!=0x17132f0)return q::fail(error,"District inverse record kind is not Edit or Make");
    Stored value;
    if(!q::typed(memory,image,node,expected_ap,error)||!q::at(memory,node,0xb8,value.uid,error)||
       !q::at(memory,node,0xc0,value.region,error)||!q::at(memory,node,0xc8,value.root,error)||
       !q::at(memory,node,0xd0,value.geo,error)||!q::at(memory,node,0x148,value.color,error)||
       !q::at(memory,node,0x158,value.grass,error)||!district_reader::text(memory,node,0x160,value.name,error)||
       !district_reader::text(memory,node,0x180,value.description,error)||!q::at(memory,node,0x1a0,value.title_size,error)||
       !q::at(memory,node,0x1ac,value.title_color,error)||!q::at(memory,node,0x1bc,value.options,error)||
       !q::at(memory,node,0x1c4,value.height_spread,error)||!q::at(memory,node,0x1d4,value.center,error)||
       !q::at(memory,node,0x1e0,value.water_color,error)||!q::at(memory,node,0x1f0,value.water_obstruction,error))return false;
    struct Copy {Memory& source;bool copy(uintptr_t p,void* result,size_t bytes,std::string& cause){return source.read(p,result,bytes,cause);}} copy{memory};
    if(!ui_terrain_runtime::poly::read(copy,node+0xd8,value.shape,error))return false;
    if(!value.uid||!value.root||!value.region||value.geo<0||value.geo>2||value.grass>1)
        return q::fail(error,"District original Edit record full UID/owner/geometry/grass invalid");
    for(auto flag:value.options)if(flag>1)return q::fail(error,"District original Edit record option is not Boolean");
    for(auto flag:value.water_obstruction)if(flag>1)return q::fail(error,"District original Edit record water/obstruction is not Boolean");
    const auto finite=[](const auto& values){for(auto bits:values)if(!std::isfinite(std::bit_cast<float>(bits)))return false;return true;};
    if(!finite(value.color)||!finite(value.water_color)||!finite(value.title_color)||!finite(value.height_spread)||!finite(value.title_size)||!finite(value.center))
        return q::fail(error,"District original Edit record contains non-finite declared values");
    out=std::move(value);return true;
}
template<class Memory>
bool graph(Memory& memory,uintptr_t image,uintptr_t object,Stored& out,std::string& error){
    auto once=[&](Stored& value){
        q::Bounds children,edit_nodes,deleting;uint8_t locked=0;uintptr_t node=0;
        if(!q::typed(memory,image,object,0x16e32d0,error)||!q::bounds(memory,object,0,children,error)||
           !q::typed(memory,image,object+0x18,0x16e2dc0,error)||!q::bounds(memory,object+0x18,1,edit_nodes,error)||
           !q::typed(memory,image,object+0x48,0x1748c50,error)||!q::bounds(memory,object+0x48,0,deleting,error)||
           !q::at(memory,object,0x10c,locked,error))return false;
        if(edit_nodes.count!=1||locked!=1)return q::fail(error,"District property inverse must be one locked original Edit record");
        if(!q::at(memory,edit_nodes.data,0,node,error)||!read_node(memory,image,node,value,error)||
           !q::unchanged(memory,object,0,children,error)||!q::unchanged(memory,object+0x18,1,edit_nodes,error)||
           !q::unchanged(memory,object+0x48,0,deleting,error))return false;
        return true;
    };
    Stored before,after;if(!once(before)||!once(after))return false;
    if(before!=after)return q::fail(error,"District original inverse payload changed during complete double read");
    out=std::move(after);return true;
}
inline action_session::DistrictState project(const action_session::DistrictState& current,const Stored& source){
    auto out=current;out.geo=source.geo;out.shape=source.shape;out.name=source.name;out.description=source.description;out.flags[0]=source.grass;
    for(size_t i=0;i<4;++i){out.values[i]=source.color[i];out.values[11+i]=source.title_color[i];out.values[23+i]=source.height_spread[i];
        if(source.water_obstruction[0])out.values[4+i]=source.water_color[i];}
    for(size_t i=0;i<3;++i){out.values[8+i]=source.title_size[i];out.values[27+i]=source.center[i];}
    out.flags[2]=source.options[0];out.flags[3]=source.options[1];out.flags[4]=source.options[2];out.flags[5]=source.options[3];out.flags[7]=source.options[4];
    out.flags[6]=source.water_obstruction[0];out.flags[8]=source.water_obstruction[1];out.flags[9]=source.water_obstruction[2];out.flags[10]=source.water_obstruction[3];
    return out;
}
}

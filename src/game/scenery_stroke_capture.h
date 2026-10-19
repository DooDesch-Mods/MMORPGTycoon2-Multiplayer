#pragma once
#include "typed_action/scenery_stroke_action.h"
#include "native_polyshape.h"
#include "npc_quest_targets.h"
#include <bit>
#include <cmath>
#include <exception>

namespace mt2mp::game::scenery_stroke_capture {
namespace q=npc_quest_targets;
namespace a=action_session;
namespace poly=ui_terrain_runtime::poly;
struct Local {
    uintptr_t receiver=0,region=0,root=0,pattern=0,options=0;
    int32_t category=-1,variant=-1;
    a::SceneryStrokeKind kind=a::SceneryStrokeKind::Paint;
    int32_t mode=0;bool replace=false;
    bool operator==(const Local&)const=default;
};
template<class Memory>bool bulk(Memory& memory,uintptr_t image,uintptr_t receiver,bool eraser,bool& owned,std::string& e){
    owned=false;
    if(eraser){if(!q::typed(memory,image,receiver,0x173fc60,e))return false;owned=true;return true;}
    struct Mode {uintptr_t options=0,data=0;size_t size=0;int32_t current=0;std::array<char,6> name{};bool operator==(const Mode&)const=default;};
    auto read=[&](Mode& value){
        if(!q::typed(memory,image,receiver,0x17396b0,e)||!q::at(memory,receiver,0x174,value.current,e)||value.current<0||value.current>2||
           !q::at(memory,receiver,0xb68,value.options,e)||!q::typed(memory,image,value.options,0x1715390,e)||
           !q::at(memory,value.options,0x88,value.data,e)||!q::at(memory,value.options,0x90,value.size,e)||
           (value.size!=5&&value.size!=6)||!value.data||!memory.read(value.data,value.name.data(),value.size,e))
            return q::fail(e,e.empty()?"Scenery stroke current/desired native mode differs":e);
        const bool single=value.size==6&&value.name==std::array<char,6>{'s','i','n','g','l','e'};
        const bool paint=value.size==5&&value.name==std::array<char,6>{'p','a','i','n','t',0};
        const bool erase=value.size==5&&value.name==std::array<char,6>{'e','r','a','s','e',0};
        return single||paint||erase||q::fail(e,"Scenery stroke desired native mode is unknown");
    };
    Mode first,last;if(!read(first)||!read(last))return false;
    if(first!=last)return q::fail(e,"Scenery stroke native mode changed during capture");
    owned=first.current!=0||first.size==5;return true;
}
inline bool pose(const a::SceneryStrokePose& value){
    for(auto bits:value)if(!std::isfinite(std::bit_cast<float>(bits)))return false;
    for(size_t index=7;index<10;++index)if(std::bit_cast<float>(value[index])<=0)return false;
    double norm=0;for(size_t index=0;index<4;++index){const auto part=std::bit_cast<float>(value[index]);norm+=double(part)*part;}
    return std::abs(norm-1.0)<=0.01;
}
template<class Memory,class Catalog,class Resolve>
bool read(Memory& memory,uintptr_t image,uintptr_t receiver,a::SceneryStrokeKind kind,Catalog&& catalog,Resolve&& resolve,
          Local& out,a::SceneryStrokeIntent& intent,std::string& e){
    out={};intent={};out.receiver=receiver;out.kind=kind;intent.kind=kind;uintptr_t graph=0;
    if(kind!=a::SceneryStrokeKind::Paint&&kind!=a::SceneryStrokeKind::Erase)return q::fail(e,"Scenery stroke kind is invalid");
    const bool paint=kind==a::SceneryStrokeKind::Paint;const auto region_offset=paint?size_t(0xb60):size_t(0x588);
    if(!q::typed(memory,image,receiver,paint?uintptr_t(0x17396b0):uintptr_t(0x173fc60),e)||
       !q::at(memory,receiver,paint?size_t(0xc50):size_t(0x5c0),graph,e)||graph||
       !q::at(memory,receiver,region_offset,out.region,e)||!out.region||
       !q::typed(memory,image,out.region,0x1787930,e)||!q::at(memory,out.region,0x70,out.root,e)||!out.root||
       !q::at(memory,out.region,0x834,intent.region,e)||intent.region<0||!resolve(intent.region,out.region,out.root,e))
        return q::fail(e,e.empty()?"Scenery stroke original receiver/idle graph/current Region differs":e);
    if(!paint){out.mode=1;intent.mode=1;return true;}
    uint8_t replace=0;
    if(!q::at(memory,receiver,0x174,out.mode,e)||(out.mode!=1&&out.mode!=2)||
       !q::at(memory,receiver,0x178,out.category,e)||!q::at(memory,receiver,0x17c,out.variant,e)||
       !q::at(memory,receiver,0xb68,out.options,e)||!q::typed(memory,image,out.options,0x1715390,e)||
       !q::at(memory,out.options,0x60,replace,e)||replace>1)
        return q::fail(e,e.empty()?"Scenery stroke original mode/options differ":e);
    out.replace=replace!=0;intent.mode=out.mode;intent.replace=out.replace;
    if(!catalog(out.category,out.variant,intent.type,intent.variant,e))return false;
    intent.random_variant=out.variant==-1;
    if(intent.type.empty()||(intent.random_variant?!intent.variant.empty():intent.variant.empty()))
        return q::fail(e,"Scenery stroke catalog identity/random selection differs");
    if(out.mode==2){uintptr_t ap=0,method=0;
        if(!q::at(memory,receiver,0xbc0,out.pattern,e)||!q::at(memory,out.pattern,0,ap,e)||!q::at(memory,ap,0x10,method,e))return false;
        const bool grid=ap==image+0x16fd310&&method==image+0x18db60;
        const bool hex=ap==image+0x1714af0&&method==image+0x18e3f0;
        const bool burst=ap==image+0x1701990&&method==image+0x18ecf0;
        if(!grid&&!hex&&!burst)return q::fail(e,"Scenery stroke generator is not an original Grid/Hexagon/Burst pattern");
    }
    return true;
}
template<class Memory>
bool dots(Memory& memory,uintptr_t image,uintptr_t array,size_t budget,std::vector<a::SceneryStrokePose>& out,std::string& e){
    out.clear();q::Bounds span;
    if(budget>1000000)return q::fail(e,"Scenery stroke generated transform budget exceeds resource bound");
    if(!q::typed(memory,image,array,0x1746400,e)||!q::bounds(memory,array,budget,span,e))return false;
    for(int32_t index=0;index<span.count;++index){a::SceneryStrokePose value;
        if(!q::at(memory,span.data,size_t(index)*0x6c,value,e)||!pose(value))
            return q::fail(e,e.empty()?"Scenery stroke native generated transform is invalid":e);
        out.push_back(value);
    }
    return q::unchanged(memory,array,budget,span,e);
}
template<class Port>
bool generated(Port& port,const Local& owner,uintptr_t shape,size_t budget,std::vector<a::SceneryStrokePose>& out,std::string& e){
    out.clear();if(owner.kind!=a::SceneryStrokeKind::Paint||owner.mode!=2)return true;
    uintptr_t array=0;bool constructed=false;std::string cause;
    bool okay=false;
    try{okay=port.generate(owner.pattern,shape,array,constructed,cause);
        if(okay)okay=port.read(array,budget,out,cause);}
    catch(const std::exception& ex){okay=false;cause=std::string("Scenery stroke generated transform read failed: ")+ex.what();}
    if(constructed){std::string cleanup;bool destroyed=false;
        try{destroyed=port.destroy(array,cleanup);}catch(const std::exception& ex){cleanup=std::string("Scenery stroke generated transform cleanup failed: ")+ex.what();}
        if(!destroyed){
        if(cleanup.empty())cleanup="Scenery stroke generated transform cleanup failed without cause";
        if(okay){cause=cleanup;okay=false;}else cause+="; generated transform cleanup: "+cleanup;
    }}
    if(!okay){out.clear();return q::fail(e,cause.empty()?"Scenery stroke original generator failed without cause":cause);}
    return true;
}
template<class Memory>
bool finish(Memory& memory,const Local& owner,a::SceneryStrokeIntent& intent,std::string& e){
    if(owner.kind==a::SceneryStrokeKind::Paint){
        if(!q::at(memory,owner.receiver,0x894,intent.bounds,e))return false;
        for(auto bits:intent.bounds)if(!std::isfinite(std::bit_cast<float>(bits)))return q::fail(e,"Scenery stroke complete paint bounds are nonfinite");
    }
    return true;
}
}

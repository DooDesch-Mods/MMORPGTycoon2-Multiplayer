#pragma once
#include "typed_action/costume_edit_action.h"
#include <cmath>
#include <set>
namespace mt2mp::game::costume_record_reader {
template<class Read>bool pose(Read& read,uintptr_t object,size_t offset,action_session::CostumePose& out,std::string& error){
    if(!read(object,offset,out.rotation,error)||!read(object,offset+0x10,out.position,error)||!read(object,offset+0x1c,out.scale,error))return false;
    for(float x:out.rotation)if(!std::isfinite(x)){error="Costume rotation is non-finite";return false;}
    for(float x:out.position)if(!std::isfinite(x)){error="Costume position is non-finite";return false;}
    for(float x:out.scale)if(!std::isfinite(x)){error="Costume scale is non-finite";return false;}return true;
}
template<class Read>bool bounds(Read& read,uintptr_t object,size_t offset,uintptr_t& data,int32_t& count,int32_t& capacity,std::string& error){
    if(!read(object,offset,data,error)||!read(object,offset+8,count,error)||!read(object,offset+12,capacity,error))return false;
    if(count<0||count>128||capacity<count||(count&&!data)){error="Costume native array bounds differ";return false;}return true;
}
template<class Read,class Text>bool record(Read& read,Text& text,uintptr_t image,uintptr_t object,action_session::CostumeRecord& out,std::string& error){
    out={};uintptr_t ap=0;uint8_t flag=2;uintptr_t data=0;int32_t count=0,capacity=0;
    if(!read(object,0,ap,error)||ap!=image+0x16c0210){error="Costume address point differs";return false;}
    if(!text(object+0x18,out.name,error)||!text(object+0x38,out.skeleton,error)||!read(object,0xc0,flag,error))return false;
    if(out.name.size()>4096||out.skeleton.size()>4096||flag>1){error="Costume native text/flag exceeds bound";return false;}out.flag=flag!=0;
    if(!bounds(read,object,0x60,data,count,capacity,error))return false;std::set<uintptr_t> pointers;
    for(int32_t i=0;i<count;++i){uintptr_t part=0;action_session::CostumePart value;
        if(!read(data,size_t(i)*8,part,error)||!part||!pointers.insert(part).second||!read(part,0,ap,error)||ap!=image+0x1736c70){error="Costume owned part address point/uniqueness differs";return false;}
        if(!text(part+0x18,value.bone,error)||!text(part+0x38,value.model,error)||!pose(read,part,0x58,value.pose,error)||!read(part,0xc4,value.model_scale,error))return false;
        if(value.bone.size()>4096||value.model.size()>4096||!std::isfinite(value.model_scale)){error="Costume part text/scale exceeds bound";return false;}
        uintptr_t attachments=0,colors=0;int32_t attached=0,attached_cap=0,colored=0,color_cap=0;
        if(!bounds(read,part,0xd0,attachments,attached,attached_cap,error)||!bounds(read,part,0x100,colors,colored,color_cap,error))return false;
        for(int32_t n=0;n<attached;++n){uintptr_t attachment=0;action_session::CostumeAttachment row;
            if(!read(attachments,size_t(n)*8,attachment,error)||!attachment||!pointers.insert(attachment).second||!read(attachment,0,ap,error)||ap!=image+0x17229b0){error="Costume owned attachment address point/uniqueness differs";return false;}
            if(!text(attachment+0x18,row.name,error)||row.name.size()>4096||!pose(read,attachment,0x38,row.pose,error))return false;value.attachments.push_back(std::move(row));}
        for(int32_t n=0;n<colored;++n){std::array<float,4> row;if(!read(colors,size_t(n)*16,row,error))return false;for(float x:row)if(!std::isfinite(x)){error="Costume color is non-finite";return false;}value.colors.push_back(row);}
        uintptr_t again=0;int32_t size=0,cap=0;
        if(!bounds(read,part,0xd0,again,size,cap,error)||again!=attachments||size!=attached||cap!=attached_cap||!bounds(read,part,0x100,again,size,cap,error)||again!=colors||size!=colored||cap!=color_cap){error="Costume owned part arrays changed during read";return false;}
        out.parts.push_back(std::move(value));}
    uintptr_t again=0;int32_t size=0,cap=0;
    if(!bounds(read,object,0x60,again,size,cap,error)||again!=data||size!=count||cap!=capacity){error="Costume owned parts changed during read";return false;}return true;
}
}

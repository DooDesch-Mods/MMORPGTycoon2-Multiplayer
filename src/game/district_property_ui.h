#pragma once
#include "district_property.h"
#include "district_reader.h"

namespace mt2mp::game::district_property_ui {
namespace property=district_property;
namespace q=district_saved_identity;
enum class Widget:uint8_t {Spinner,Checkbox,Color};
struct Binding {
    uintptr_t widget=0,value=0,object=0,scalar=0;
    uint64_t uid=0;
    property::Kind kind=property::Kind::Height;
    uint8_t editing=0;
    bool operator==(const Binding&)const=default;
};
template<class Memory>
bool bind(Memory& memory,uintptr_t image,uintptr_t widget,Widget type,Binding& out,bool& ours,std::string& error){
    ours=false;out={};uintptr_t value=0,object=0,ap=0;
    if(type==Widget::Color)value=widget+0x338;
    else if(!q::at(memory,widget,type==Widget::Spinner?0x428:0x3c8,value,error))return false;
    if(!value)return true;
    if(!q::at(memory,value,8,object,error))return false;
    if(!object)return true;
    if(!q::at(memory,object,0,ap,error))return false;
    if(ap!=image+0x16da000)return true;
    ours=true;
    const uintptr_t widget_ap=type==Widget::Spinner?0x16c1720:type==Widget::Checkbox?0x16d98c0:0x17224a0;
    const uintptr_t value_ap=type==Widget::Spinner?0x1749660:type==Widget::Checkbox?0x1749610:0x1749520;
    if(!q::typed(memory,image,widget,widget_ap,error)||!q::typed(memory,image,value,value_ap,error))return false;
    std::string name;uintptr_t scalar=0;uint64_t uid=0;uint8_t editing=0;
    if(!district_reader::text(memory,value,0x18,name,error)||!q::at(memory,value,0x38,scalar,error)||
       !q::at(memory,value,0x10,editing,error)||!q::at(memory,object,0x18,uid,error))return false;
    const auto* field=property::field(name);
    const auto field_type=type==Widget::Spinner?property::Type::Number:type==Widget::Checkbox?property::Type::Flag:property::Type::Color;
    if(!field||field->type!=field_type||scalar!=object+field->offset||!uid||editing>1)
        return q::fail(error,"District native widget property/type/bound address/full UID/edit state differs");
    const uintptr_t property_ap=field_type==property::Type::Number?0x16d3050:field_type==property::Type::Flag?0x16cfb60:0x16c69b0;
    uintptr_t offset=0;
    if(!q::typed(memory,image,image+field->property,property_ap,error)||!q::at(memory,image+field->property,0x38,offset,error))return false;
    if(offset!=field->offset)return q::fail(error,"District original property descriptor offset differs");
    if(type==Widget::Spinner){uintptr_t integer=0;
        if(!q::at(memory,widget,0x430,integer,error))return false;
        if(integer)return q::fail(error,"District float spinner also has an integer binding");
    }
    uintptr_t secondary=0;
    if(!q::at(memory,widget,type==Widget::Spinner?0x380:type==Widget::Checkbox?0x3c0:0x2e8,secondary,error))return false;
    if(secondary)return q::fail(error,"District property widget has an additional command or raw binding");
    out={widget,value,object,scalar,uid,field->kind,editing};return true;
}
template<class Memory>
bool spinner(Memory& memory,const Binding& binding,int32_t requested,property::Value& out,std::string& error){
    const auto* field=property::field(binding.kind);
    int32_t minimum=0,maximum=0;float scale=0;
    if(!field||field->type!=property::Type::Number)return q::fail(error,"District spinner field is not a numeric property");
    if(!q::at(memory,binding.widget,0x43c,minimum,error)||!q::at(memory,binding.widget,0x440,maximum,error)||
       !q::at(memory,binding.widget,0x438,scale,error))return false;
    if(!std::isfinite(scale)||scale<=0||minimum>maximum||float(minimum)/scale!=field->minimum||float(maximum)/scale!=field->maximum)
        return q::fail(error,"District original spinner scale/range differs from the active UI property");
    const float target=float(std::clamp(requested,minimum,maximum))/scale;
    out={binding.kind,{std::bit_cast<uint32_t>(target),0,0,0}};
    return property::valid(out)||q::fail(error,"District original spinner result invalid");
}
template<class Memory>
bool unchanged(Memory& memory,uintptr_t image,const Binding& before,Widget type,std::string& error){
    Binding after;bool ours=false;
    return bind(memory,image,before.widget,type,after,ours,error)&&
        ((ours&&after==before)||q::fail(error,"District widget owner/property lifetime changed during capture"));
}
}

#pragma once
#include "typed_action/district_action.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace mt2mp::game::district_property {
enum class Kind:uint8_t {
    Height=1,Spread,WaterHeight,WaterSpread,TitleHeight,TitleFontSize,
    Color,WaterColor,TitleColor,TitleColorTop,TitleColorOutline,
    ChangeLand,ChangeColor,ChangeWater,ChangeWaterColor,LockLandAndWater,
    AutoObstruct,ObstructTop,ObstructSides,WithGrass,ShowTitle,PrioritiseWater
};
enum class Type:uint8_t {Number,Color,Flag};
struct Field {
    Kind kind;Type type;std::string_view name;
    uintptr_t property,offset;
    size_t index;
    float minimum,maximum;
    bool original_history;
};
inline constexpr std::array<Field,22> fields{{
    {Kind::Height,Type::Number,"height",0x13cc480,0x328,23,-100,100,true},
    {Kind::Spread,Type::Number,"spread",0x13cc440,0x32c,24,0,200,true},
    {Kind::WaterHeight,Type::Number,"waterHeight",0x13cc400,0x330,25,-100,100,true},
    {Kind::WaterSpread,Type::Number,"waterSpread",0x13cc3c0,0x334,26,0,200,true},
    {Kind::TitleHeight,Type::Number,"titleHeight",0x13cc340,0x2e0,8,10,300,false},
    {Kind::TitleFontSize,Type::Number,"titleFontSize",0x13cc300,0x2e4,9,0,64,false},
    {Kind::Color,Type::Color,"color",0x13cc840,0x290,0,0,1,true},
    {Kind::WaterColor,Type::Color,"waterColor",0x13cc800,0x2a0,4,0,1,false},
    {Kind::TitleColor,Type::Color,"titleColor",0x13cc280,0x2ec,11,0,1,false},
    {Kind::TitleColorTop,Type::Color,"titleColorTop",0x13cc240,0x2fc,15,0,1,false},
    {Kind::TitleColorOutline,Type::Color,"titleColorOutline",0x13cc200,0x30c,19,0,1,false},
    {Kind::ChangeLand,Type::Flag,"changeLand",0x13cc680,0x31d,3,0,1,false},
    {Kind::ChangeColor,Type::Flag,"changeColor",0x13cc640,0x31e,4,0,1,false},
    {Kind::ChangeWater,Type::Flag,"changeWater",0x13cc600,0x31f,5,0,1,false},
    {Kind::ChangeWaterColor,Type::Flag,"changeWaterColor",0x13cc5c0,0x320,6,0,1,false},
    {Kind::LockLandAndWater,Type::Flag,"lockLandAndWater",0x13cc580,0x321,7,0,1,false},
    {Kind::AutoObstruct,Type::Flag,"autoObstruct",0x13cc540,0x322,8,0,1,false},
    {Kind::ObstructTop,Type::Flag,"obstructTop",0x13cc500,0x323,9,0,1,false},
    {Kind::ObstructSides,Type::Flag,"obstructSides",0x13cc4c0,0x324,10,0,1,false},
    {Kind::WithGrass,Type::Flag,"withGrass",0x13cc7c0,0x2b0,0,0,1,false},
    {Kind::ShowTitle,Type::Flag,"showTitle",0x13cc780,0x2b1,1,0,1,false},
    {Kind::PrioritiseWater,Type::Flag,"prioritiseWater",0x13cc140,0x351,12,0,1,false}
}};
inline const Field* field(Kind kind){
    const auto number=uint8_t(kind);
    return number&&number<=fields.size()?&fields[number-1]:nullptr;
}
inline const Field* field(std::string_view name){
    for(const auto& row:fields)if(row.name==name)return &row;
    return nullptr;
}
inline bool title(const Field& value){return value.name.starts_with("title")||value.kind==Kind::ShowTitle;}
struct Value {
    Kind kind=Kind::Height;
    std::array<uint32_t,4> bits{};
    bool operator==(const Value&)const=default;
};
inline bool valid(const Value& value){
    const auto* row=field(value.kind);if(!row)return false;
    if(row->type==Type::Flag)return value.bits[0]<=1&&!value.bits[1]&&!value.bits[2]&&!value.bits[3];
    const size_t count=row->type==Type::Color?4:1;
    for(size_t i=0;i<count;++i){const float current=std::bit_cast<float>(value.bits[i]);
        if(!std::isfinite(current)||current<row->minimum||current>row->maximum)return false;}
    for(size_t i=count;i<4;++i)if(value.bits[i])return false;
    return true;
}
inline Value current(Kind kind,const action_session::DistrictState& state){
    Value value{kind,{}};const auto* row=field(kind);if(!row)return value;
    if(row->type==Type::Flag)value.bits[0]=state.flags[row->index];
    else for(size_t i=0;i<(row->type==Type::Color?4u:1u);++i)value.bits[i]=state.values[row->index+i];
    return value;
}
inline bool changed(const Value& value,const action_session::DistrictState& state){return valid(value)&&current(value.kind,state)!=value;}
inline bool expected(const action_session::DistrictState& before,const Value& value,uint32_t continent,
                     action_session::DistrictState& out,std::string& error){
    const auto* row=field(value.kind);const float height=std::bit_cast<float>(continent);
    if(!row||!changed(value,before)||!std::isfinite(height)||before.geo<0||before.geo>2){
        error="District property/current value/continent/geometry invalid or unchanged";return false;}
    out=before;
    if(row->type==Type::Flag)out.flags[row->index]=uint8_t(value.bits[0]);
    else for(size_t i=0;i<(row->type==Type::Color?4u:1u);++i)out.values[row->index+i]=value.bits[i];
    if(!title(*row)){
        for(const size_t index:{size_t(23),size_t(25)}){
            const float requested=std::bit_cast<float>(out.values[index]);
            if(requested+height<0)out.values[index]=std::bit_cast<uint32_t>(-height);
        }
        if(out.flags[7]){out.values[25]=out.values[23];out.values[26]=out.values[24];}
        if(out.flags[8]){
            out.flags[9]=uint8_t(out.geo==2&&out.flags[5]);
            out.flags[10]=uint8_t(out.geo!=0&&std::abs(std::bit_cast<float>(out.values[23]))>
                std::abs(std::bit_cast<float>(out.values[24])));
        }
    }
    return true;
}
inline bool effect(const action_session::DistrictState& before,const Value& value,uint32_t continent,
                   const action_session::DistrictState& after,std::string& error){
    action_session::DistrictState required;
    if(!expected(before,value,continent,required,error))return false;
    if(required==after)return true;
    // Original title regeneration may retire an invalid custom title position.
    if(!required.manual_title_inside&&required.flags[11]){
        required.flags[11]=0;required.manual_title_inside=true;
        if(required==after)return true;
    }
    error="District original property changed unrelated authored fields or its derived result differs";return false;
}
}

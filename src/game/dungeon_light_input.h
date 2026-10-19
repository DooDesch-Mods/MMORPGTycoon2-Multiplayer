#pragma once
#include "typed_action/dungeon_light_action.h"
#include <cmath>
#include <set>
namespace mt2mp::game::dungeon_light_input {
inline constexpr std::array<size_t,4> spinner_offsets{0x508,0x510,0x500,0x518};
inline constexpr std::array<size_t,3> slider_offsets{0x528,0x538,0x540};
inline constexpr std::array<size_t,4> color_offsets{0x4f8,0x4f0,0x530,0x548};
struct Widgets {std::array<uintptr_t,4> spinners{},colors{};std::array<uintptr_t,3> sliders{};bool operator==(const Widgets&)const=default;};
template<class Read>bool read(Read& at,uintptr_t image,uintptr_t window,action_session::DungeonLightInput& out,Widgets& widgets,std::string& error){
    out={};widgets={};std::set<uintptr_t> pointers;
    for(size_t group=0;group<3;++group){const size_t size=group==1?3:4;for(size_t i=0;i<size;++i){uintptr_t object=0,ap=0,binding=0;const auto offset=group==0?spinner_offsets[i]:group==1?slider_offsets[i]:color_offsets[i];
        if(!at(window,offset,object,error))return false;
        if(!object||!pointers.insert(object).second){error="Dungeon light actual native input widget pointer/uniqueness differs";return false;}
        if(!at(object,0,ap,error))return false;
        if(ap!=image+(group==0?0x16c1720:group==1?0x1787a30:0x17224a0)){error="Dungeon light actual native input widget address point differs";return false;}
        if(group==2){if(!at(object,0x370,binding,error))return false;if(binding){error="Dungeon light ColorSwatchPicker has a gameplay value binding";return false;}widgets.colors[i]=object;
            if(!at(object,0x2c8,out.colors[i],error))return false;for(float x:out.colors[i])if(!std::isfinite(x)){error="Dungeon light native input color is non-finite";return false;}}
        else {const auto b0=group==0?0x428:0x420,b1=group==0?0x430:0x428;
            if(!at(object,b0,binding,error))return false;if(binding){error="Dungeon light input float gameplay value binding is present";return false;}
            if(!at(object,b1,binding,error))return false;if(binding){error="Dungeon light input integer gameplay value binding is present";return false;}
            if(group==0){widgets.spinners[i]=object;if(!at(object,0x44c,out.spinners[i],error))return false;}
            else {widgets.sliders[i]=object;if(!at(object,0x4d0,out.sliders[i],error))return false;}
        }
    }}return true;
}
template<class Convert,class Direction>bool calculate(const action_session::DungeonLightInput& input,const Widgets& widgets,Convert& convert,Direction& direction,std::array<float,23>& out,std::string& error){
    out={};std::array<float,4> spinners;std::array<float,3> sliders;
    for(size_t i=0;i<4;++i)if(!convert(false,widgets.spinners[i],input.spinners[i],spinners[i],error))return false;
    for(size_t i=0;i<3;++i)if(!convert(true,widgets.sliders[i],input.sliders[i],sliders[i],error))return false;
    for(size_t i=0;i<4;++i){out[i]=input.colors[0][i]*spinners[0];out[7+i]=input.colors[1][i]*spinners[2];out[11+i]=input.colors[2][i];out[16+i]=input.colors[3][i];}
    std::array<float,3> dir;if(!direction(spinners[1],dir,error))return false;for(size_t i=0;i<3;++i)out[4+i]=dir[i];
    out[15]=sliders[0]*0.0010000000474974513f;out[20]=sliders[1]*0.009999999776482582f;out[21]=sliders[2];out[22]=spinners[3];
    for(float x:out)if(!std::isfinite(x)){error="Dungeon light original numeric result is non-finite";return false;}return true;
}
}

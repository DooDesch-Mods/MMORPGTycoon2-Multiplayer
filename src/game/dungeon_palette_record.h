#pragma once
#include "typed_action/dungeon_palette_action.h"
#include <cmath>
#include <set>
namespace mt2mp::game::dungeon_palette_record {
struct Input {uintptr_t checkbox=0,theme=0;std::array<uintptr_t,4> swatches{};bool defaults=false,reset_checkbox=false,refresh_swatches=false;std::array<std::array<float,4>,4> colors{};};
// This is the registered native CursorBehaviour, with a secondary vtable at +18 rather than a UID.
template<class Read>bool input(Read& at,uintptr_t image,uintptr_t cursor,action_session::DungeonPaletteEdit edit,Input& out,std::string& error){
    out={};uintptr_t ap=0,secondary=0,bound=0,editing=0;uint8_t checked=2;
    if(!at(cursor,0,ap,error)||!at(cursor,0x18,secondary,error)||!at(cursor,0x308,out.checkbox,error)||!at(cursor,0x2c8,out.theme,error)||!at(cursor,0x348,editing,error))return false;
    if(ap!=image+0x173ea20||secondary!=image+0x173eba8||!out.checkbox){error="Dungeon palette registered Tile receiver/class/Checkbox differs";return false;}
    if(editing){error="Dungeon palette cannot overlap the native Tile edit transaction";return false;}
    if(!at(out.checkbox,0,ap,error)||!at(out.checkbox,0x346,checked,error))return false;
    if(ap!=image+0x16d98c0||checked>1){error="Dungeon palette native Checkbox class/value differs";return false;}
    for(size_t offset:{size_t(0x3c0),size_t(0x3c8)}){if(!at(out.checkbox,offset,bound,error))return false;if(bound){error="Dungeon palette Checkbox has a gameplay value binding";return false;}}
    out.defaults=edit==action_session::DungeonPaletteEdit::Options&&checked!=0;out.reset_checkbox=edit==action_session::DungeonPaletteEdit::Colors;
    std::set<uintptr_t> unique{out.checkbox};
    for(size_t i=0;i<4;++i){auto& swatch=out.swatches[i];
        if(!at(cursor,0x2e8+i*8,swatch,error))return false;
        if(!swatch||!unique.insert(swatch).second){error="Dungeon palette four native Swatches are absent or alias";return false;}
        if(!at(swatch,0,ap,error)||!at(swatch,0x370,bound,error)||!at(swatch,0x2c8,out.colors[i],error))return false;
        if(ap!=image+0x17224a0||bound){error="Dungeon palette native Swatch class/gameplay binding differs";return false;}
    }
    if(out.defaults&&out.theme){uintptr_t colors=0;int32_t count=0,capacity=0;
        if(!at(out.theme,0,ap,error)||!at(out.theme,0x80,colors,error)||!at(out.theme,0x88,count,error)||!at(out.theme,0x8c,capacity,error))return false;
        if(ap!=image+0x16ff4d0||count<0||capacity<count||(count&&(!colors||count<4||colors>UINTPTR_MAX-size_t(count)*16))){error="Dungeon palette native Theme color array cannot supply four normal channels";return false;}
        if(count){out.refresh_swatches=true;for(size_t i=0;i<4;++i)if(!at(colors,i*16,out.colors[i],error))return false;}
    }
    for(const auto& color:out.colors)for(float x:color)if(!std::isfinite(x)){error="Dungeon palette native effective RGBA is non-finite";return false;}
    return true;
}
struct Receipt {bool attempted=false,returned=false;uint32_t flag_calls=0,color_calls=0;};
// The original outer helper uses current Shard. These original leaves take the explicit, already bound Dungeon slots.
template<class Read,class Flag,class Item,class Color>bool apply(Read& at,Flag& flag,Item& item,Color& color,uintptr_t image,uintptr_t dungeon,uintptr_t palette_data,int32_t count,const action_session::DungeonPaletteIntent& intent,Receipt& receipt,std::string& error){
    receipt={};uintptr_t ap=0,data=0;int32_t actual_count=0,capacity=0;
    if(!at(dungeon,0x1a0,ap,error)||!at(dungeon,0x1c0,data,error)||!at(dungeon,0x1c8,actual_count,error)||!at(dungeon,0x1cc,capacity,error))return false;
    if(ap!=image+0x172bf80||data!=palette_data||actual_count!=count||count<4||capacity<count||!data||data>UINTPTR_MAX-size_t(count)*4){error="Dungeon palette explicit ColorConfig/vector owner differs";return false;}
    std::array<uintptr_t,4> slots{};
    for(size_t i=0;i<4;++i){if(!item(dungeon+0x1b8,int32_t(i),slots[i],error))return false;if(slots[i]!=data+i*4){error="Dungeon palette original GetItem returned a foreign slot";return false;}}
    receipt.attempted=true;++receipt.flag_calls;if(!flag(dungeon+0x1d0,intent.defaults,error))return false;
    for(size_t i=0;i<4;++i){++receipt.color_calls;if(!color(slots[i],intent.colors[i],error))return false;}
    uint8_t value=2;if(!at(dungeon,0x1d0,value,error))return false;
    if(value!=uint8_t(intent.defaults)){error="Dungeon palette original boolean loader readback differs";return false;}
    for(size_t i=0;i<4;++i){uint32_t packed=0;if(!at(slots[i],0,packed,error))return false;if(packed!=intent.packed[i]){error="Dungeon palette original packed-color readback differs";return false;}}
    receipt.returned=true;return true;
}
}

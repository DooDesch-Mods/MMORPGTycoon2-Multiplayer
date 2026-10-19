#pragma once
#include "typed_action/dungeon_palette_action.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mt2mp::game::dungeon_layout_reader {
using Cell=action_session::DungeonPaletteCell;
struct Record {std::vector<Cell> cells;std::vector<uint32_t> palette;bool operator==(const Record&)const=default;};
struct Local {uintptr_t data=0,palette_data=0;int32_t count=0,capacity=0,palette_count=0,palette_capacity=0;std::vector<uintptr_t> cells;bool operator==(const Local&)const=default;};

// Cells are native owned slots without UID headers. The caller binds the actual Dungeon full UID and exterior key.
template<class Read,class Text>bool read(Read& at,Text& text,uintptr_t image,uintptr_t dungeon,Record& output,Local& local,std::string& error){
    output={};local={};Record record;Local owner;uintptr_t ap=0;int32_t grid_count=0;
    if(!at(dungeon,0x108,ap,error)||!at(dungeon,0x80,grid_count,error)||!at(dungeon,0x128,owner.data,error)||
       !at(dungeon,0x130,owner.count,error)||!at(dungeon,0x134,owner.capacity,error)||!at(dungeon,0x1c0,owner.palette_data,error)||
       !at(dungeon,0x1c8,owner.palette_count,error)||!at(dungeon,0x1cc,owner.palette_capacity,error))return false;
    if(ap!=image+0x1706690){error="Dungeon native owned Layout address point differs";return false;}
    if(owner.count<0||owner.capacity<owner.count||owner.count!=grid_count||(owner.count&&!owner.data)||
       (owner.count&&owner.data>UINTPTR_MAX-size_t(owner.count)*sizeof(uintptr_t))){error="Dungeon native layout/grid slot counts or span differ";return false;}
    if(owner.palette_count<4||owner.palette_capacity<owner.palette_count||!owner.palette_data||owner.palette_data>UINTPTR_MAX-size_t(owner.palette_count)*sizeof(uint32_t)){
        error="Dungeon native color palette owner/span differs";return false;}
    for(int32_t index=0;index<owner.palette_count;++index){uint32_t color=0;if(!at(owner.palette_data,size_t(index)*sizeof(uint32_t),color,error))return false;record.palette.push_back(color);}
    std::set<uintptr_t> unique;
    for(int32_t index=0;index<owner.count;++index){uintptr_t object=0,data_ap=0;uint8_t open=2;Cell cell;
        if(!at(owner.data,size_t(index)*sizeof(uintptr_t),object,error))return false;
        if(!object||!unique.insert(object).second){error="Dungeon native layout slot is absent or aliases another cell";return false;}
        if(!at(object,0,ap,error)||!at(object,0x38,data_ap,error)||!at(object,0x50,cell.colors,error)||!at(object,0x60,open,error)||
           !text(object,0x18,cell.theme,error))return false;
        if(ap!=image+0x1723990||data_ap!=image+0x17378b0||open>1){error="Dungeon native layout Cell/Data class or open flag differs";return false;}
        cell.open=open!=0;record.cells.push_back(std::move(cell));owner.cells.push_back(object);
    }
    uintptr_t data=0,palette_data=0;int32_t count=0,capacity=0,colors=0,color_capacity=0,grid=0;
    if(!at(dungeon,0x128,data,error)||!at(dungeon,0x130,count,error)||!at(dungeon,0x134,capacity,error)||!at(dungeon,0x80,grid,error)||
       !at(dungeon,0x1c0,palette_data,error)||!at(dungeon,0x1c8,colors,error)||!at(dungeon,0x1cc,color_capacity,error))return false;
    if(data!=owner.data||count!=owner.count||capacity!=owner.capacity||grid!=grid_count||palette_data!=owner.palette_data||colors!=owner.palette_count||color_capacity!=owner.palette_capacity){
        error="Dungeon native layout/palette ownership changed during read";return false;}
    for(size_t index=0;index<owner.cells.size();++index){uintptr_t object=0;
        if(!at(data,index*sizeof(uintptr_t),object,error))return false;
        if(object!=owner.cells[index]){error="Dungeon native layout slot changed during read";return false;}}
    output=std::move(record);local=std::move(owner);return true;
}
}

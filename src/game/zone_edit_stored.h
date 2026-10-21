#pragma once
#include "native_polyshape.h"

namespace mt2mp::game::zone_edit_stored {
struct Stored {
    uint64_t uid=0;
    uintptr_t root=0,region=0;
    int32_t type=-1,density=0,monster_type=0,level=0;
    action_session::TerrainShape shape;
    bool operator==(const Stored&)const=default;
};
// These are original inverse values. Current canonical ownership is proved by
// the caller; no live Zone or pointer from this receipt is put on the wire.
template<class Memory>
bool read(Memory& memory,uintptr_t image,uintptr_t node,Stored& out,std::string& error){
    out={};
    if(!image||image>UINTPTR_MAX-0x16e4a18||!node||node>UINTPTR_MAX-0x150){
        error="Zone stored inverse image/node address is null or overflows";return false;
    }
    const auto at=[&](uintptr_t base,size_t offset,auto& value){
        if(!base||base>UINTPTR_MAX-offset||base+offset>UINTPTR_MAX-sizeof value){
            error="Zone stored inverse field address overflows";return false;
        }
        return memory.copy(base+offset,&value,sizeof value,error);
    };
    const auto sample=[&](Stored& value,uintptr_t& ap,uintptr_t& method){
        if(!at(node,0,ap))return false;
        uintptr_t expected=0;
        if(ap==image+0x16e4a00)expected=image+0x55bd50;
        else if(ap==image+0x16e4980)expected=image+0x55c710;
        else{error="Zone stored inverse is not an original MakeZone/EditZone addresspoint";return false;}
        if(!at(ap,0x10,method))return false;
        if(method!=expected){error="Zone stored inverse original Do method differs";return false;}
        if(!at(node,0xb8,value.uid)||!at(node,0xc0,value.type)||!at(node,0xc4,value.density)||
           !ui_terrain_runtime::poly::read(memory,node+0xc8,value.shape,error)||
           !at(node,0x138,value.root)||!at(node,0x140,value.region)||
           !at(node,0x148,value.monster_type)||!at(node,0x14c,value.level))return false;
        if(!value.uid||!value.root||!value.region||value.type<0||value.type>3){
            error="Zone stored inverse full UID/root/Region/type differs";return false;
        }
        return true;
    };
    Stored first,second;uintptr_t first_ap=0,first_method=0,second_ap=0,second_method=0;
    if(!sample(first,first_ap,first_method)||!sample(second,second_ap,second_method))return false;
    if(first!=second||first_ap!=second_ap||first_method!=second_method){
        error="Zone stored inverse complete values or original class changed during read";return false;
    }
    out=std::move(second);error.clear();return true;
}
}

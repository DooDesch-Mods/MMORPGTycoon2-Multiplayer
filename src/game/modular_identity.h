#pragma once
#include "mt2mp/game/native_engine.h"
namespace mt2mp::game::modular_identity {
// This separate admission does not make modular Scenery a normal Building for
// the existing Service, purchase or standalone inventory UI contracts.
template<class Read>bool read(Read&& at,uintptr_t image,uintptr_t object,const native::BuildingState& value,std::string& e){
    if(!object||object>UINTPTR_MAX-0xcc0||value.type!="Scenery"||value.variant!="base"||value.dungeon_membership||value.dungeon_instance_membership){e="Modular identity requires the original root Scenery/base form";return false;}
    uintptr_t ap=0,layout=0,plan=0,layout_ap=0,plan_ap=0;uint64_t uid=0;uint8_t art=1;
    if(!at(object,size_t(0),ap,e)||ap!=image+0x16d93c0||!at(object,size_t(0x18),uid,e)||uid!=value.prop.uid||!uid||!at(object,size_t(0xcba),art,e)||art||!at(object,size_t(0x7d0),layout,e)||!layout||!at(object,size_t(0x7d8),plan,e)||!plan||!at(layout,size_t(0),layout_ap,e)||layout_ap!=image+0x1712a60||!at(plan,size_t(0),plan_ap,e)||plan_ap!=image+0x16fea40){if(e.empty())e="Modular identity exact class/full UID/Layout/Plan differs";return false;}
    uintptr_t fresh=0;if(!at(object,size_t(0x7d0),fresh,e)||fresh!=layout||!at(object,size_t(0x7d8),fresh,e)||fresh!=plan){if(e.empty())e="Modular identity owned Layout/Plan changed during read";return false;}e.clear();return true;
}
}

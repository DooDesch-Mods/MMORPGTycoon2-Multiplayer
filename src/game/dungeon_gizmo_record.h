#pragma once
#include "typed_action/dungeon_gizmo_action.h"
#include <array>
#include <string_view>
namespace mt2mp::game::dungeon_gizmo_record {
inline constexpr std::array<const char*,5> labels{"SetGold","SetLoot","SetVendorTrash","SetKeys","SetBossKeys"};
struct Window {uintptr_t object=0,target=0,proxy=0,grid=0;uint64_t uid=0;};
template<class Read,class Find>
bool window(Read& at,Find& find,uintptr_t image,uintptr_t object,Window& out,std::string& error){
    out={};Window value;value.object=object;uintptr_t ap=0,weak_ap=0;
    if(!at(object,0,ap,error)||!at(object,0x4e8,weak_ap,error)||!at(object,0x4f0,value.target,error)||!at(object,0x4f8,value.proxy,error)||!at(object,0x568,value.grid,error))return false;
    if(ap!=image+0x1719c80||weak_ap!=image+0x16f9ee0){error="Gizmo actual named Window/weak class differs";return false;}
    if(!value.target){if(value.proxy){error="Gizmo empty native Window retains a weak proxy";return false;}out=value;return true;}
    if(!value.proxy){error="Gizmo native Window target has no weak proxy";return false;}
    uintptr_t reverse=0,owner=0,actual=0;
    if(!at(value.target,0,ap,error)||!at(value.target,0x18,value.uid,error)||!at(value.target,0x28,reverse,error)||
       !at(value.proxy,0,owner,error))return false;
    if(ap!=image+0x1749ab0||!value.uid||reverse!=value.proxy||owner!=value.target+0x20){error="Gizmo native Window weak/full UID reciprocity differs";return false;}
    if(!find(value.uid,actual,error))return false;if(actual!=value.target){error="Gizmo Window full UID registry resolves another actor";return false;}
    out=value;return true;
}
template<class Read>bool grid(Read& at,uintptr_t image,const Window& window,std::string& error){
    uintptr_t ap=0,parent=0;int32_t rows=0;
    if(!window.grid){error="Gizmo native content Grid is absent";return false;}
    if(!at(window.grid,0,ap,error)||!at(window.grid,0x280,parent,error)||!at(window.grid,0x3c8,rows,error))return false;
    if(ap!=image+0x16dade0||parent!=window.object||rows!=5){error="Gizmo original five-row content Grid/parent differs";return false;}return true;
}
template<class Read>bool value(Read& at,uintptr_t record,int32_t& number,std::string& error){
    uintptr_t tokens=0;int32_t count=0,capacity=0,type=0;
    if(!at(record,0x38,tokens,error)||!at(record,0x40,count,error)||!at(record,0x44,capacity,error))return false;
    if(!tokens||count!=1||capacity<count){error="Gizmo normal numeric command requires one native token";return false;}
    if(!at(tokens,0,type,error)||!at(tokens,0x28,number,error))return false;
    if(type!=4||number<0||number>5){error="Gizmo normal content input requires integer 0..5";return false;}return true;
}
inline bool replay(uintptr_t receiver,action_session::DungeonGizmoEdit edit,int32_t value,uintptr_t expected_receiver,
                   action_session::DungeonGizmoEdit expected_edit,int32_t expected_value,const action_session::DungeonGizmoRecord* preparation,
                   uint32_t& calls,bool& execute,std::string& error){
    execute=false;if(receiver!=expected_receiver){error="Gizmo replay native Window differs";return false;}
    if(preparation){auto copy=*preparation;const auto* field=action_session::dungeon_gizmo_value(copy,edit);
        if(!field||*field!=value){error="Gizmo native Window preparation attempted a different gameplay value";return false;}
        ++calls;return true;
    }
    if(edit!=expected_edit||value!=expected_value||calls){error="Gizmo original-once edit/value/count differs";return false;}
    ++calls;execute=true;return true;
}
template<class Restore>bool restore_once(bool& attempted,Restore&& restore,std::string& error){
    if(attempted){error="Gizmo local Window restoration already attempted; original not replayed";return false;}
    attempted=true;return restore(error);
}
}

#pragma once
#include "dungeon_gizmo_live_state.h"

namespace mt2mp::game::dungeon_gizmo_live {
struct Local {
    dungeon_gizmo_identity::Member member;
    uintptr_t blueprint=0,display=0,door_actor=0,trigger_actor=0,model=0,definition=0;uint32_t features=0;
    std::array<int32_t,4> indexes{-1,-1,-1,-1};
    bool operator==(const Local&)const=default;
};
template<class Read,class ClipRead>
bool read_row(Read& at,ClipRead& clip,uintptr_t image,const dungeon_gizmo_identity::Member& owner,
              const std::string& key,Row& out,Local& local,std::string& error){
    error.clear();out={};local={};Row row;row.key=key;Local pin;pin.member=owner;uintptr_t ap=0,shard=0,region=0;uint64_t uid=0;
    uint8_t triggered=2,transition=2,interaction=0;
    if(!owner.object||!owner.uid||!dungeon_gizmo_identity::key(key)||
       !at(owner.object,0,ap,error)||ap!=image+0x1749ab0||!at(owner.object,0x18,uid,error)||uid!=owner.uid||
       !at(owner.object,0x170,shard,error)||shard!=owner.shard||!at(owner.object,0x178,region,error)||region!=owner.region||
       !at(owner.object,0x250,triggered,error)||triggered>1||!at(owner.object,0x254,row.state,error)||
       !at(owner.object,0x328,row.immediate,error)||!at(owner.object,0x2d0,transition,error)||transition>1||
       !at(owner.object,0x2d8,pin.blueprint,error)||!at(owner.object,0x2e0,pin.door_actor,error)||
       !at(owner.object,0x300,pin.model,error)||!at(owner.object,0x278,pin.definition,error))
        return fail(error,error.empty()?"current class/full UID/Scene or boolean differs":error);
    if(pin.definition&&!at(pin.definition,0xa0,pin.features,error))return false;
    row.triggered=triggered!=0;row.transition=transition!=0;row.interaction=interaction!=0;row.on_map=pin.model!=0;
    if(pin.blueprint){if(!at(pin.blueprint,0x210,pin.display,error))return false;
        if(pin.display&&!at(pin.display,0x498,pin.trigger_actor,error))return false;}
    if((pin.features&1)&&pin.trigger_actor){if(!at(owner.object,0x2f8,interaction,error)||interaction>1)return fail(error,error.empty()?"container interaction byte differs":error);row.interaction=interaction!=0;}
    for(size_t n=0;n<4;++n){const auto actor=n<3?pin.door_actor:pin.trigger_actor;
        if(!actor||(n<3?!(pin.features&2):!(pin.features&1)))continue;int32_t count=-1;
        if(!at(actor,0x20,count,error)||count<0||count>4096||!at(owner.object,0x2e8+n*4,pin.indexes[n],error))return false;
        if(pin.indexes[n]>=count)return fail(error,"semantic clip index exceeds its actual Actor asset");
        if(!clip(actor,pin.indexes[n],row.clips[n],error))return false;
    }
    if(row.transition){int32_t actual_index=-1;
        if(!at(owner.object,0x2c4,actual_index,error)||!at(owner.object,0x2c8,row.from,error)||!at(owner.object,0x2cc,row.to,error))return false;
        for(size_t n=0;n<3;++n)if(pin.indexes[n]>=0&&pin.indexes[n]==actual_index){row.animation=int32_t(n);break;}
        if(actual_index>=0&&(!pin.door_actor||row.animation<0))return fail(error,"active Door animation lacks its actual semantic asset");
    }
    // The ctor leaves the inactive transition cache uninitialized. Read it only
    // while the native transition flag owns it, and publish semantic slots.
    uintptr_t check=0;if(!at(owner.object,0x2d8,check,error)||check!=pin.blueprint||!at(owner.object,0x2e0,check,error)||check!=pin.door_actor)
        return fail(error,error.empty()?"render owner changed during complete read":error);
    std::string why;if(!valid(State{Scene{owner.scene,0,{row}}},why))return fail(error,why);
    out=std::move(row);local=pin;return true;
}
}

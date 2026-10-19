#pragma once
#include "intervention_actor_native.h"
#include "boss_identity.h"
#include "intervention_actor_links.h"
#include <algorithm>

namespace mt2mp::game::interventions::actors {
struct Local {
    Target target;uintptr_t object=0;uint64_t uid=0;
    boss_identity::Scope scope;native::Render render;uintptr_t skeleton=0;
    std::vector<uintptr_t> ready_members;
    uintptr_t zone=0;uint64_t zone_uid=0;int32_t zone_index=-1;
    std::shared_ptr<const Links> links;
    uintptr_t item_data=0;int32_t item_count=0,item_capacity=0;
    std::vector<ItemLocal> items;
    bool operator==(const Local&)const=default;
};
template<class Read,class Copy,class Maximum,class ClipRead>
bool read_row(Read& at,Copy& copy,Maximum& maximum,ClipRead& clip,uintptr_t image,const Local& owner,Row& out,Local& local,std::string& error){
    out={};local=owner;const auto object=owner.object;uintptr_t ap=0,root=0,region=0,set_map=0,transform_ap=0,notify=0;uint64_t uid=0;uint8_t dead=2,on_map=2;
    const auto family=owner.target.family;const uintptr_t expected_ap=family==Family::Npc?0x1742980:family==Family::Boss?0x1743040:family==Family::Monster?0x16c0770:0;
    const auto failed=[&](const std::string& cause){error=cause;return false;};
    if(!expected_ap||!interventions::valid(owner.target)||!boss_identity::valid(owner.scope)||!object||!owner.uid)return failed("Actor Frame read lacks current canonical physical owner");
    if(!at(object,0,ap,error)||ap!=image+expected_ap||!at(object,0x18,uid,error)||uid!=owner.uid||
       !at(object,0x170,root,error)||root!=owner.scope.shard||!at(object,0x178,region,error)||region!=owner.scope.region||
       !at(ap,0x290,set_map,error)||set_map!=image+(family==Family::Npc?0x3c2a00:0x39a920))return failed(error.empty()?"Actor Frame class/full UID/scene/map method differs":error);
    out.target=owner.target;
    if(!at(object,0x4a8,out.slot,error)||!at(object,0x4a4,out.type,error)||!at(object,0x6f8,out.level,error)||
       !at(region,0x834,out.pose.region,error)||!at(object,0x100,out.pose.orientation,error)||!at(object,0x110,out.pose.position,error)||!at(object,0x11c,out.pose.scale,error)||
       !at(object,0x4ac,dead,error)||dead>1||!at(object,0x368,on_map,error)||on_map>1||!at(object,0x7f8,out.action,error))return failed(error.empty()?"Actor Frame current Character/transform/boolean fields differ":error);
    out.dead=dead!=0;out.on_map=on_map!=0;
    const uintptr_t expected_transform_ap=family==Family::Npc?0x1742e58:family==Family::Boss?0x1743530:0x16c0c58;
    const uintptr_t expected_notify=family==Family::Npc?0x3c2de0:0x39e750;
    if(!at(object,0x30,transform_ap,error)||transform_ap!=image+expected_transform_ap||!at(transform_ap,0x38,notify,error)||notify!=image+expected_notify)return failed(error.empty()?"Actor Frame original embedded transform class/callback differs":error);
    if(!at(object,0x6f0,local.render.character,error)||!local.render.character||!at(local.render.character,0,local.render.actor,error)||!local.render.actor||!at(local.render.actor,0x40,local.skeleton,error)||!local.skeleton)return failed(error.empty()?"Actor Frame current CharacterActor/Actor/skeleton absent":error);
    int32_t count=-1;if(!at(local.render.actor,0x20,count,error)||count<0||count>4096||!copy(local.render.character+8,local.render.indexes.data(),sizeof local.render.indexes,error))return failed(error.empty()?"Actor Frame semantic clip array extent differs":error);
    for(size_t i=0;i<animation_slots;++i){if(local.render.indexes[i]>=count)return failed("Actor Frame semantic clip index outside current native asset");if(!clip(local.render,int32_t(i),out.clips[i],error))return false;}
    for(size_t i=0;i<3;++i){uintptr_t attribute_ap=0;float value=0;
        if(!at(object,0x730+i*0x28,attribute_ap,error)||attribute_ap!=image+0x16dd550||!at(object,0x750+i*0x28,out.values[i],error)||!at(object,0x754+i*0x28,out.recovery[i],error)||!maximum(object,int32_t(i),value,error))return failed(error.empty()?"Actor Frame embedded attribute receiver differs":error);
        out.maxima[i]=std::bit_cast<uint32_t>(value);}
    if(family==Family::Npc){Npc value;
        if(!at(object,0x80c,value.marker_kind,error)||!at(object,0x838,value.online_state,error)||!at(object,0x834,value.state,error)||!at(object,0x828,value.respawn,error)||!at(object,0x818,value.spawn,error)||!at(object,0x824,value.move_delay,error)||!at(object,0x82c,value.interact_timer,error)||!at(object,0x830,value.tick_slot,error))return false;out.lifecycle=value;
    }else if(family==Family::Boss){Boss value;
        if(!at(object,0x968,value.phase,error)||!at(object,0x96c,value.state,error)||!at(object,0x9ac,value.respawn,error)||!at(object,0x970,value.spawn,error))return false;out.lifecycle=value;
    }else{ZoneMonster value;
        if(!zone_monster_key(owner.target.key)||!at(object,0x824,value.status,error)||!at(object,0x828,value.ai,error)||!at(object,0x910,value.respawn,error)||!at(object,0x82c,value.spawn,error))return failed(error.empty()?"Actor Frame Zone Monster current lifecycle differs":error);
        uintptr_t zone=0,zone_ap=0,zone_root=0,zone_region=0,zone_data=0,zone_member=0;uint64_t zone_uid=0;int32_t zone_count=-1,zone_capacity=-1;
        if(!owner.zone||!owner.zone_uid||owner.zone_index<0||!at(object,0x8d0,zone,error)||zone!=owner.zone||!at(zone,0,zone_ap,error)||zone_ap!=image+0x1745ce0||!at(zone,0x18,zone_uid,error)||zone_uid!=owner.zone_uid||!at(zone,0x170,zone_root,error)||zone_root!=owner.scope.shard||!at(zone,0x178,zone_region,error)||zone_region!=owner.scope.region||!at(zone,0x388,zone_data,error)||!at(zone,0x390,zone_count,error)||!at(zone,0x394,zone_capacity,error))return failed(error.empty()?"Actor Frame Zone Monster canonical parent/full UID/owner differs":error);
        if(zone_count<0||zone_count>4096||zone_capacity<zone_count||owner.zone_index>=zone_count||!zone_data||zone_data>UINTPTR_MAX-size_t(zone_count)*8||!at(zone_data,size_t(owner.zone_index)*8,zone_member,error)||zone_member!=object)return failed(error.empty()?"Actor Frame Zone Monster current canonical member ordinal no longer names this full UID":error);
        uintptr_t manager=0,manager_owner=0,data=0;int32_t ready_count=-1,capacity=-1;
        if(!at(region,0x988,manager,error)||!at(manager,0,ap,error)||ap!=image+0x1714100||!at(manager,0x80,manager_owner,error)||manager_owner!=region||!at(manager,0x70,data,error)||!at(manager,0x78,ready_count,error)||!at(manager,0x7c,capacity,error))return failed(error.empty()?"Actor Frame Zone Monster ready manager reciprocity differs":error);
        if(ready_count<0||ready_count>4096||capacity<ready_count||(ready_count&&(!data||data>UINTPTR_MAX-size_t(ready_count)*8)))return failed("Actor Frame Zone Monster ready array extent invalid");
        std::vector<uintptr_t> members(size_t(ready_count),uintptr_t{});if(ready_count&&!copy(data,members.data(),members.size()*8,error))return false;
        std::set<uintptr_t> unique;for(const auto member:members)if(!member||!unique.insert(member).second)return failed("Actor Frame Zone Monster ready array repeats a member");
        value.ready=unique.contains(object);uintptr_t after_data=0;int32_t after_count=-1,after_capacity=-1;
        if(!at(manager,0x70,after_data,error)||!at(manager,0x78,after_count,error)||!at(manager,0x7c,after_capacity,error)||after_data!=data||after_count!=ready_count||after_capacity!=capacity)return failed(error.empty()?"Actor Frame Zone Monster ready array changed during read":error);
        local.ready_members=std::move(members);out.lifecycle=value;
    }
    return valid(State{out},error);
}
}

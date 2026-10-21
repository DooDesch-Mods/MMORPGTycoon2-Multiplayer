#pragma once
#include "building_npc_owner.h"
#include "dungeon_scene_roster.h"
#include "dungeon_gizmo_identity.h"
namespace mt2mp::game::building_npc_owner {
template<class Read>
bool binding_scope(Read& at,uintptr_t image,uintptr_t world_root,const npc_saved_identity::Current& current,
                   uintptr_t& region,std::string& error){
    region=0;
    if(!building_npc_key::valid(current.key)||!dungeon_gizmo_identity::scene(current.scene)||
       !current.native_shard||!current.native_region||current.native_shard==world_root||
       !current.row.local_pointer||!current.row.local_uid)return fail(error,"NPC scoped current child owner metadata invalid");
    uintptr_t shard=0,actual_region=0,ap=0,reverse=0;int32_t id=-1,type=-1;
    if(!at(current.row.local_pointer,0x170,shard,error)||!at(current.row.local_pointer,0x178,actual_region,error)||
       !at(current.native_region,0,ap,error))return false;
    if(ap!=image+0x1787930)return fail(error,"NPC scoped current Region class differs");
    if(!at(current.native_region,0x70,reverse,error)||!at(current.native_region,0x834,id,error)||
       !at(current.native_shard,0,ap,error)||!at(current.native_shard,0x48,type,error))return false;
    if(ap!=image+0x174ad20||shard!=current.native_shard||actual_region!=current.native_region||reverse!=shard||
       id!=current.saved_region||type!=(current.scene.ends_with("/template")?1:2))return fail(error,"NPC scoped current child/Shard/Region binding differs");
    region=current.native_region;error.clear();return true;
}
// The deleted parent cannot be read after original queued deletion. Prove its
// retained Scene from the current original roster and complete Region census.
template<class Read>
bool retirement_scope(Read& at,const std::vector<dungeon_scene_roster::Scene>& scenes,uintptr_t image,
                      const building_npc_lifecycle::Receipt& before,Scope& output,std::string& error){
    output={};if(!before.present||!dungeon_gizmo_identity::scene(before.parent.scene)||!before.original_shard||!before.original_region)
        return fail(error,"Building child NPC retirement lacks its original scoped receipt");
    const dungeon_scene_key::Scene* selected=nullptr;
    for(const auto& row:scenes)if(row.owner.key==before.parent.scene){
        if(selected)return fail(error,"Building child NPC retirement scene repeats");selected=&row.owner;
    }
    if(!selected||selected->shard!=before.original_shard||selected->region!=before.original_region)
        return fail(error,"Building child NPC retirement original scene is not current");
    uintptr_t ap=0,reverse=0;int32_t type=-1,id=-1;
    if(!at(selected->shard,0,ap,error))return false;
    if(ap!=image+0x174ad20)return fail(error,"Building child NPC retirement Shard class differs");
    if(!at(selected->shard,0x48,type,error)||!at(selected->region,0,ap,error))return false;
    if(ap!=image+0x1787930)return fail(error,"Building child NPC retirement Region class differs");
    if(!at(selected->region,0x70,reverse,error)||!at(selected->region,0x834,id,error))return false;
    if(type!=(before.parent.scene.ends_with("/template")?1:2)||reverse!=selected->shard||id!=before.parent.saved_region)
        return fail(error,"Building child NPC retirement original Scene reciprocal owner differs");
    output={image,selected->shard,selected->region};error.clear();return true;
}
// Scene membership comes from the existing original Dungeon roster. Only an
// exact retained graph owner can replace active roster membership after Remove.
template<class Read,class Root,class Scenes,class Held,class Find>
bool scope(Read& at,Root& root,Scenes& scenes,Held& held,Find& find,uintptr_t image,uintptr_t world_root,
           const building_npc_lifecycle::Parent& parent,Scope& output,std::string& error){
    output={};if(parent.scene=="world"){uintptr_t region=0;if(!root(parent.saved_region,region,error))return false;
        output={image,world_root,region};return true;}
    if(!dungeon_gizmo_identity::scene(parent.scene))return fail(error,"Building child NPC nonroot scene key invalid");
    uintptr_t registered=0;if(!find(parent.uid,registered,error))return false;
    if(registered!=parent.object)return fail(error,"Building child NPC scene parent full UID registry differs");
    std::vector<dungeon_scene_roster::Scene> current;if(!scenes(current,error))return false;
    const dungeon_scene_key::Scene* selected=nullptr;
    for(const auto& row:current)if(row.owner.key==parent.scene){if(selected)return fail(error,"Building child NPC scene owner repeated");selected=&row.owner;}
    uintptr_t shard=0,region=0,ap=0,reverse=0;uint64_t uid=0;int32_t id=-1,type=-1;
    if(!at(parent.object,0,ap,error)||!at(parent.object,0x18,uid,error)||!at(parent.object,0x170,shard,error)||
       !at(parent.object,0x178,region,error))return false;
    if(ap!=image+0x16d93c0||uid!=parent.uid||!shard||!region||shard==world_root)
        return fail(error,"Building child NPC nonroot parent class/full UID/owner differs");
    if(selected){if(selected->shard!=shard||selected->region!=region)return fail(error,"Building child NPC exact active scene reciprocal owner differs");}
    else {std::string key,scene;bool matched=false;if(!held(parent.object,key,matched,error,&scene))return false;
        if(!matched||key!=parent.key||scene!=parent.scene)return fail(error,"Building child NPC absent active scene has no exact retained graph owner");}
    if(!at(shard,0,ap,error))return false;
    if(ap!=image+0x174ad20)return fail(error,"Building child NPC current Shard class differs");
    if(!at(shard,0x48,type,error)||!at(region,0,ap,error))return false;
    if(ap!=image+0x1787930)return fail(error,"Building child NPC current Region class differs");
    if(!at(region,0x70,reverse,error)||!at(region,0x834,id,error))return false;
    if(reverse!=shard||id!=parent.saved_region||type!=(parent.scene.ends_with("/template")?1:2))
        return fail(error,"Building child NPC exact original scene type/Region differs");
    output={image,shard,region};error.clear();return true;
}
}

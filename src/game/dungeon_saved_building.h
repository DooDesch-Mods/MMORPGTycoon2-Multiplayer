#pragma once
#include "dungeon_scene_key.h"
#include "dungeon_scene_roster.h"
#include <algorithm>

namespace mt2mp::game::dungeon_saved_building {
// The saved path uses the original root exterior child and Instance.index.
// It does not require a canonical alias before snapshot identity is sealed.
template<class Read,class Typed,class RootSlot>
bool path(uintptr_t image,uintptr_t map,uintptr_t object,int32_t child,uintptr_t expected_array,
          Read& at,Typed& typed,RootSlot& root_slot,std::string& suffix,std::string& error){
    suffix.clear();const auto fail=[&](const char* cause){error=cause;return false;};
    if(!map||map>UINTPTR_MAX-0x1c0)return fail("Saved Building Map/root address invalid");
    uintptr_t shard=0,region=0,manager=0,ap=0,reverse=0;
    if(child<0||!at(object,0,ap,error)||ap!=image+0x16d93c0||
       !at(object,0x170,shard,error)||!at(object,0x178,region,error))
        return fail(error.empty()?"Saved Building type/child/owner differs":error.c_str());
    const auto root=map+0x1c0;
    if(shard==root){int32_t id=-1;if(!root_slot(object,child,id,expected_array,error))return false;
        suffix=snapshot_building_key::root_suffix(id,child);return true;}
    dungeon_scene_key::Scene scene;
    if(!dungeon_scene_key::read_owner(image,root,shard,at,typed,scene,error))return false;
    if(scene.region!=region||!at(region,0x950,manager,error)||!at(manager,0,ap,error)||ap!=image+0x17183c0||
       !at(manager,0x450,reverse,error)||reverse!=shard||!at(manager,0x458,reverse,error)||reverse!=region||
       (expected_array&&expected_array!=manager+0x408))
        return fail(error.empty()?"Saved Dungeon Building reciprocal manager differs":error.c_str());
    if(!at(manager,0x408,ap,error)||ap!=image+0x172d7c0)return fail(error.empty()?"Saved Dungeon Building array class differs":error.c_str());
    dungeon_scene_roster::Array members;
    if(!dungeon_scene_roster::array(at,manager+0x408,members,error))return false;
    if(members.count>4096||child>=members.count||members.members[size_t(child)]!=object)
        return fail("Saved Dungeon Building child is absent from exact ordered array");
    uintptr_t exterior_region=0,exterior_manager=0;
    if(!at(scene.exterior,0x178,exterior_region,error)||!at(exterior_region,0x950,exterior_manager,error))return false;
    dungeon_scene_roster::Array exteriors;
    if(!dungeon_scene_roster::array(at,exterior_manager+0x408,exteriors,error))return false;
    if(exteriors.count>4096)return fail("Saved Dungeon exterior array exceeds native snapshot budget");
    const auto exterior=std::find(exteriors.members.begin(),exteriors.members.end(),scene.exterior);
    if(exterior==exteriors.members.end())return fail("Saved Dungeon exterior is absent from original root array");
    const auto exterior_child=int32_t(exterior-exteriors.members.begin());int32_t exterior_id=-1;
    if(!root_slot(scene.exterior,exterior_child,exterior_id,exterior_manager+0x408,error))return false;
    dungeon_scene_key::Scene final;
    if(!dungeon_scene_key::read_owner(image,root,shard,at,typed,final,error)||
       final.type!=scene.type||final.region!=scene.region||final.dungeon!=scene.dungeon||final.instance!=scene.instance||
       final.instance_index!=scene.instance_index||final.exterior!=scene.exterior||final.exterior_uid!=scene.exterior_uid||
       !at(object,0x170,reverse,error)||reverse!=shard||!at(object,0x178,reverse,error)||reverse!=region||
       !at(region,0x950,reverse,error)||reverse!=manager||!at(manager,0x450,reverse,error)||reverse!=shard||
       !at(manager,0x458,reverse,error)||reverse!=region||
       !dungeon_scene_roster::unchanged(at,manager+0x408,members,error)||
       !dungeon_scene_roster::unchanged(at,exterior_manager+0x408,exteriors,error))
        return fail(error.empty()?"Saved Dungeon Building owner or ordered child changed":error.c_str());
    suffix=snapshot_building_key::root_suffix(exterior_id,exterior_child)+
        (scene.type==1?"/template":"/instance["+std::to_string(scene.instance_index)+"]")+
        "/buildingManager/building["+std::to_string(child)+"]";
    error.clear();return true;
}
}

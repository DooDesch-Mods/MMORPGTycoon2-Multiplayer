#pragma once
#include "action_session.h"
namespace mt2mp::action_session::tick_commands {
inline bool ordered_dungeon_creation(const Request& request){
    return request.dungeon_building&&request.action==Action::PlaceBuilding&&
        request.dungeon_scene=="world"&&request.building.type=="DungeonExterior";
}
inline bool ordered_dungeon_placement(const Request& request){
    return ordered_dungeon_creation(request)||(request.dungeon_building&&request.action==Action::PlaceBuilding&&
        request.building.type=="DungeonPortal"&&request.dungeon_scene!="world"&&
        valid_dungeon_building_scene(request.dungeon_scene));
}
inline bool ordered_dungeon_action(const Request& request){
    if(ordered_dungeon_placement(request))return true;
    if(!request.dungeon_building||request.dungeon_scene=="world"||
       !valid_dungeon_building_scene(request.dungeon_scene))return false;
    // Remove has no proposed Building value. Its native transaction proves the
    // sampled target is a Portal before entering the original operation.
    return (request.action==Action::MoveBuilding&&request.building.type=="DungeonPortal")||
        request.action==Action::RemoveSnapshotBuilding;
}
// History remains owned by its original local mutation. Its native executor
// verifies confirmation, retirement, graph ownership and the exact stack top.
inline bool ordered_building_family(const Request& request,const Request* placed,const Request* removed,std::string& cause) {
    if(request.dungeon_building){
        if(ordered_dungeon_action(request))return true;
        cause="Ordered Dungeon Building action requires original Exterior placement or Portal placement/move/remove";return false;
    }
    switch(request.action){
    case Action::SetLogo:
    case Action::PlaceBuilding:
    case Action::MoveBuilding:
    case Action::RemoveSnapshotBuilding:return true;
    case Action::UndoBuilding:
    case Action::RedoBuilding:break;
    default:cause="Ordered native action family has not installed deterministic execution scopes";return false;
    }
    if((placed!=nullptr)==(removed!=nullptr)){cause="Ordered Building history requires one exact original mutation";return false;}
    const auto& source=placed?*placed:*removed;
    if(source.dungeon_building||source.action_uuid!=request.target_action_uuid||source.snapshot_uuid!=request.snapshot_uuid||
       source.canonical_record_key!=request.canonical_record_key||
       (placed?(source.action!=Action::PlaceBuilding&&source.action!=Action::MoveBuilding):source.action!=Action::RemoveSnapshotBuilding)){
        cause="Ordered Building history source action, snapshot, key, or family differs";return false;
    }
    return true;
}
}

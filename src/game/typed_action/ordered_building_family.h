#pragma once
#include "action_session.h"
namespace mt2mp::action_session::tick_commands {
// History remains owned by its original local mutation. Its native executor
// verifies confirmation, retirement, graph ownership and the exact stack top.
inline bool ordered_building_family(const Request& request,const Request* placed,const Request* removed,std::string& cause) {
    if(request.dungeon_building){cause="Ordered Dungeon Building execution scopes are not installed";return false;}
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

#pragma once
#include "typed_action/action_session.h"
#include <memory>
#include <stdexcept>
namespace dungeon_building_fixture {
using namespace mt2mp::action_session;
inline constexpr const char* action_id="33333333-3333-4333-8333-333333333333";
inline constexpr const char* source_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
inline Ledger money(){Ledger value;value.cash=value.record_cash=10000;value.flat_day=value.record_day=8;value.record_present=1;return value;}
inline BuildingState object(bool world){BuildingState value;value.type=world?"DungeonExterior":"DungeonPortal";value.variant="Default";value.alive=true;value.x=10;return value;}
inline Request request(Action action,bool world){Request value;value.action=action;value.peer=2;value.sequence=1;
    value.action_uuid=action_id;value.snapshot_uuid=snapshot_id;value.canonical_record_key=std::string("action:")+(action==Action::PlaceBuilding?action_id:source_id);
    value.dungeon_building=true;value.dungeon_scene=world?"world":std::string("action:")+source_id+"/template";
    if(action==Action::PlaceBuilding||action==Action::MoveBuilding)value.building=object(world);
    if(action==Action::UndoBuilding||action==Action::RedoBuilding)value.target_action_uuid=source_id;return value;}
inline DungeonBuildingEffect effect(const Request& request){DungeonBuildingEffect value;const auto exterior=std::string("action:")+source_id;
    value.before.authored[exterior]=std::string(64,'a');value.after.authored=value.before.authored;
    if(request.dungeon_scene=="world"&&request.action==Action::PlaceBuilding)value.after.authored[request.canonical_record_key]=std::string(64,'b');
    else if(request.dungeon_scene=="world"&&(request.action==Action::RemoveSnapshotBuilding||request.action==Action::RedoBuilding))value.after.authored.erase(exterior);
    else value.after.authored[exterior]=std::string(64,'b');
    if(request.dungeon_scene=="world"&&request.action==Action::UndoBuilding)value.before.authored.erase(exterior);
    value.before.quests=value.after.quests=std::string(64,'c');value.before.references=value.after.references=std::string(64,'d');
    value.before.random={1,2,3,4};value.after.random={5,6,7,8};value.inverse=std::string(64,'e');
    namespace clone=mt2mp::game::scene_clone;const auto scene=exterior+"/template";
    value.mapping.operation=request.action_uuid;clone::SceneCount count;count.scene=scene;count.gizmos=1;count.buildings=1;value.mapping.scenes.push_back(count);
    value.mapping.members.push_back({scene,clone::birth_key(request.action_uuid,clone::Family::Gizmo,0),clone::Family::Gizmo,0,0,request.action_uuid});
    value.mapping.members.push_back({scene,clone::birth_key(request.action_uuid,clone::Family::Building,1),clone::Family::Building,0,1,request.action_uuid});
    return value;}
inline Message message(Action action,bool world,bool result){Message value;value.session=5;value.request=request(action,world);
    if(!result)return value;
    value.kind=Kind::HostResult;value.revision=1;value.required_mask=3;value.outcome=Outcome::Confirmed;
    auto& e=value.evidence;e.before=e.after=money();e.undo_confirmed=e.retirement_confirmed=true;
    e.object_before=object(world);e.object_after=e.object_before;
    if(action==Action::PlaceBuilding){e.object_before={};e.after.cash-=50;e.after.record_cash-=50;}
    if(action==Action::MoveBuilding)e.object_before.x-=1;
    if(action==Action::RemoveSnapshotBuilding){e.object_after.alive=false;e.target_retired=e.selection_confirmed=true;e.after.cash+=25;e.after.record_cash+=25;}
    if(action==Action::UndoBuilding)e.object_before.alive=false;
    if(action==Action::RedoBuilding){e.object_after.alive=false;e.target_retired=true;}
    e.dungeon_building_effect=effect(value.request);return value;
}
}

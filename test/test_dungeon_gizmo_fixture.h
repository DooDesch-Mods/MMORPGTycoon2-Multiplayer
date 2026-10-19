#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline bool equal_apply=false,saved_key=false,instance_scene=false;
inline DungeonGizmoEdit gizmo_edit=DungeonGizmoEdit::Gold;inline int32_t desired_value=5;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline std::string scene_key(){return std::string(64,'b')+":world:building:17"+(instance_scene?"/instance/9":"/template");}
inline std::string gizmo_key(){return saved_key?mt2mp::game::dungeon_gizmo_identity::record(std::string(64,'b'),67,3,instance_scene?9:-1,0):mt2mp::game::dungeon_gizmo_identity::birth(action_id,0);}
inline std::string parent_key(){return mt2mp::game::dungeon_gizmo_identity::record(std::string(64,'b'),67,3,-1,1);}
inline DungeonGizmoIntent intent(){return {gizmo_key(),scene_key(),std::string(64,'a'),gizmo_edit,desired_value};}
inline DungeonGizmoState before(){
    DungeonGizmoState value;value.protected_sha256=std::string(64,'c');value.global_random={11,12,13,14};
    DungeonGizmoRecord record;record.name="chest";record.type="Treasure";record.variant="base";record.parent=parent_key();record.rotation={0,0,0,1};record.position={1,2,3};record.scale={1,1,1};record.state=3;
    record.gold=1;record.loot=2;record.vendor_trash=3;record.keys=4;record.boss_keys=5;record.lock_id=7;record.edge_a=8;record.edge_b=9;
    *dungeon_gizmo_value(record,gizmo_edit)=equal_apply?desired_value:desired_value==5?0:5;
    DungeonGizmoRecord parent=record;parent.parent.clear();parent.name="prototype";
    value.scenes={{scene_key(),{{gizmo_key(),record},{parent_key(),parent}}}};return value;
}
inline DungeonGizmoState after(const DungeonGizmoState& value){DungeonGizmoState out;need(dungeon_gizmo_project(intent(),value,out),"exact Gizmo projection failed");return out;}
inline Ledger money(){Ledger v;v.cash=v.record_cash=10000;v.flat_day=v.record_day=28;v.record_present=1;v.record_income=500;v.record_expense=100;v.line_counts[0]=2;v.line_income[0]=-100;return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::EditDungeonGizmo;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=intent().key;r.dungeon_gizmo=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence.before=m.evidence.after=money();m.evidence.dungeon_gizmo_before=before();m.evidence.dungeon_gizmo_after=after(before());m.evidence.retirement_confirmed=true;return m;}

#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
namespace input_fixture {
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline DungeonInputTool selected_tool=DungeonInputTool::Door;
inline int32_t price=0,rank=0;inline bool instance=false;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline DungeonInputIntent intent(){
    DungeonInputIntent v;v.key=std::string(64,'b')+":world:building:17";v.scene=v.key+(instance?"/instance/9":"/template");
    v.before_sha256=std::string(64,'a');v.tool=selected_tool;
    if(selected_tool==DungeonInputTool::Monster){v.character_type=4;v.rank=rank;v.level=5;v.position={1,2,3};}
    else if(selected_tool==DungeonInputTool::Paint){v.erase=true;v.shapes={{{{0,0},{32,0},{32,32},{0,32}}}};}
    else{v.type=selected_tool==DungeonInputTool::Treasure?"container":selected_tool==DungeonInputTool::Door?"Door":"Key";v.variant="base";v.cost=price;v.position={1,2,3};}
    return v;
}
inline DungeonInputState before(){
    DungeonInputState v;v.dungeon_sha256=std::string(64,'a');v.scenes_sha256=std::string(64,'b');
    v.quests_sha256=std::string(64,'c');v.layout_sha256=std::string(64,'d');
    auto& sound=v.protected_state;for(size_t i=0;i<sound.light.size();++i)sound.light[i]=float(i+1)*.1f;sound.atmosphere=3;
    sound.dungeon.desired=1;sound.dungeon.price=100;sound.dungeon.instances={{0,true},{2,false}};
    sound.dungeon.global_random={11,12,13,14};sound.dungeon.undo_count=2;return v;
}
inline DungeonInputState after(const DungeonInputState& prior){
    auto v=prior;++v.protected_state.dungeon.undo_count;const auto wanted=intent();
    if(selected_tool==DungeonInputTool::Paint)v.dungeon_sha256=std::string(64,'e');
    else{
        v.scenes_sha256=std::string(64,'f');DungeonInputBirth born;born.scene=wanted.scene;born.tool=selected_tool;
        if(selected_tool==DungeonInputTool::Monster){born.key=std::string("action:")+action_id;
            born.monster={0,3,rank,1,4,5,"Spider",wanted.position,{1,1,1},wanted.rotation};}
        else{born.key=mt2mp::game::dungeon_gizmo_identity::birth(action_id,0);born.gizmo.name="native resource";
            born.gizmo.type=wanted.type;born.gizmo.variant=wanted.variant;born.gizmo.position=wanted.position;
            born.gizmo.rotation=wanted.rotation;born.gizmo.scale=wanted.scale;}
        v.birth=std::move(born);
    }
    return v;
}
inline Ledger money(){Ledger v;v.cash=v.record_cash=10000;v.flat_day=v.record_day=28;v.record_present=1;
    v.record_income=500;v.record_expense=100;v.line_counts[0]=2;v.line_income[0]=-100;return v;}
inline Ledger paid(){auto v=money();if(selected_tool==DungeonInputTool::Door||selected_tool==DungeonInputTool::Key||selected_tool==DungeonInputTool::Treasure){
    v.cash-=price;v.record_cash-=price;v.record_expense+=price;++v.line_counts[6];v.line_income[6]-=price;}return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::DungeonInput;r.peer=2;r.sequence=1;
    r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=intent().key;r.dungeon_input=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.evidence.before=money();m.evidence.after=paid();m.evidence.dungeon_input_before=before();m.evidence.dungeon_input_after=after(before());
    m.evidence.undo_confirmed=m.evidence.retirement_confirmed=true;return m;}
}

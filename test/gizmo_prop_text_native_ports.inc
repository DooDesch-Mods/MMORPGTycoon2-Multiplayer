namespace dungeon_gizmo_identity=mt2mp::game::dungeon_gizmo_identity;
namespace gizmo_rename_native_fixture {
action::DungeonGizmoState current;
dungeon_gizmo_identity::Member member;
unsigned writes=0;bool owner=true,fail_original=false,corrupt=false;
void setup(){current={};current.protected_sha256=std::string(64,'a');current.global_random={1,2,3,4};
    const std::string scene="action:33333333-3333-4333-8333-333333333333/template";
    member={scene,0x70000,0x71000,0x72000,0x73000,0x100000001};
    action::DungeonGizmoRecord row;row.name="Old chest";row.type="container";row.variant="base";row.rotation={0,0,0,1};row.scale={1,1,1};row.gold=3;
    current.scenes={{scene,{{"action:44444444-4444-4444-8444-444444444444:dungeon:gizmo:0",row}}}};
    writes=0;owner=true;fail_original=corrupt=false;
}
}
namespace mt2mp::game::npc_probe::monster_action_runtime::dungeon_gizmo {
bool rename_target(uintptr_t object,std::string& key,std::string& scene,std::string& error){
    using namespace gizmo_rename_native_fixture;
    if(!owner||object!=member.object){error="actual Header/full UID owner changed";return false;}
    key=current.scenes[0].rows[0].key;scene=member.scene;return true;
}
bool rename_read(const action::Binding& binding,const std::string& snapshot,const std::string& key,const std::string& scene,
                 action::DungeonGizmoState& out,dungeon_gizmo_identity::Member& actual,action::Ledger& money,witness::UndoWitness& undo,std::string& error){
    using namespace gizmo_rename_native_fixture;
    if(!owner||binding!=fixture_binding||snapshot!=fixture_snapshot||key!=current.scenes[0].rows[0].key||scene!=member.scene){error="actual current canonical Gizmo scope changed";return false;}
    out=current;actual=member;money=fixture_money;undo=witness::fixture_undo;return true;
}
bool rename_apply(const action::Binding&,const std::string&,const std::string&,const std::string&,const dungeon_gizmo_identity::Member& expected,
    const action::DungeonGizmoState& before,const action::Ledger&,const witness::UndoWitness&,const std::string& text,bool& returned,std::string& error){
    using namespace gizmo_rename_native_fixture;
    if(!owner||member!=expected||before!=current){error="actual fresh pre-store owner changed";return false;}
    ++writes;current.scenes[0].rows[0].value.name=text;returned=!fail_original;
    if(corrupt)++current.scenes[0].rows[0].value.gold;
    if(fail_original){error="original Prop Rename SEH=5";return false;}return true;
}
}
namespace scenery_kiss {bool can_move_rva(uintptr_t,uintptr_t){return false;}}
#include "gizmo_prop_text_runtime.inc"
#include "prop_text_rename_capture_actual.inc"

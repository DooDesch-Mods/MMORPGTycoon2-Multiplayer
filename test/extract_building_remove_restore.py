from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
def body(s,anchor):
    start=s.index(anchor)
    if anchor=='static bool restore_redone_building_impl(':
        signature='static bool restore_redone_building_impl(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,bool modular,const std::string& dungeon_scene={}) '
        assert s.startswith(signature+'{',start), 'Building restore full signature/body seam differs'
        pos=start+len(signature)
    else:pos=s.index('{',start)
    depth=1;end=pos+1
    while depth:
        if s[end]=='{':depth+=1
        elif s[end]=='}':depth-=1
        end+=1
    return s[start:end]+'\n'
a.output.mkdir(parents=True,exist_ok=True)
def emit(name,source,anchors):
    s=(a.root/source).read_text(encoding='utf-8')
    (a.output/name).write_text(''.join(body(s,x) for x in anchors),encoding='utf-8')
emit('history_method.inc','src/game/typed_action_runtime.cpp',['    action::Outcome perform_move_history(', '    action::Outcome perform_building_history('])
emit('construction_provenance.inc','src/game/typed_action_runtime.cpp',['    bool construction_jobs('])
effects=(a.root/'src/game/typed_action/action_session.cpp').read_text(encoding='utf-8')
effect=body(effects,'bool object_effect(')
dispatch='''bool object_effect(const Request& r,const Evidence& e) {
    if(npc_history(r))return valid_npc_history_effect(r,e);
    if(monster_history(r))return monster_history_effect(r,e);
    if(modular_action(r))return modular_effect(r,e);
    if(group_object_history(r))return group_object_effect(r,e);
    if(scenery_stroke_history(r))return scenery_stroke_history_effect(r,e);
    if(duplicate_history(r))return duplicate_history_effect(r,e);
    if(group_history(r))return group_history_effect(r,e);
'''
assert effect.startswith(dispatch), 'Update the explicit non-Building dispatch boundary'
# This direct helper fixture covers the Building branch. The Controller fixture
# also links the complete production codec, including every family dispatcher.
building_guard='''bool object_effect(const Request& r,const Evidence& e) {
    if(r.action!=Action::UndoBuilding&&r.action!=Action::RedoBuilding&&r.action!=Action::RemoveSnapshotBuilding&&r.action!=Action::PlaceBuilding&&r.action!=Action::MoveBuilding)
        throw std::runtime_error("Building effect fixture received another action family");
'''
(a.output/'history_effect.inc').write_text(building_guard+effect[len(dispatch):],encoding='utf-8')
emit('registry_history.inc','src/game/remove_diagnostics.cpp',['bool history_retirement(','bool retire_undone_building(','static bool restore_redone_building_impl(','bool restore_redone_building(','bool history_target_absent(','bool confirmed_remove_graph('])
emit('registry_binding.inc','src/game/remove_diagnostics.cpp',['struct Bound '])
with (a.output/'registry_binding.inc').open('a') as out:out.write(';\n')
emit('graph_history.inc','src/game/building_history_slice/native_bridge.cpp',['bool bind_current_snapshot(','bool submit_ui(','static bool remember_graph(','bool remember_place(','bool remember_remove(','bool remember_selection(','bool remember_region(','bool remember_path(','bool graph_for(','action::Outcome perform('])
runtime=(a.root/'src/game/typed_action_runtime.cpp').read_text(encoding='utf-8')
dispatch=body(runtime,'    bool retire(const action::Binding&')
assert 'return request.dungeon_building?retire_dungeon_building(binding,request,error):retire_base(binding,request,error);' in dispatch
retire=body(runtime,'    bool retire_base(const action::Binding&')
(a.output/'dungeon_original_scope.inc').write_text(body(runtime,'    bool dungeon_scope('),encoding='utf-8')
tail=retire[retire.index('        const auto found=operations_.find(request.action_uuid);'):]
(a.output/'remove_retire.inc').write_text('bool retire_remove(const action::Binding& binding,const action::Request& request,std::string& error) {\n'+tail,encoding='utf-8')
(a.output/'remove_operation.inc').write_text(body(runtime,'struct Operation {').rstrip()+';\n',encoding='utf-8')

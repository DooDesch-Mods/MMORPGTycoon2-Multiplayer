from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
def body(s,anchor):
    start=s.index(anchor);pos=s.index('{',start);depth=1;end=pos+1
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
emit('history_effect.inc','src/game/typed_action/action_session.cpp',['bool object_effect('])
emit('registry_history.inc','src/game/remove_diagnostics.cpp',['bool history_retirement(','bool retire_undone_building(','bool restore_redone_building(','bool history_target_absent(','bool confirmed_remove_graph('])
emit('registry_binding.inc','src/game/remove_diagnostics.cpp',['struct Bound '])
with (a.output/'registry_binding.inc').open('a') as out:out.write(';\n')
emit('graph_history.inc','src/game/building_history_slice/native_bridge.cpp',['bool bind_current_snapshot(','bool submit_ui(','static bool remember_graph(','bool remember_place(','bool remember_remove(','bool remember_selection(','bool remember_region(','bool remember_path(','bool graph_for(','action::Outcome perform('])
runtime=(a.root/'src/game/typed_action_runtime.cpp').read_text(encoding='utf-8')
retire=body(runtime,'    bool retire(const action::Binding&')
tail=retire[retire.index('        const auto found=operations_.find(request.action_uuid);'):]
(a.output/'remove_retire.inc').write_text('bool retire_remove(const action::Binding& binding,const action::Request& request,std::string& error) {\n'+tail,encoding='utf-8')
(a.output/'remove_operation.inc').write_text(body(runtime,'struct Operation {').rstrip()+';\n',encoding='utf-8')

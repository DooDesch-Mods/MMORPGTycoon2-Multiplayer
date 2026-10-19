"""Extract the real query/admission and Engine forwarding bodies, without source freezes."""
import argparse
import json
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--overlay',type=Path)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
def read(relative):
    file=a.source/relative
    if a.overlay and (a.overlay/relative).is_file():file=a.overlay/relative
    return file.read_text(encoding='utf-8')
def function(text,signature):
    assert text.count(signature)==1,signature
    begin=text.index(signature);start=text.index('{',begin);depth=0
    for end in range(start,len(text)):
        if text[end]=='{':depth+=1
        elif text[end]=='}':
            depth-=1
            if depth==0:return text[begin:end+1]+'\n'
    raise RuntimeError(signature)
runtime=read('src/game/typed_action_runtime.cpp')
query=function(runtime,'bool read_binding(')
admission=function(runtime,'bool network_binding(')
assert 'state.pending' not in query
assert 'state.pending' in admission and 'return read_binding(snapshot,out,error);' in admission
for needle in ('runtime->thread!=GetCurrentThreadId()', '!state.started||state.locked||runtime->lobby_retiring',
               'context!=runtime->controller.bound_context()', 'context.snapshot_uuid!=snapshot',
               'runtime->engine.current(binding,snapshot,error)', 'out=binding;error.clear();return true;'):
    assert needle in query,needle
for file in ('party_frame_native.cpp','scene_clone/runtime.cpp','dungeon_instance_runtime.cpp','dungeon_instance_native.cpp','control_input/control_runtime.cpp','character_boss_retirement_runtime.inc'):
    assert 'typed_action_runtime::read_binding(' in read('src/game/'+file),file
control=read('src/game/control_input/control_runtime.cpp')
assert 'typed_action_runtime::network_binding(' in function(control,'bool capture_acquire(')
assert 'controller.value("pending",false)' in function(control,'a::Outcome perform(')
assert 'contract::pending(' in read('src/game/character_boss_retirement_runtime.inc')
assert 'typed_action_runtime::network_binding(' in function(read('src/game/ui_build_runtime.cpp'),'bool queue_building_purchase(')
request=function(read('src/game/typed_action/action_session.cpp'),'bool Controller::request(')
assert request.index('status_.pending=true')<request.index('engine_.capture_host_authority_before')<request.index('++status_.attempted')
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'binding_queries.inc').write_text(query+admission)
(a.output/'binding_native_current.inc').write_text(function(runtime,'bool current(const action::Binding&'))
(a.output/'binding_authority_capture.inc').write_text(function(runtime,'bool capture_host_authority_before('))
(a.output/'source-check.json').write_text(json.dumps({'status':'PASS','production_bodies':4,'retained_owner_callers':6,'compiler_executed':False},indent=2)+'\n')
print('PASS: 4 actual query/current/authority-capture bodies; 6 retained-owner readers; new-action gates retained')

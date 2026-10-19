from pathlib import Path
import argparse, hashlib, json, re

p=argparse.ArgumentParser()
p.add_argument('--root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()

def body(text,anchor):
    assert text.count(anchor)==1, 'Expected one production function: '+anchor
    start=text.index(anchor)
    opening=text.index('{',start)
    depth=0
    for end in range(opening,len(text)):
        depth+=(text[end]=='{')-(text[end]=='}')
        if depth==0:return text[start:end+1]
    raise AssertionError('Unclosed production function: '+anchor)

domain=(a.root/'src/game/framework_lobby_domain.inc').read_text(encoding='utf-8')
integration=(a.root/'src/game/native_lobby_framework.inc').read_text(encoding='utf-8')
anchors=('bool open_native_new_world()', 'bool render_native_new_world(std::string& error)',
         'bool observe_native_new_world(bool title_active,bool ingame_active,std::string& error)')
functions=[body(domain,anchor) for anchor in anchors]
render=body(integration,'bool framework_render(std::string& error)')
tick=body(integration,'bool tick(std::string& error)')
calls=('render_native_new_world','observe_native_new_world')
for name in calls:
    pattern=r'\b'+name+r'\s*\('
    assert len(re.findall(pattern,render))==1, name+' must run once from framework_render'
    assert not re.search(pattern,tick), name+' must not run from Tick'
assert '"ui.new_world"' not in tick, 'Tick must not bypass the render handoff with a direct SDK call'
edge='if(!render_native_new_world(error)||!observe_native_new_world(ui_mode==2,ui_mode==3,error))return false;'
assert render.count(edge)==1, 'Render handoff edge changed; update only the affected extraction'
assert render.index(edge)<render.index('if(!framework_open&&!framework_observed_open)'), 'Passive observation must survive hidden Multiplayer panels'
a.output.mkdir(parents=True,exist_ok=True)
view=(a.root/'src/game/framework_lobby_view.inc').read_text(encoding='utf-8')
actions=(a.root/'src/game/framework_lobby_actions.inc').read_text(encoding='utf-8')
input_functions=[body(view, anchor) for anchor in (
    'bool framework_effectively_visible(', 'bool framework_reject_unattempted(', 'bool framework_apply_response(')]
input_functions.append(body(actions,'bool framework_process_action('))
assert 'if(!framework_apply_response(response,error))return false;' in render
assert 'return framework_process_action(error);' in tick
(a.output/'lobby_input_bodies.inc').write_text('\n\n'.join(input_functions)+'\n',encoding='utf-8')
(a.output/'lobby_handoff_bodies.inc').write_text('\n\n'.join(functions)+'\n',encoding='utf-8')
(a.output/'lobby_handoff_render_edge.inc').write_text(edge+'\n',encoding='utf-8')
(a.output/'source_receipt.json').write_text(json.dumps({
    'functions':{anchor:hashlib.sha256(value.encode()).hexdigest() for anchor,value in zip(anchors,functions)},
    'render_edge_sha256':hashlib.sha256(edge.encode()).hexdigest(),
    'render_calls_present_once':True,'tick_calls_absent':True,
    'whole_file_hash_guard':False,
},indent=2)+'\n',encoding='utf-8')
print('Extracted three complete production handoff functions; render/Tick callsite checks passed')

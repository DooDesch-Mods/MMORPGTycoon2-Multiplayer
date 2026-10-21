"""Extract the complete actual current Quest destination dispatcher for its fixture."""
from pathlib import Path
import argparse
import hashlib
import json

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('source',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
path=args.source/'src/game/npc_quest_target_runtime.cpp'
text=path.read_text(encoding='utf-8-sig')
def complete_function(name, source=text):
    text=source
    marker='bool '+name+'('
    if text.count(marker)!=1:raise RuntimeError('Complete Quest destination extraction anchor changed: '+name)
    begin=text.index(marker);opening=text.index('{',begin);depth=1;cursor=opening+1;quoted=False;escape=False
    while cursor<len(text) and depth:
        char=text[cursor]
        if quoted:
            if escape:escape=False
            elif char=='\\':escape=True
            elif char=='"':quoted=False
        elif char=='"':quoted=True
        elif char=='{':depth+=1
        elif char=='}':depth-=1
        cursor+=1
    if depth:raise RuntimeError('Complete Quest destination function body is unclosed: '+name)
    return text[begin:cursor]
body=complete_function('bind_scene_destination')+'\n'+complete_function('bind_current_destination')+'\n'
# Only the held-owner branch changes this complete original direct-Monster reader.
direct_source=(args.source/'src/game/npc_quest_authoring_runtime.inc').read_text(encoding='utf-8-sig')
direct=complete_function('direct_destination',direct_source)
held='        bool held=false;\n        if(!typed_action_runtime::resolve_held_dungeon_destination(pointer,key,held,e))return false;\n        if(held)return true;\n'
assert direct.count('    if(actor_shard!=p.world.root){\n'+held)==1, 'Held destination branch left its original nonroot admission'
assert hashlib.sha256(direct.replace(held,'',1).encode()).hexdigest()=='a7518608844404f8ba75684b19bd21823abdad0ca5d8e26928be1b1d57b3d0a1', 'Original direct-Monster world/full-UID/active-pool guards changed'


args.output.mkdir(parents=True,exist_ok=True)
(args.output/'dungeon_quest_destination_actual.inc').write_text(body,encoding='utf-8')
(args.output/'bodies.json').write_text(json.dumps({'dungeon_quest_destination_actual.inc':{
    'source':str(path),'sha256':hashlib.sha256(body.encode()).hexdigest()}},indent=2)+'\n')

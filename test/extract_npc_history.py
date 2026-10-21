from pathlib import Path
import argparse

p=argparse.ArgumentParser()
p.add_argument('--root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
source=(a.root/'src/game/npc_history_runtime.inc').read_text(encoding='utf-8')
anchor='bool unaffected('
assert source.count(anchor)==1
start=source.index(anchor)
end=source.index('{',start)+1
depth=1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}')
    end+=1
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'npc_history_others.inc').write_text(source[start:end]+'\n',encoding='utf-8')
assert source.count('building_history_slice::perform(request.action,request.target_action_uuid,error)')==1
assert source.index('if(!moving&&!alias(')<source.index('qr::Witness actual;json actual_refs;')
assert 'source.restore=quests;' in source

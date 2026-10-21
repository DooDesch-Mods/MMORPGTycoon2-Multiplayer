from pathlib import Path
import argparse,hashlib,json,os
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=a.root/'src/game/developer_staff/construction_native.cpp'
text=source.read_text(encoding='utf-8')

def write_generated(path, content):
    # Preserve unchanged includes so unrelated source edits do not recompile fixtures.
    data=content.replace('\n',os.linesep).encode('utf-8')
    if not path.is_file() or path.read_bytes()!=data:
        path.write_bytes(data)
def body(anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
write_generated(a.output/'loaded_actor.inc',body('struct Actor {')+';\n'+body('bool quiet(const Actor&')+'\n')
runtime=(a.root/'src/game/typed_action_runtime.cpp').read_text(encoding='utf-8')
start=runtime.index('bool enable_live_state(');end=runtime.index('bool live_state_active()',start);entry=runtime[start:end]
once=entry.index('runtime->live_attempted=true;');client=entry.index('if(context.role==action::Role::Client){');settle=entry.index('settle_loaded_client(');port=entry.index('runtime->live_port=');baseline=entry.index('runtime->day_state->bootstrap(')
assert once<client<settle<port<baseline
assert 'if(!runtime->engine.current(binding,context.snapshot_uuid,error))return false;' in entry[client:settle]
assert entry.count('settle_loaded_client(')==1
write_generated(a.output/'boundary.json',json.dumps({'construction_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'runtime_sha256':hashlib.sha256((a.root/'src/game/typed_action_runtime.cpp').read_bytes()).hexdigest(),'one_shot_client_before_baselines':True},indent=2)+'\n')

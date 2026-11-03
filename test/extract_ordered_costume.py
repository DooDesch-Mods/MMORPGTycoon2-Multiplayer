from pathlib import Path
import argparse
import hashlib
import json

p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=(a.source/'src/game/costume_edit_runtime.inc').read_text()
parent=(a.source/'src/game/character_rename_runtime.inc').read_text()
receipt={}
def function(text,anchor):
    start=text.index(anchor);at=text.index('{',start)+1;depth=1
    while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
    return text[start:at]+'\n'
def emit(name,text,origin='src/game/costume_edit_runtime.inc'):
    (a.output/name).write_text(text,encoding='utf-8',newline='\n')
    receipt[name]={'source':origin,'sha256':hashlib.sha256(text.encode()).hexdigest()}
emit('costume_types_actual.inc',function(source,'struct Pending {').removesuffix('\n')+';\n'+function(source,'struct Operation {').removesuffix('\n')+';\n')
emit('costume_record_actual.inc',source[source.index('json pose_json('):source.index('bool code(')])
emit('costume_helpers_actual.inc',source[source.index('bool same_local('):source.index('bool entry(void* receiver')])
emit('costume_capture_actual.inc',source[source.index('bool entry(void* receiver'):source.index('\n}\nbool pending_ui()')])
emit('costume_tick_actual.inc',source[source.index('void tick(){'):source.index('a::Outcome perform(')])
emit('costume_perform_actual.inc',source[source.index('a::Outcome perform('):source.index('bool observe(')])
emit('costume_observe_actual.inc',source[source.index('bool observe('):source.index('json status()')])
emit('costume_parent_capture_binding_actual.inc',function(parent,'bool capture_binding('),'src/game/character_rename_runtime.inc')
(a.output/'body-hashes.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Extracted complete Costume capture, Context, consumed execution, protected proof and observation/retirement bodies')

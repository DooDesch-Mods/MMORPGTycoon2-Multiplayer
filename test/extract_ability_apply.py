from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
runtime=(a.source/'src/game/ability_edit_runtime.inc').read_text()
native=(a.source/'src/game/ability_apply_native.inc').read_text()
rows={}
def emit(name,text,source):
    (a.output/name).write_text(text,encoding='utf-8',newline='\n')
    rows[name]={'source':source,'sha256':hashlib.sha256(text.encode()).hexdigest()}
emit('ability_apply_tails_actual.inc',native[:native.index('bool apply_route(')],'src/game/ability_apply_native.inc')
emit('ability_apply_body_actual.inc',(a.source/'src/game/ability_apply_runtime.inc').read_text(),'src/game/ability_apply_runtime.inc')
emit('ability_apply_disposal_actual.inc',native[native.index('using ApplyFindUid='):native.index('uint32_t apply_destroy(uintptr_t')],'src/game/ability_apply_native.inc')
emit('ability_proposal_actual.inc',(a.source/'src/game/ability_proposal_lifetime.inc').read_text(),'src/game/ability_proposal_lifetime.inc')
start=runtime.index('bool entry(void* receiver,void* record)try{');end=runtime.index('\n}\nbool native_apply_state',start)
emit('ability_apply_capture_actual.inc',runtime[start:end],'src/game/ability_edit_runtime.inc')
(a.output/'apply-body-hashes.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Extracted full actual Apply capture, scoped presentation entries, lease executor and cleanup bodies')

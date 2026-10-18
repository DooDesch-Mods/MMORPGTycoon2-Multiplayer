from pathlib import Path
import argparse
import hashlib
import json
p=argparse.ArgumentParser()
p.add_argument('--source-root',type=Path,required=True)
p.add_argument('--candidate',type=Path)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.candidate=a.candidate or a.source_root;a.output.mkdir(parents=True,exist_ok=True)
hashes={}
for name in ('staff_display_owner.inc','staff_display_ui.inc'):
    source=a.candidate/'src/game/tutorial_replication'/name
    data=source.read_bytes();(a.output/name).write_bytes(data)
    hashes[str(source)]=hashlib.sha256(data).hexdigest()
source=a.source_root/'src/game/tutorial_replication/startingpoint_native.inc'
text=source.read_text(encoding='utf-8');first=text.index('bool same_world(');last=text.index('#include "research_release.inc"',first)
(a.output/'same_world.inc').write_text(text[first:last],encoding='utf-8')
hashes[str(source)]=hashlib.sha256(source.read_bytes()).hexdigest()
source=a.candidate/'src/game/tutorial_replication/native.cpp'
text=source.read_text(encoding='utf-8');first=text.index('bool hooks_enabled(std::string& e){');last=text.index('bool verify_pay_entry(',first)
(a.output/'tutorial_hook_confirmation.inc').write_text(text[first:last],encoding='utf-8')
hashes[str(source)]=hashlib.sha256(source.read_bytes()).hexdigest()
source=a.candidate/'src/game/tutorial_replication/staff_display_values.inc'
text=source.read_text(encoding='utf-8');first=text.index('bool code_result(');last=text.index('DWORD native_destroy(',first)
(a.output/'code_result.inc').write_text(text[first:last],encoding='utf-8')
hashes[str(source)]=hashlib.sha256(source.read_bytes()).hexdigest()
source=a.candidate/'src/game/tutorial_replication/staff_display_hook.inc'
text=source.read_text(encoding='utf-8');first=text.index('// Hidden windows')
(a.output/'posthook.inc').write_text('namespace display_native {\n'+text[first:],encoding='utf-8')
hashes[str(source)]=hashlib.sha256(source.read_bytes()).hexdigest()
(a.output/'inputs.json').write_text(json.dumps(hashes,indent=2)+'\n',encoding='utf-8')

"""Extract actual Root/Scene Scenery companion admission bodies for the existing alias target."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=a.source/'src/game/scenery_scene_identity.inc';s=source.read_text(encoding='utf-8');start=s.index('bool registered(');end=s.index('bool owner(',start)
body=s[start:end]
assert body.count('bool registered(')==1 and body.count('bool prepare_saved(')==1 and body.count('bool capture_saved(')==1 and body.count('bool append_saved(')==1
assert 'root_initialized' in body and 'complete.root_present' in body
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'root_scenery_admission_body.inc').write_text(body,encoding='utf-8',newline='\n')
(a.output/'root_scenery_admission_body.json').write_text(json.dumps(dict(source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),body_sha256=hashlib.sha256(body.encode()).hexdigest(),functions=['registered','prepare_saved','capture_saved','append_saved']),indent=2)+'\n',encoding='utf-8')
print('PASS EXTRACTED4 actual saved Scenery admission/capture bodies')

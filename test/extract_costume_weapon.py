from pathlib import Path
import argparse
import hashlib
import json

p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=a.source/'src/game/costume_edit_runtime.inc';text=source.read_text(encoding='utf-8')
start=text.index('bool entry(void* receiver,void* record)try{')
end=text.index('\n}\nbool pending_ui()',start)
body=text[start:end]
assert body.endswith('return reject("Costume capture non-standard exception");}')
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'costume_weapon_capture_body.inc').write_text(body+'\n',encoding='utf-8')
(a.output/'inputs.json').write_text(json.dumps({'source':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()},indent=2)+'\n')
print('Extracted complete actual Costume entry including original exception boundaries')

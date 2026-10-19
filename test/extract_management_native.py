from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=a.source_root/'src/game/management_live_port.inc';text=source.read_text()
start='uintptr_t scalar_address(';end='uint32_t slots_once('
assert text.count(start)==1 and text.count(end)==1,'Management actual native body boundary changed'
body=text[text.index(start):text.index(end)]
a.output.mkdir(parents=True,exist_ok=True);target=a.output/'management_scalar_body.inc';target.write_text(body)
(a.output/'management_scalar_body.json').write_text(json.dumps({'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'extracted_sha256':hashlib.sha256(target.read_bytes()).hexdigest()},indent=2)+'\n')

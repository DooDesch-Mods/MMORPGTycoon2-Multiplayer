from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=a.source_root/'src/game/general_project_runtime.cpp';text=source.read_text(encoding='utf-8')
parts=[]
for start,end in [('bool original_route(','bool capture_ui('),('bool apply_progress(',None)]:
 assert text.count(start)==1,start
 body=text[text.index(start):text.index(end)] if end else text[text.index(start):text.rfind('\n}')]
 parts.append(body)
a.output.mkdir(parents=True,exist_ok=True);target=a.output/'general_project_actual.inc';target.write_text('\n'.join(parts),encoding='utf-8')
(a.output/'general_project_actual.json').write_text(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'actual_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'functions':['original_route','admit_bug_birth','apply_progress']},indent=2)+'\n',encoding='utf-8')

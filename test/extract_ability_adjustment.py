from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
path=a.source/'src/game/ability_adjustment_runtime.inc';text=path.read_text(encoding='utf-8')
rows=[]
for start,name in [('bool capture_adjustment_impl(','ability_adjustment_capture_body.inc'),('void view_select_entry(','ability_adjustment_tail_body.inc'),('json record_json(','ability_adjustment_record_body.inc')]:
    if start.startswith('json '):text=(a.source/'src/game/ability_edit_runtime.inc').read_text(encoding='utf-8')
    assert text.count(start)==1;begin=text.index(start);end=text.index('{',begin)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    body=text[begin:end]+'\n';(a.output/name).write_text(body,encoding='utf-8');rows.append({'function':start,'sha256':hashlib.sha256(body.encode()).hexdigest()})
(a.output/'receipt.json').write_text(json.dumps({'input_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bodies':rows},indent=2)+'\n',encoding='utf-8');print('Extracted three complete actual Ability adjustment record/capture/presentation bodies')

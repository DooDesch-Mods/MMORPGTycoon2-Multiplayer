"""Read retained native Move evidence without replay."""
import argparse
import json
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('summary',type=Path)
args=parser.parse_args(); summary=json.loads(args.summary.read_text(encoding='utf-8'))
for check in summary['checks']:
    result=check.get('result',{})
    if 'immutable-request' in check['check']: print(json.dumps(check,ensure_ascii=False))
    if isinstance(result,dict) and 'ui_move' in result:
        print(json.dumps({'check':check['check'],'controller':result['typed_actions'],'move':result['ui_move'],'props':result['props'],'cash':result.get('simulation',{}).get('cash')},ensure_ascii=False))
for job in summary['jobs']:
    if 'observe' not in job['step']: continue
    evidence=json.loads(Path(job['evidence_path']).read_text(encoding='utf-8'))
    for call in evidence['calls']:
        if call['tool']=='game_inspect_engine':
            result=call['result']['structuredContent']['result']
            print(json.dumps({'step':job['step'],'controller':result['typed_actions'],'move':result['ui_move'],'props':result['props']},ensure_ascii=False))

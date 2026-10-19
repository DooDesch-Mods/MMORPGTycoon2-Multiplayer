"""Print affected native receipts and media from one retained acceptance run."""
import json
from pathlib import Path
import sys

summary=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
for job in summary['jobs']:
    path=job.get('evidence_path')
    if not path: continue
    evidence=json.loads(Path(path).read_text(encoding='utf-8'))
    for call in evidence['calls']:
        if call['tool']=='bind_runtime': continue
        outer=call.get('result',{}).get('structuredContent',{})
        result=outer.get('result',{})
        keys=('path','snapshot_id','network_identity','snapshot','snapshot_progress','typed_actions','controller','lobby','prepare')
        print(json.dumps({'step':job['step'],'tool':call['tool'],'ok':outer.get('ok'),
            'result':{key:result[key] for key in keys if key in result},'available_fields':list(result)},ensure_ascii=False))

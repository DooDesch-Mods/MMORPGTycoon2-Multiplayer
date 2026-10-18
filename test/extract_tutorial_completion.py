"""Extract actual Completion hook and execution bodies with controlled test ports."""
import argparse
import hashlib
import json
from pathlib import Path

def part(text,begin,end):
    if text.count(begin)!=1 or text.count(end)!=1:
        raise SystemExit('tutorial Completion extraction boundary changed: '+begin)
    return text[text.index(begin):text.index(end)]

def extract(root,output):
    folder=root/'src/game/tutorial_replication'
    paths={name:folder/name for name in ('native.cpp','lifecycle_owner.inc','lifecycle_hook.inc','lifecycle_apply.inc')}
    source={name:path.read_text(encoding='utf-8-sig') for name,path in paths.items()}
    native=source['native.cpp'];hook=source['lifecycle_hook.inc'];apply=source['lifecycle_apply.inc']
    bodies={
        'lifecycle_cache.inc':part(source['lifecycle_owner.inc'],'struct Cache {','bool array('),
        'lifecycle_advance.inc':part(hook,'void advance(void* value){','void scenario_start('),
        'lifecycle_start.inc':hook[hook.index('void scenario_start('):hook.rfind('\n}')],
        'lifecycle_update.inc':part(native,'void sequence_update(','void postload('),
        'lifecycle_pin.inc':part(apply,'bool pin(','bool create('),
        'lifecycle_execute.inc':part(apply,'bool create(','bool prepare('),
        'lifecycle_progress_only.inc':part(native,'static bool research_progress_only(', 'static bool preflight_local('),
        'lifecycle_preflight.inc':part(native,'static bool preflight_local(', 'bool apply(const action_session::Context&'),
        'lifecycle_outer_apply.inc':part(native,'bool apply(const action_session::Context&','\n#include "startingpoint_commit_intent.inc"'),
    }
    output.mkdir(parents=True,exist_ok=True)
    for name,body in bodies.items():
        target=output/name;data=body.encode()
        if not target.exists() or target.read_bytes()!=data:target.write_bytes(data)
    (output/'completion_extraction.json').write_text(json.dumps({str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths.values()},indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source-root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();extract(args.source_root,args.output)

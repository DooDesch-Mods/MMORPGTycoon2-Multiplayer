"""Compare preserved MCP observations without changing the game."""
from pathlib import Path
import argparse
import hashlib
import json

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host',type=Path,required=True)
parser.add_argument('--client',type=Path,required=True)
parser.add_argument('--save',type=Path,required=True)
args=parser.parse_args()
def result(path,tool):
    data=json.loads(path.read_text(encoding='utf-8'))
    if data.get('passed') is not True:raise ValueError('MCP observation failed: '+str(path))
    matches=[c for c in data['calls'] if c['tool']==tool]
    if len(matches)!=1 or matches[0]['state']!='confirmed':raise ValueError('MCP observation missing: '+tool)
    response=matches[0]['result']['structuredContent']
    if response.get('ok') is not True:raise ValueError('native response failed: '+tool)
    return response['result']
host=result(args.host,'game_inspect_engine');client=result(args.client,'game_inspect_engine')
save=result(args.save,'game_capture_local_snapshot')
report={'host_evidence':str(args.host),'client_evidence':str(args.client),'save_evidence':str(args.save),
        'host_keys':list(host),'save':save,'observations':{}}
for role,engine in [('host',host),('client-1',client)]:
    props=engine.get('props',[])
    report['observations'][role]={k:engine.get(k) for k in ('world','clock','simulation','typed_actions','ui_remove')}
    report['observations'][role]['props']=props
    report['observations'][role]['prop_count']=len(props)
path=ROOT/'artifacts/user_world_checkpoint'
path.mkdir(parents=True,exist_ok=True)
out=path/'comparison.json'
out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'comparison':str(out),'save':save,'observations':{
    role:{k:v for k,v in observed.items() if k!='props'} for role,observed in report['observations'].items()}},ensure_ascii=False))

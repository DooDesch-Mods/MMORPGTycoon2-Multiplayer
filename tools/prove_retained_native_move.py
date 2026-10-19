"""Validate the already completed Client Move from immutable MCP evidence."""
import json
from pathlib import Path
from local_acceptance import ROOT, compare_ledgers
source=ROOT/'artifacts/acceptance/afdafb88-4cae-49ac-a560-629b4e71a05b/summary.json'
summary=json.loads(source.read_text(encoding='utf-8'))
if summary.get('passed') is not False or summary.get('error',{}).get('cause')!='Move pose differs: host:y': raise RuntimeError('retained diagnostic failure differs')
def engine(step):
    job=next(job for job in summary['jobs'] if job['step']==step)
    evidence=json.loads(Path(job['evidence_path']).read_text(encoding='utf-8'))
    if evidence.get('passed') is not True: raise RuntimeError('retained read job failed: '+step)
    return next(call['result']['structuredContent']['result'] for call in evidence['calls'] if call['tool']=='game_inspect_engine')
request=next(check['result'] for check in summary['checks'] if check['check']=='client-1-immutable-request')
before={peer:engine('client-1-before-'+peer) for peer in ('host','client-1')}
after={peer:engine('client-1-observe-'+peer) for peer in ('host','client-1')}
ledger=compare_ledgers(before['host'],before['client-1'])
if compare_ledgers(after['host'],after['client-1'])!=ledger: raise RuntimeError('retained Move ledger differs')
objects={}; receipts={}
for peer,state in after.items():
    status=state['typed_actions']; receipt=state['ui_move']['last_native']; receipts[peer]=receipt
    if status.get('action_uuid')!=request['action_uuid'] or status.get('confirmed')!=1 or status.get('attempted')!=1 or status.get('pending') or status.get('locked') or status.get('outcome')!=1 or status.get('failed')!=0: raise RuntimeError('retained exact completion differs: '+peer)
    for field in ('confirmed','undo_confirmed','selection_confirmed','cleanup_confirmed','move_attempted','move_returned','ctor_returned','dtor_returned','loc_returned','begin_returned','open_returned','close_returned','end_returned','vector_returned','string_returned'):
        if receipt.get(field) is not True: raise RuntimeError('retained native receipt differs: '+peer+':'+field)
    if receipt.get('token')!=request['action_uuid'] or receipt.get('record_key')!=request['record_key'] or receipt.get('exception') or receipt.get('cleanup_exception'): raise RuntimeError('retained native identity/exception differs')
    old=next(prop for prop in before[peer]['props'] if prop['uid']==receipt['native_uid'])
    objects[peer]=next(prop for prop in state['props'] if prop['uid']==receipt['native_uid'])
    if old['uid']!=objects[peer]['uid']: raise RuntimeError('retained native UID changed')
    for field in ('type','variant','x','z'):
        if objects[peer][field]!=request['building'][field]: raise RuntimeError('retained native pose/type differs: '+field)
if after['host']['typed_actions']['ack_mask']!=3 or objects['host']['y']!=objects['client-1']['y']: raise RuntimeError('retained Host ACK or settled height differs')
target=ROOT/'artifacts/move_retained/client_20261012/summary.json'; target.parent.mkdir(parents=True,exist_ok=True)
target.write_text(json.dumps({'passed':True,'scope':'original Client Move confirmed on both peers; Host-origin and normal UI Move not claimed','runtime_run_id':summary['runtime_run_id'],'source':str(source),'request':request,'objects':objects,'receipts':receipts,'ledger':ledger,'host_ack_mask':3,'replayed':False},indent=2)+'\n',encoding='utf-8')
print(str(target))

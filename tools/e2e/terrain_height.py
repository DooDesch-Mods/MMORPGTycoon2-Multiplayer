"""Prove Client up and Host down through original Height Press/Drag/Release."""
from __future__ import annotations
import argparse
import math
from pathlib import Path
import re
import struct
import sys
import time
import uuid
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from local_acceptance import AcceptanceError,AcceptanceRunner,compare_ledgers,extract_result,log,utc
from tools.e2e.run import PEERS,preflight,shared_ready
from tools.e2e.subscriber_additions import check_executable_identity

CONTENT=('id','preset','styles','scenery_count','scenery_sha256','grid_sha256','contours_sha256','bugs_sha256','bug_count')
def digest(value):return isinstance(value,str) and re.fullmatch(r'[0-9a-f]{64}',value) is not None
def rng(value):return isinstance(value,list) and len(value)==4 and all(type(word) is int and 0<=word<=18446744073709551615 for word in value)
def admission(job,fields):
    if any(job.get(k)!=v for k,v in fields.items()) or job.get('attempted')!=3 or job.get('returned')!=3 or any(job.get(k) is not True for k in ('selection_confirmed','input_restored','captured','drag_captured')) or job.get('cause'):
        raise AcceptanceError('Height original Activate/Press/virtual Drag/Release capture/readback differs; no replay')
    if not digest(job.get('before_sha256')) or type(job.get('before_bits')) is not int:
        raise AcceptanceError('Height original native grid/cell baseline absent; no replay')
def baseline(args,engines,grids):
    shared_ready(engines);cash=compare_ledgers(engines['host'],engines['client-1'])
    if cash['cash']<100:raise AcceptanceError('Height requires native cash>=100')
    for peer in PEERS:
        ui=engines[peer].get('ui_terrain',{})
        if engines[peer].get('capabilities',{}).get('native_terrain_height') is not True or ui.get('enabled') is not True or ui.get('pending') is not False or ui.get('capturing') is not False:
            raise AcceptanceError(peer+' Height native UI unavailable/pending')
        if grids[peer].get('id')!=args.region_id or not digest(grids[peer].get('heights_sha256')) or not rng(grids[peer].get('random')) or any(k not in grids[peer] for k in CONTENT):
            raise AcceptanceError(peer+' Height native grid/Region baseline incomplete')
    if any(grids['host'][k]!=grids['client-1'][k] for k in (*CONTENT,'heights_sha256','release_edits')) or engines['host']['typed_actions']['host_revision']!=engines['client-1']['typed_actions']['host_revision']:
        raise AcceptanceError('Height common native grid/Region/revision baseline differs')
    return cash
def expense(before,after):
    wanted={**before,'cash':before['cash']-100,'record_cash':before['record_cash']-100,'record_expense':before['record_expense']+100,'line_counts':list(before['line_counts']),'line_income':list(before['line_income'])}
    wanted['line_counts'][6]+=1;wanted['line_income'][6]-=100
    if after!=wanted:raise AcceptanceError('Height full 20-category SubtractCash100/category6 Ledger differs')
def healthy(before,after):
    for peer in PEERS:
        old,new=before[peer],after[peer];status=new['typed_actions'];ui=new['ui_terrain']
        if status.get('locked') is True or status.get('outcome')==3 or status.get('failed')!=old['typed_actions']['failed'] or ui.get('rejected')!=old['ui_terrain']['rejected']:
            raise AcceptanceError(f"{peer} Height failed/Unknown controller={status.get('cause')} UI={ui.get('cause')}; no replay")
def confirmed(args,before,grids_before,cash_before,after,grids_after,origin,fields,action):
    shared_ready(after);healthy(before,after);cash_after=compare_ledgers(after['host'],after['client-1']);expense(cash_before,cash_after)
    rows={}
    for peer in PEERS:
        old,new=before[peer],after[peer];controller=new['typed_actions'];ui=new['ui_terrain'];prior=old['typed_actions']
        if controller.get('action_uuid')!=action or controller.get('outcome')!=1 or controller.get('pending') is not False or any(controller.get(k)!=prior[k]+1 for k in ('attempted','confirmed','host_revision')):
            raise AcceptanceError(peer+' Height exact-once Controller/revision differs')
        if any(ui.get(k)!=old['ui_terrain'][k]+int(peer==origin) for k in ('captured','submitted')):
            raise AcceptanceError(peer+' Height original capture/submission count differs')
        found=[row for row in ui.get('operations',[]) if row.get('action_uuid')==action]
        if len(found)!=1 or any(found[0].get(k) is not True for k in ('attempted','returned','confirmed','retired','undo_confirmed')) or found[0].get('native_cost')!=100 or found[0].get('cause') or found[0].get('removed_scenery_uids')!=[] or found[0].get('retired_keys')!=[]:
            raise AcceptanceError(peer+' Height original operation/retirement/unchanged Undo differs')
        row=found[0];height=row.get('height',{})
        if height.get('cell')!=fields['cell'] or height.get('up')!=fields['up'] or height.get('undo_unchanged') is not True or height.get('before_sha256')!=grids_before[peer]['heights_sha256'] or height.get('after_sha256')!=grids_after[peer]['heights_sha256'] or height.get('before_sha256')==height.get('after_sha256'):
            raise AcceptanceError(peer+' Height original full-grid digest/cell differs')
        try:
            bits=height['before_bits'];value=struct.unpack('<f',struct.pack('<I',bits))[0];after_bits=struct.unpack('<I',struct.pack('<f',value+(20 if fields['up'] else -20)))[0]
        except (KeyError,TypeError,OverflowError,struct.error) as error:raise AcceptanceError('Height original float32 cell receipt invalid') from error
        if not math.isfinite(value) or height.get('after_bits')!=after_bits:raise AcceptanceError(peer+' Height original selected cell +/-20 differs')
        if any(grids_before[peer][k]!=grids_after[peer][k] for k in CONTENT):raise AcceptanceError(peer+' Height changed unrelated Region/contours/scenery/bugs')
        # Client Original receives the Host Region RNG before it executes.
        if not rng(grids_after[peer].get('random')) or grids_after[peer]['random']!=grids_before['host']['random']:
            raise AcceptanceError(peer+' Height Region RNG differs from unchanged Host authority')
        for suffix,grid in (('before',grids_before[peer]),('after',grids_after[peer])):
            if row.get('release_'+suffix)!=grid['release_edits'] or row.get('bugs_'+suffix)!=grid['bug_count']:
                raise AcceptanceError(peer+' Height release/bug operation readback differs')
        retained=[item for item in grids_after[peer].get('ui_terrain',{}).get('operations',[]) if item.get('action_uuid')==action]
        if retained!=[row]:raise AcceptanceError(peer+' Height original receipt changed during readback')
        rows[peer]=row
    if after['host']['typed_actions'].get('ack_mask')!=3:raise AcceptanceError('Height Host lacks ACK3')
    job=after[origin]['ui_terrain'].get('ui_jobs',{}).get(fields['token'],{});admission(job,fields)
    if job.get('submitted') is not True or job.get('confirmed') is not True or job.get('action_uuid')!=action or job['before_sha256']!=grids_before[origin]['heights_sha256'] or job['before_bits']!=rows[origin]['height']['before_bits']:
        raise AcceptanceError('Height UI token/native cell/RequestUUID lacks exact Host completion')
    if rows['host']['height']!=rows['client-1']['height'] or any(rows['host'].get(k)!=rows['client-1'].get(k) for k in ('release_before','release_after','bugs_before','bugs_after')) or any(grids_after['host'][k]!=grids_after['client-1'][k] for k in (*CONTENT,'random','heights_sha256','release_edits')):
        raise AcceptanceError('Height Host/Client original effect or complete grid readback differs')
    return {'action_uuid':action,'origin':origin,'up':fields['up'],'cell':fields['cell'],'host_ack_mask':3,'operations':rows,'ledger_before':cash_before,'ledger_after':cash_after,'height_before':grids_before['host']['heights_sha256'],'height_after':grids_after['host']['heights_sha256'],'region_rng_authority':{'host_before':grids_before['host']['random'],'local_before':{peer:grids_before[peer]['random'] for peer in PEERS},'after':{peer:grids_after[peer]['random'] for peer in PEERS}},'global_rng_observations':{peer:grids_after[peer].get('global_random') for peer in PEERS}}
def execute(args,runner_factory=AcceptanceRunner,now=time.monotonic):
    args.test_root,args.players,args.cleanup,args.step_timeout=ROOT/'_MPTest',2,False,45;runner=runner_factory(args)
    runner.summary['scope']='two original Height tool inputs: Client up and Host down, exact grid +/-20, full Ledger100/category6 each, ACK3; no mutation replay';runner.summary['height_inputs']=[]
    try:
        preflight(runner,args.freeze,args.setup);check_executable_identity(runner)
        results=[]
        for index,(origin,up) in enumerate((('client-1',True),('host',False)),1):
            prefix=f'height-{index}';before={peer:runner.inspect(prefix+'-before-'+peer,peer) for peer in PEERS};grids={peer:runner.single(prefix+'-grid-before-'+peer,peer,'game_read_terrain',{'id':args.region_id}) for peer in PEERS};cash=baseline(args,before,grids)
            runner.checked(prefix+'-common-baseline',{'engines':before,'grids':grids,'ledger':cash});fields={'token':str(uuid.uuid4()),'id':args.region_id,'cell':args.cell,'up':up,'radius':args.radius}
            record={'state':'attempted','origin':origin,'fields':fields};runner.summary['height_inputs'].append(record);runner.persist()
            admitted=runner.single(prefix+'-native-once',origin,'game_native_terrain_height',fields);admission(admitted,fields);record.update(state='captured-before-host',admission=admitted);runner.persist()
            deadline,poll,action=now()+45,0,None
            while True:
                poll+=1;after={}
                for peer in PEERS:
                    remaining=deadline-now()
                    if remaining<=2:raise AcceptanceError(f'Height observation timeout required<45s input={index} action={action}; no replay')
                    receipt=runner.job(f'{prefix}-observe-{poll}-{peer}',peer,[{'tool':'game_inspect_engine','arguments':{}}],remaining=remaining);after[peer]=extract_result(receipt,'game_inspect_engine')
                healthy(before,after);job=after[origin]['ui_terrain'].get('ui_jobs',{}).get(fields['token'],{})
                if job.get('cause'):raise AcceptanceError('Height UI failed: '+job['cause']+'; no replay')
                action=job.get('action_uuid')
                if action and job.get('confirmed') is True and all(after[p]['typed_actions'].get('pending') is False for p in PEERS):
                    if str(uuid.UUID(action))!=action:raise AcceptanceError('Height completion ActionUUID noncanonical; no replay')
                    break
            final={peer:runner.single(prefix+'-grid-after-'+peer,peer,'game_read_terrain',{'id':args.region_id}) for peer in PEERS};result=confirmed(args,before,grids,cash,after,final,origin,fields,action);results.append(result);record.update(state='confirmed',action_uuid=action);runner.checked(prefix+'-confirmed',{'result':result,'engines':after,'grids':final})
        runner.summary.update(passed=True,state='confirmed-native-height-two-inputs-two-peers',results=results)
    except Exception as error:
        runner.summary.update(passed=False,state='failed-native-height-two-peers',error={'step':runner.summary['step'],'cause':f'{type(error).__name__}: {error}','behavior':'two intended inputs maximum; next input only after exact prior confirmation; effects retained; no retry after failure/timeout/Unknown'});log('error',str(error))
    runner.summary['finished_utc']=utc();runner.persist();print(runner.summary_path);return 0 if runner.summary['passed'] else 1
def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--setup',type=Path,required=True);parser.add_argument('--freeze',type=Path,required=True);parser.add_argument('--region-id',type=int,required=True);parser.add_argument('--cell',type=int,required=True);parser.add_argument('--radius',type=float,default=32);args=parser.parse_args(argv)
    if not 0<=args.region_id<=2147483647 or not 0<=args.cell<1000000 or not math.isfinite(args.radius) or not 1<=args.radius<=1024:parser.error('region-id int32, cell0..999999, finite radius1..1024 required')
    return execute(args)
if __name__=='__main__':raise SystemExit(main())

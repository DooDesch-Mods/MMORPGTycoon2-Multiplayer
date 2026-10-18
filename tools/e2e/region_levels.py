"""Prove one actual Client SetRegionLevel toolbar/range/Press/Drag/Release action."""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import sys
import time
import uuid
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from local_acceptance import AcceptanceError,AcceptanceRunner,compare_ledgers,extract_result,log,utc
from tools.e2e.run import PEERS,preflight,shared_ready
from tools.e2e.region_properties import REGION_FIELDS,region_state,require_healthy_observation
from tools.e2e.subscriber_additions import check_executable_identity

def digest(value):return isinstance(value,str) and re.fullmatch(r'[0-9a-f]{64}',value) is not None
def rng(value):return isinstance(value,list) and len(value)==4 and all(type(x) is int and 0<=x<=18446744073709551615 for x in value)
def actors(value):
    if not isinstance(value,dict) or set(value)!={'counts','roster_sha256','levels_sha256','expected_sha256','global_random'} or not isinstance(value['counts'],list) or len(value['counts'])!=3 or any(type(n) is not int or not 0<=n<=100000 for n in value['counts']) or any(not digest(value[key]) for key in ('roster_sha256','levels_sha256','expected_sha256')) or not rng(value['global_random']):
        raise AcceptanceError('Region levels native actor census/RNG absent or malformed')
    return value
def actor_basis(value):
    value=actors(value);return {key:value[key] for key in ('counts','roster_sha256','levels_sha256')}
def baseline(args,engines,regions):
    shared_ready(engines);cash=compare_ledgers(engines['host'],engines['client-1']);states={peer:region_state(regions[peer]) for peer in PEERS}
    comparable=lambda value:{key:value[key] for key in REGION_FIELDS if key not in ('random','type_random')}
    if comparable(states['host'])!=comparable(states['client-1']) or states['host']['id']!=args.region_id or states['host']['owned'] is not True or [states['host']['level'],states['host']['max_level']]==[args.min_level,args.max_level]:
        raise AcceptanceError('Region levels common owned changed native Region baseline differs')
    for peer in PEERS:
        ui=engines[peer].get('ui_region',{})
        if engines[peer].get('capabilities',{}).get('native_region_property_ui') is not True or ui.get('enabled') is not True or ui.get('pending_ui') is not False or ui.get('property_cause'):
            raise AcceptanceError(peer+' Region level native capture not idle/healthy')
        if regions[peer].get('position_resolves_region') is not True:raise AcceptanceError(peer+' Region level native center identity differs')
    if actor_basis(regions['host'].get('actors'))!=actor_basis(regions['client-1'].get('actors')) or engines['host']['typed_actions']['host_revision']!=engines['client-1']['typed_actions']['host_revision']:
        raise AcceptanceError('Region levels common canonical actor roster/levels/revision differs')
    return cash
def admission(value):
    if value.get('attempted')!=4 or value.get('returned')!=4 or any(value.get(key) is not True for key in ('captured_once','input_restored','local_state_unchanged')):
        raise AcceptanceError('Region level actual Activate/range/Press-Drag/Release admission differs; no replay')
def confirmed(args,before,regions_before,cash,after,regions_after,action):
    shared_ready(after);result_cash=compare_ledgers(after['host'],after['client-1'])
    if result_cash!=cash:raise AcceptanceError('Region levels full20 Ledger changed')
    rows={};after_states={peer:region_state(regions_after[peer]) for peer in PEERS}
    if after_states['host']!=after_states['client-1']:raise AcceptanceError('Region level final Region or regional RNG differs')
    for peer in PEERS:
        require_healthy_observation(before,after,peer);old,new=before[peer],after[peer];controller=new['typed_actions'];prior=old['typed_actions'];ui=new['ui_region']
        if controller.get('action_uuid')!=action or controller.get('pending') is not False or controller.get('outcome')!=1 or any(controller.get(key)!=prior[key]+1 for key in ('attempted','confirmed','host_revision')):
            raise AcceptanceError(peer+' Region level exact-once revision/confirmation differs')
        if any(ui.get(key)!=old['ui_region'][key]+int(peer=='client-1') for key in ('property_captured','property_submitted')):raise AcceptanceError(peer+' Region level original capture/submission count differs')
        found=[row for row in ui.get('property_operations',[]) if row.get('action_uuid')==action]
        if len(found)!=1 or found[0].get('kind')!=4 or any(found[0].get(key) is not True for key in ('attempted','returned','confirmed','retired','undo_confirmed','level_constructed','level_destroyed')) or not found[0].get('level_graph') or not found[0].get('level_brush') or found[0].get('cause') or found[0].get('removed_uids')!=[] or found[0].get('retired_shared_keys')!=[]:
            raise AcceptanceError(peer+' Region level original/Undo/retirement receipt differs')
        row=found[0];first,last=actors(row.get('level_before')),actors(row.get('level_after'))
        if actor_basis(first)!=actor_basis(regions_before[peer].get('actors')) or actor_basis(last)!=actor_basis(regions_after[peer].get('actors')) or first['counts']!=last['counts'] or first['roster_sha256']!=last['roster_sha256'] or first['expected_sha256']!=last['levels_sha256'] or last['expected_sha256']!=last['levels_sha256']:
            raise AcceptanceError(peer+' Region level original Actor/Quest clamp or roster/counts differs')
        expected=region_state(regions_before[peer]);expected.update(level=args.min_level,max_level=args.max_level,random=row.get('after_random'),type_random=row.get('after_type_random'))
        if row.get('before_levels')!=[regions_before[peer]['level'],regions_before[peer]['max_level']] or row.get('after_levels')!=[args.min_level,args.max_level] or after_states[peer]!=expected:
            raise AcceptanceError(peer+' Region level requested pair changed unrelated Region state')
        if not rng(row.get('before_random')) or not rng(row.get('after_random')) or not rng(row.get('before_type_random')) or not rng(row.get('after_type_random')):raise AcceptanceError(peer+' Region level synchronous regional RNG absent')
        rows[peer]=row
    for key in ('level_before','level_after','before_levels','after_levels','before_random','after_random','before_type_random','after_type_random','before_cash','after_cash','retired_keys_sha256'):
        if rows['host'].get(key)!=rows['client-1'].get(key):raise AcceptanceError('Region level synchronous original receipts differ: '+key)
    if after['host']['typed_actions'].get('ack_mask')!=3:raise AcceptanceError('Region level Host lacks ACK3')
    return {'action_uuid':action,'host_ack_mask':3,'ledger_before':cash,'ledger_after':result_cash,'operations':rows,'region_after':after_states['host'],'global_rng_observations':{peer:regions_after[peer]['actors']['global_random'] for peer in PEERS}}
def execute(args,runner_factory=AcceptanceRunner,now=time.monotonic):
    args.test_root=getattr(args,'test_root',ROOT/'_MPTest')
    args.players,args.cleanup,args.step_timeout=2,False,45;runner=runner_factory(args)
    runner.summary['scope']='one Client actual SetRegionLevel input, native original on Host and Client, canonical actor/quest levels, exact Undo, full20 Ledger, synchronous RNG and ACK3; no replay'
    try:
        preflight(runner,args.freeze,args.setup,candidate_binary=getattr(args,'candidate_binary',None));check_executable_identity(runner)
        before={peer:runner.inspect('region-level-before-'+peer,peer) for peer in PEERS};regions={peer:runner.single('region-level-read-before-'+peer,peer,'game_read_region_preset',{'region_id':args.region_id}) for peer in PEERS};cash=baseline(args,before,regions)
        fields={'diagnostic_uuid':str(uuid.uuid4()),'region_id':args.region_id,'kind':'levels','name':'','type':-1,'hit':regions['client-1']['region_position'],'levels':[args.min_level,args.max_level]}
        runner.checked('region-level-common-baseline',{'engines':before,'regions':regions,'ledger':cash,'fields':fields});runner.summary['single_intent']={'state':'attempted','fields':fields};runner.persist()
        admitted=runner.single('region-level-native-once','client-1','game_native_region_property_ui',fields);admission(admitted);runner.summary['single_intent'].update(state='captured-before-host',admission=admitted);runner.persist()
        deadline,poll,action=now()+45,0,None
        while True:
            poll+=1;after={}
            for peer in PEERS:
                remaining=deadline-now()
                if remaining<=2:raise AcceptanceError('Region level observation timeout action='+str(action)+'; no replay')
                job=runner.job(f'region-level-observe-{poll}-{peer}',peer,[{'tool':'game_inspect_engine','arguments':{}}],remaining=remaining);after[peer]=extract_result(job,'game_inspect_engine');require_healthy_observation(before,after,peer)
            if after['client-1']['ui_region'].get('property_submitted')==before['client-1']['ui_region']['property_submitted']+1:action=after['client-1']['ui_region'].get('property_last_action_uuid')
            if action and all(after[peer]['typed_actions'].get('action_uuid')==action and after[peer]['typed_actions'].get('pending') is False and after[peer]['typed_actions'].get('outcome')==1 for peer in PEERS):break
        final={peer:runner.single('region-level-read-after-'+peer,peer,'game_read_region_preset',{'region_id':args.region_id}) for peer in PEERS};result=confirmed(args,before,regions,cash,after,final,action);runner.checked('region-level-confirmed',{'result':result,'engines':after,'regions':final});runner.summary.update(passed=True,state='confirmed-native-region-level-two-peers',result=result);runner.summary['single_intent'].update(state='confirmed',action_uuid=action)
    except Exception as error:
        runner.summary.update(passed=False,state='failed-native-region-level-two-peers',error={'step':runner.summary['step'],'cause':f'{type(error).__name__}: {error}','behavior':'exactly one intended Client input maximum; no mutation replay after failure/timeout/Unknown'});log('error',str(error))
    runner.summary['finished_utc']=utc();runner.persist();print(runner.summary_path);return 0 if runner.summary['passed'] else 1
def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--setup',type=Path,required=True);parser.add_argument('--freeze',type=Path,required=True);parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest');parser.add_argument('--region-id',type=int,required=True);parser.add_argument('--min-level',type=int,required=True);parser.add_argument('--max-level',type=int,required=True)
    parser.add_argument('--candidate-binary',type=Path,help='Exact selected mod artifact; required for Framework plugin mode')
    args=parser.parse_args(argv)
    if not 0<=args.region_id<=2147483647 or not 1<=args.min_level<=args.max_level<=214748354:parser.error('region-id int32 and ordered min/max levels in safe native integer domain required')
    return execute(args)
if __name__=='__main__':raise SystemExit(main())

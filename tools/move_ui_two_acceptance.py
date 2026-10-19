"""Prove each player's normal native BuildingInfoWindow Move and cursor route."""
import argparse
import math
import time
import uuid
from pathlib import Path
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, compare_ledgers, utc, log, hash_file
from runtime import Runtime, FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind, candidate_binary_receipt

PEERS=('host','client-1')

def parse_args(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--direct-move',type=Path)
    mode.add_argument('--setup',type=Path)
    mode.add_argument('--placement',type=Path)
    parser.add_argument('--freeze',type=Path,required=True)
    parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest')
    parser.add_argument('--candidate-binary',type=Path,help='Exact selected mod artifact; required for Framework plugin mode')
    parser.add_argument('--offset-x',type=float,default=40)
    parser.add_argument('--actor',choices=('both','host','client-1'),default='both')
    return parser.parse_args(argv)

def target_rows(bindings,key):
    rows={}
    for peer in PEERS:
        matches=[row for row in bindings[peer].get('bindings',[]) if row.get('record_key')==key]
        if len(matches)!=1: raise AcceptanceError('Move UI target binding count differs: '+peer+': actual='+str(len(matches))+' required=1')
        row=matches[0]
        if row.get('revoked') is not False or row.get('unknown') is not False: raise AcceptanceError('Move UI target binding is revoked or Unknown: '+peer)
        if type(row.get('local_uid')) is not int or row['local_uid']<=0: raise AcceptanceError('Move UI target local UID invalid: '+peer)
        rows[peer]=row
    if any(rows['host'].get(field)!=rows['client-1'].get(field) for field in ('snapshot_id','type','variant')):
        raise AcceptanceError('Move UI shared snapshot/type/variant binding differs')
    return rows

def baseline_key(bindings):
    candidates=[]
    for peer in PEERS:
        candidates.append({row.get('record_key') for row in bindings[peer].get('bindings',[])
            if row.get('type')=='SmallInn' and row.get('variant')=='base' and row.get('revoked') is False
            and row.get('unknown') is False and isinstance(row.get('record_key'),str) and ':world:building:' in row['record_key']})
    common=candidates[0]&candidates[1]
    if len(common)!=1: raise AcceptanceError('Move UI common SmallInn/base baseline count differs: actual='+str(len(common))+' required=1')
    key=next(iter(common)); target_rows(bindings,key); return key

def validate_candidate(runner,frozen,candidate_binary):
    for peer in PEERS:
        path=runner.manifests[peer]
        identity=read_json(path)
        kind=manifest_runtime_kind(identity)
        if kind==FRAMEWORK_PLUGIN_KIND:
            bound=Runtime(runner.root).bind(str(path))
            if bound.get('process_identity_verified') is not True:
                raise AcceptanceError('native Move UI Framework process identity unverified: '+peer)
            receipt=candidate_binary_receipt(identity,candidate_binary)
        elif candidate_binary is not None:
            receipt=candidate_binary_receipt(identity,candidate_binary)
            actual=hash_file(Path(identity['runtime_root'])/'game'/'MT2_Multiplayer.dll')
            if actual!=frozen['dll_sha256']:
                raise AcceptanceError('native Move UI installed DLL bytes differ: '+peer)
        else:
            if identity['installed_binary_sha256']['MT2_Multiplayer.dll']!=frozen['dll_sha256']:
                raise AcceptanceError('native Move UI DLL differs: '+peer)
            continue
        if receipt['sha256']!=frozen['dll_sha256']:
            raise AcceptanceError('native Move UI candidate differs from freeze: '+peer)
        runner.checked('selected-candidate-'+peer,receipt)

def prepare_fixture(runner,args,proof,frozen):
    placement=getattr(args,'placement',None)
    label='placement' if placement else 'setup' if args.setup else 'direct Move'
    if proof.get('passed') is not True: raise AcceptanceError(label+' fixture not confirmed')
    expected=proof.get('new_run_id',proof.get('runtime_run_id')) if args.setup else proof.get('runtime_run_id')
    if not isinstance(expected,str) or not expected: raise AcceptanceError(label+' owned run missing')
    runner.load_manifests()
    if runner.summary['runtime_run_id']!=expected: raise AcceptanceError(label+' owned run differs')
    validate_candidate(runner,frozen,getattr(args,'candidate_binary',None))
    if placement:
        if proof.get('state')!='confirmed-normal-building-UI-two':raise AcceptanceError('normal placement proof required')
        action=proof.get('action_uuid')
        if not isinstance(action,str) or str(uuid.UUID(action))!=action:raise AcceptanceError('placement action UUID invalid')
        matches=[row['result'] for row in proof.get('checks',[]) if row.get('check')=='normal-UI-building-confirmed']
        if len(matches)!=1 or matches[0].get('receipt',{}).get('action_uuid')!=action:raise AcceptanceError('placement receipt identity differs')
        key='action:'+action
        for peer in PEERS:
            if matches[0].get('objects',{}).get(peer,{}).get('record_key')!=key:raise AcceptanceError('placement object identity differs: '+peer)
            state=runner.inspect(peer+'-read-installed-Move',peer)
            controller=state.get('typed_actions',{})
            if not controller.get('started') or controller.get('pending') or controller.get('locked'):raise AcceptanceError('placement Controller not idle: '+peer)
            if not state.get('ui_move',{}).get('enabled'):runner.single(peer+'-enable-move',peer,'game_enable_ui_move')
        return key
    if args.setup:
        for peer in PEERS:
            for name,tool in (('start-controller','game_start_typed_actions'),('enable-build','game_enable_ui_build'),('enable-remove','game_enable_ui_remove'),('enable-move','game_enable_ui_move')):
                runner.single(peer+'-'+name,peer,tool)
        bindings={peer:runner.single('initial-binding-'+peer,peer,'game_read_remove_bindings') for peer in PEERS}
        return baseline_key(bindings)
    matches=[check.get('result',{}).get('record_key') for check in proof.get('checks',[]) if check.get('check')=='host-native-move-confirmed']
    if len(matches)!=1 or not isinstance(matches[0],str) or not matches[0]: raise AcceptanceError('direct Move confirmed target key missing or ambiguous')
    return matches[0]

def finite_number(value,label):
    if type(value) not in (int,float) or abs(value)>3.4028234663852886e38 or not math.isfinite(value):
        raise AcceptanceError('Move UI finite float32 number required: '+label+' actual='+str(value))

def final_pose(receipt,request,row):
    if receipt.get('confirmed') is not True or receipt.get('submitted') is not True or receipt.get('command_accepted') is not True:
        raise AcceptanceError('normal Move UI receipt unconfirmed: '+str(receipt.get('cause')))
    for field,expected in (('phase',4),('capture_count',1),('exception',0),('cleanup_exception',0),('attempted_mask',255),('returned_mask',255)):
        if type(receipt.get(field)) is not int or receipt[field]!=expected:
            raise AcceptanceError('normal Move UI receipt '+field+' differs: actual='+str(receipt.get(field))+' required='+str(expected)+' cause='+str(receipt.get('cause')))
    for field in ('acceptance_token','snapshot_uuid','record_key'):
        if receipt.get(field)!=request[field]: raise AcceptanceError('normal Move UI receipt binding differs: '+field)
    action=receipt.get('action_uuid')
    try: valid=isinstance(action,str) and str(uuid.UUID(action))==action
    except ValueError: valid=False
    if not valid: raise AcceptanceError('normal Move UI action UUID invalid: '+str(action))
    pose=receipt.get('final_intent_pose')
    if not isinstance(pose,dict): raise AcceptanceError('normal Move UI final_intent_pose unavailable')
    for field in ('type','variant'):
        if pose.get(field)!=row[field]: raise AcceptanceError('normal Move UI final_intent_pose '+field+' differs')
    for field,count in (('position',3),('rotation',4),('scale',3)):
        values=pose.get(field)
        if not isinstance(values,list) or len(values)!=count: raise AcceptanceError('normal Move UI final_intent_pose '+field+' requires '+str(count)+' scalars')
        for index,value in enumerate(values): finite_number(value,'final_intent_pose.'+field+'['+str(index)+']')
    return pose

def native_receipt(receipt,action,row,peer):
    for field,expected in (('token',action),('native_uid',row['local_uid']),('snapshot_id',row['snapshot_id']),('record_key',row['record_key'])):
        if receipt.get(field)!=expected: raise AcceptanceError('Move UI actual native binding differs: '+peer+':'+field+' cause='+str(receipt.get('error')))
    for field in ('attempted','confirmed','undo_confirmed','selection_confirmed','cleanup_confirmed',
        *(step+'_'+state for step in ('loc','begin','open','ctor','move','dtor','close','end','vector','string') for state in ('attempted','returned'))):
        if receipt.get(field) is not True: raise AcceptanceError('Move UI actual native receipt missing: '+peer+':'+field+' cause='+str(receipt.get('error')))
    for field in ('exception','cleanup_exception'):
        if type(receipt.get(field)) is not int or receipt[field]!=0: raise AcceptanceError('Move UI native '+field+' differs: '+peer+' actual='+str(receipt.get(field))+' cause='+str(receipt.get('error')))

def settled_object(objects,pose,peer):
    for index,field in ((0,'x'),(2,'z')):
        finite_number(objects.get(field),peer+'.'+field)
        if objects[field]!=pose['position'][index]: raise AcceptanceError('Move UI final captured position differs: '+peer+':'+field+' actual='+str(objects[field])+' required='+str(pose['position'][index]))
    finite_number(objects.get('y'),peer+'.y')

def settled_peers(objects,pose):
    for peer in PEERS: settled_object(objects[peer],pose,peer)
    if objects['host']['y']!=objects['client-1']['y']: raise AcceptanceError('settled Move UI height differs between peers')

def main():
    args=parse_args(); args.players=2; args.cleanup=False; args.step_timeout=45
    runner=AcceptanceRunner(args)
    runner.summary['scope']='normal existing BuildingInfoWindow Move and native cursor callbacks, normal GameUpdate capture, original authoritative native Move and local retirement on both peers'
    try:
        proof=read_json(args.setup or args.direct_move or args.placement); frozen=read_json(args.freeze)
        key=prepare_fixture(runner,args,proof,frozen)
        actors=('client-1','host') if args.actor=='both' else (args.actor,)
        runner.summary['actors']=list(actors)
        for actor in actors:
            before={peer:runner.inspect(actor+'-before-'+peer,peer) for peer in ('host','client-1')}
            ledger=compare_ledgers(before['host'],before['client-1'])
            base=before['host']['typed_actions']['confirmed']
            if before['client-1']['typed_actions']['confirmed']!=base: raise AcceptanceError('Controller counts differ before Move UI')
            bindings={peer:runner.single(actor+'-binding-'+peer,peer,'game_read_remove_bindings') for peer in ('host','client-1')}
            rows=target_rows(bindings,key)
            row=rows[actor]; x=row['x']+args.offset_x; z=row['z']
            origin=before[actor]['native_open_origin']
            prepared=runner.single(actor+'-prepare-native-default',actor,'game_native_cancel_tool',{'acceptance_token':str(uuid.uuid4()),'snapshot_uuid':row['snapshot_id'],'expected_world_epoch':origin['world_epoch'],'expected_generation':origin['generation']})
            if prepared.get('confirmed') is not True or prepared.get('unknown') or prepared.get('exception'): raise AcceptanceError('native default tool preparation not confirmed: '+actor)
            ground=runner.single(actor+'-ground',actor,'game_probe_building_placement',{'type':row['type'],'variant':row['variant'],'x':x,'z':z})
            token=str(uuid.uuid4())
            request={'acceptance_token':token,'snapshot_uuid':row['snapshot_id'],'record_key':key,'expected_world_epoch':origin['world_epoch'],'expected_generation':origin['generation'],'x':x,'y':ground['y'],'z':z}
            runner.checked(actor+'-immutable-native-UI-request',request)
            admitted=runner.single(actor+'-normal-Move-command-once',actor,'game_native_move_ui',request)
            runner.checked(actor+'-normal-UI-admitted',admitted)
            deadline=time.monotonic()+45
            while time.monotonic()<deadline:
                receipt=runner.single(actor+'-read-normal-UI',actor,'game_read_move_ui',{'acceptance_token':token})
                if receipt.get('confirmed') is True: break
                if receipt.get('phase') in (0,5): raise AcceptanceError('Move UI rejected or Unknown: '+str(receipt))
            else: raise AcceptanceError('Move UI observation timeout; no replay')
            pose=final_pose(receipt,request,row)
            action=receipt['action_uuid']
            after={peer:runner.inspect(actor+'-after-'+peer,peer) for peer in ('host','client-1')}
            if compare_ledgers(after['host'],after['client-1'])!=ledger: raise AcceptanceError('Move UI changed ledger')
            if after['host']['typed_actions'].get('ack_mask')!=3: raise AcceptanceError('Move UI Host lacks both ACKs')
            objects={}
            for peer,state in after.items():
                controller=state['typed_actions']; native=state['ui_move']['last_native']
                if controller.get('action_uuid')!=action or controller.get('confirmed')!=base+1 or controller.get('pending') or controller.get('outcome')!=1: raise AcceptanceError('Move UI exact completion differs: '+peer)
                native_receipt(native,action,rows[peer],peer)
                objects[peer]=next(prop for prop in state['props'] if prop.get('uid')==rows[peer]['local_uid'])
                if state['ui_move']['captured']!=before[peer]['ui_move']['captured']+(1 if peer==actor else 0): raise AcceptanceError('normal Move capture origin or replay loop differs: '+peer)
            settled_peers(objects,pose)
            runner.checked(actor+'-normal-Move-confirmed',{'action_uuid':action,'record_key':key,'objects':objects,'ledger':ledger,'receipt':receipt})
        for peer in ('host','client-1'):
            row=objects[peer]
            runner.single('camera-'+peer,peer,'game_focus_test_camera',{'x':row['x'],'y':row['y'],'z':row['z'],'distance':100})
            runner.capture(peer)
        runner.summary.update(passed=True,state='confirmed-normal-Move-two')
    except Exception as error:
        runner.summary.update(passed=False,state='failed-normal-Move-two',error={'step':runner.summary['step'],'cause':str(error),'behavior':'remaining mutations skipped; no native command or Move replay'})
        log('error','normal Move: '+str(error))
    runner.summary['finished_utc']=utc(); runner.persist(); print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1

if __name__=='__main__': raise SystemExit(main())

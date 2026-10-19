"""Prove one Client request, one Host removal, one native Client application and both actual retirements."""
import argparse
from pathlib import Path
import time
import uuid
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, ledger_state, utc, log
from native_remove_acceptance import target, retirement_confirmed

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest')
    parser.add_argument('--expected-dll-sha256',required=True)
    parser.add_argument('--expected-run-id',required=True,type=lambda value:str(uuid.UUID(value)))
    parser.add_argument('--step-timeout',type=float,default=45)
    args=parser.parse_args();args.players=2;args.cleanup=False
    runner=AcceptanceRunner(args)
    runner.summary['scope']='one typed Client RemoveSnapshotBuilding through actual Host execution, native Client application and exact release ACKs; previous native removal and retirement states carried forward'
    try:
        runner.load_manifests()
        if runner.summary['runtime_run_id']!=args.expected_run_id or read_json(runner.root/'run.json').get('players')!=2:
            raise AcceptanceError('typed remove exact owned two-peer run differs')
        sources={};targets={};before={}
        for peer in ('host','client-1'):
            manifest=read_json(runner.manifests[peer])
            if manifest['installed_binary_sha256']['MT2_Multiplayer.dll']!=args.expected_dll_sha256:
                raise AcceptanceError('typed remove DLL differs peer='+peer)
            sources[peer]=runner.single(peer+'-typed-bindings',peer,'game_read_remove_bindings')
            targets[peer]=target(sources[peer])
        for field in ('snapshot_id','record_key','type','variant','x','y','z'):
            if targets['host'][field]!=targets['client-1'][field]:
                raise AcceptanceError('typed native target differs field='+field)
        runner.checked('typed-native-targets',targets)
        for peer in ('host','client-1'):
            item=targets[peer]
            runner.single(peer+'-typed-focus',peer,'game_focus_test_camera',{'x':item['x'],'y':item['y'],'z':item['z'],'distance':150})
            before[peer]=runner.inspect(peer+'-typed-before',peer)
            runner.capture(peer)
            started=runner.single(peer+'-typed-start-once',peer,'game_start_typed_actions')
            if started.get('started') is not True or started.get('locked') is not False:
                raise AcceptanceError('typed controller start not confirmed peer='+peer)
        if ledger_state(before['host'])!=ledger_state(before['client-1']):
            raise AcceptanceError('typed initial ledgers differ; no request submitted')
        identifier=str(uuid.uuid4());runner.summary['action_uuid']=identifier;runner.persist()
        submitted=runner.single('client-typed-remove-request-once','client-1','game_request_snapshot_remove',
            {'action_uuid':identifier,'record_key':targets['client-1']['record_key']})
        runner.checked('typed-client-request-submitted',submitted)
        deadline=time.monotonic()+args.step_timeout;after={}
        for peer in ('host','client-1'):
            count=0
            while True:
                if time.monotonic()>=deadline:
                    raise AcceptanceError('typed completion observation timeout peer='+peer+'; no request replay')
                count+=1;engine=runner.inspect(f'{peer}-typed-completion-{count:02d}',peer)
                state=engine.get('typed_actions',{})
                if state.get('locked') is True:
                    raise AcceptanceError('typed session Unknown peer='+peer+' cause='+state.get('cause',''))
                if state.get('action_uuid')==identifier and state.get('outcome')==1 and state.get('pending') is False:
                    for field in ('attempted','confirmed','host_revision'):
                        if type(state.get(field)) is not int or state[field]!=1:
                            raise AcceptanceError('typed exactly-once count differs peer='+peer+' field='+field)
                    if state.get('failed')!=0 or state.get('cause'):
                        raise AcceptanceError('typed completion failure field present peer='+peer)
                    if peer=='host' and (state.get('required_mask')!=3 or state.get('ack_mask')!=3):
                        raise AcceptanceError('typed Host lacks both native acknowledgements')
                    removed=engine.get('native_remove',{})
                    receipt=removed.get('receipts',{}).get(identifier,{})
                    retired=removed.get('retirements',{}).get(identifier,{})
                    if receipt.get('confirmed') is not True or receipt.get('binding_revoked') is not True:
                        raise AcceptanceError('typed exact native remove receipt unavailable peer='+peer)
                    retirement_confirmed(retired,identifier,receipt['sequence'])
                    if any(prop.get('uid')==targets[peer]['local_uid'] for prop in engine.get('props',[])):
                        raise AcceptanceError('typed removed building still present peer='+peer)
                    after[peer]=engine;runner.checked('typed-native-completion',{'peer':peer,'state':state,'retirement':retired})
                    break
        if ledger_state(after['host'])!=ledger_state(after['client-1']):
            raise AcceptanceError('typed final ledgers differ')
        if ledger_state(after['host'])['cash']!=ledger_state(before['host'])['cash']+400:
            raise AcceptanceError('typed native refund differs from small fixture')
        runner.checked('typed-native-ledgers',ledger_state(after['host']))
        for peer in ('host','client-1'):runner.capture(peer)
        runner.summary.update(state='confirmed-typed-native-remove-two',passed=True,
            client_request_replication_proven=True,holder_retirement_proven=True)
    except Exception as failure:
        runner.summary.update(state='failed-typed-native-remove-two',passed=False,
            error={'step':runner.summary['step'],'cause':f'{type(failure).__name__}: {failure}',
                'behavior':'remaining mutations skipped; no native action or request replay'})
        log('error',f'typed remove failed: {failure}')
    runner.summary['finished_utc']=utc();runner.persist()
    log('info' if runner.summary['passed'] else 'error',f'typed remove passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1

if __name__=='__main__':raise SystemExit(main())

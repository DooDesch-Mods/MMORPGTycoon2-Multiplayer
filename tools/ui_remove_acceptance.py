"""Prove actual Client BuildingInfoWindow UI intent and one authoritative native removal per peer."""
import argparse
import hashlib
import time
import uuid
from pathlib import Path
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, ledger_state, utc, log
from native_remove_acceptance import target, retirement_confirmed

from runtime import Runtime, FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind, candidate_binary_receipt


def validate_run(runner, expected_run_id):
    if runner.summary['runtime_run_id'] != expected_run_id:
        raise AcceptanceError('UI Remove exact owned two-peer run differs')
    identities = {peer: read_json(runner.manifests[peer]) for peer in ('host', 'client-1')}
    kinds = {manifest_runtime_kind(identity) for identity in identities.values()}
    if len(kinds) != 1:
        raise AcceptanceError('UI Remove mixed runtime kinds are not one owned run')
    if FRAMEWORK_PLUGIN_KIND not in kinds:
        if read_json(runner.root / 'run.json').get('players') != 2:
            raise AcceptanceError('UI Remove exact owned two-peer run differs')
        return
    from framework_local_test import validate_manifest, safe_path
    selected = Path(runner.root)
    if selected.name != expected_run_id or selected.parent.name != 'runs':
        raise AcceptanceError('UI Remove Framework selected root is not the exact runs/UUID directory')
    launcher_root = selected.parent.parent
    safe_path(selected, launcher_root)
    active_path = safe_path(launcher_root / 'run.json', launcher_root)
    active = read_json(active_path)
    validate_manifest(active, launcher_root)
    if active.get('run_id') != expected_run_id or type(active.get('players')) is not int or active['players'] != 2:
        raise AcceptanceError('UI Remove active Framework run or two-player count differs')
    games = [record for record in active['processes'] if record.get('role') == 'game']
    if len(games) != 2 or sorted(record['peer_id'] for record in games) != [1, 2]:
        raise AcceptanceError('UI Remove active Framework game identities are not exact peers 1 and 2')
    fields = ('schema_version', 'runtime_kind', 'run_id', 'owner', 'runtime_root', 'executable_path',
              'pid', 'process_start_time', 'role', 'peer_id', 'framework_profile', 'installed_binary_sha256')
    for number, peer in enumerate(('host', 'client-1'), 1):
        identity = identities[peer]
        if Path(runner.manifests[peer]) != selected / peer / 'instance.json' or Path(identity['runtime_root']) != selected / peer:
            raise AcceptanceError('UI Remove selected Framework peer path differs peer=' + peer)
        record = next(item for item in games if item['peer_id'] == number)
        for field in fields:
            if record.get(field) != identity.get(field) or type(record.get(field)) is not type(identity.get(field)):
                raise AcceptanceError('UI Remove active Framework identity differs peer=' + peer + ' field=' + field)
    runner.checked('exact-active-framework-run', {'path': str(active_path), 'run_id': expected_run_id, 'players': 2})


def validate_candidate(runner, expected_hash, candidate_binary):
    for peer in ('host', 'client-1'):
        path = runner.manifests[peer]
        identity = read_json(path)
        if manifest_runtime_kind(identity) == FRAMEWORK_PLUGIN_KIND:
            bound = Runtime(runner.root).bind(str(path))
            if bound.get('process_identity_verified') is not True:
                raise AcceptanceError('UI Remove exact Framework process identity is not verified peer=' + peer)
            receipt = candidate_binary_receipt(identity, candidate_binary)
        elif candidate_binary is not None:
            receipt = candidate_binary_receipt(identity, candidate_binary)
            installed = Path(identity['runtime_root']) / 'game' / 'MT2_Multiplayer.dll'
            if hashlib.sha256(installed.read_bytes()).hexdigest() != receipt['sha256']:
                raise AcceptanceError('UI Remove actual installed DLL differs from explicit candidate peer=' + peer)
        else:
            receipt = {'module': 'MT2_Multiplayer.dll', 'sha256': identity['installed_binary_sha256']['MT2_Multiplayer.dll']}
        if receipt['sha256'] != expected_hash:
            raise AcceptanceError('UI Remove DLL differs peer=' + peer)
        runner.checked('selected-candidate-' + peer, receipt)



def require_ui(state, captured, replayed, peer):
    for name, value in (('enabled', True), ('unknown', False), ('replay_active', False),
                        ('captured', captured), ('replay_originals', replayed), ('rejected', 0), ('foreign_originals', 0), ('inflight', 0)):
        if state.get(name) != value or type(state.get(name)) is not type(value):
            raise AcceptanceError(f'UI Remove counter/state differs peer={peer} field={name} actual={state.get(name)} required={value}')


def actual_refund(bindings,item,peer):
    receipt=item.get('native_refund',{})
    for field in ('confirmed','attempted','returned'):
        if receipt.get(field) is not True:raise AcceptanceError('native refund '+field+' missing peer='+peer+' cause='+str(receipt.get('cause')))
    if type(receipt.get('seh')) is not int or receipt['seh']!=0 or receipt.get('cause')!='':raise AcceptanceError('native refund getter failure peer='+peer+' cause='+str(receipt.get('cause'))+' SEH='+str(receipt.get('seh')))
    value=receipt.get('value')
    if type(value) is not int or not -2147483648<=value<=2147483647:raise AcceptanceError('native refund value is not int32 peer='+peer)
    for field in ('local_uid','snapshot_id','record_key'):
        if receipt.get(field)!=item.get(field) or type(receipt.get(field)) is not type(item.get(field)):raise AcceptanceError('native refund target binding differs peer='+peer+' field='+field)
    for field in ('world_epoch','world_generation'):
        if type(bindings.get(field)) is not int or bindings[field]<=0 or type(receipt.get(field)) is not int or receipt[field]!=bindings[field]:raise AcceptanceError('native refund actual World binding differs peer='+peer+' field='+field)
    if type(receipt.get('getter_rva')) is not int or receipt['getter_rva'] not in (0xd314f0,0xcf30e0):raise AcceptanceError('native refund actual getter differs peer='+peer)
    return value


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest')
    parser.add_argument('--expected-dll-sha256',required=True)
    parser.add_argument('--candidate-binary',type=Path,help='Exact selected mod artifact; required for Framework plugin mode')
    parser.add_argument('--expected-run-id',required=True,type=lambda value:str(uuid.UUID(value)))
    parser.add_argument('--step-timeout',type=float,default=45)
    parser.add_argument('--target-key')
    parser.add_argument('--reuse-installed',action='store_true')
    parser.add_argument('--expected-type',default='SmallInn')
    parser.add_argument('--expected-variant',default='base')
    parser.add_argument('--expected-refund',type=int)
    parser.add_argument('--prepare-tool',action='store_true')
    parser.add_argument('--enable-live-host-state',action='store_true')
    args=parser.parse_args();args.players=2;args.cleanup=False
    runner=AcceptanceRunner(args)
    runner.summary['scope']='new actual Client BuildingInfoWindow.UICommand through genuine vsRecord and normal lower UI capture; previous typed removal, retirement and persistence are carried forward'
    try:
        runner.load_manifests()
        validate_run(runner,args.expected_run_id)
        validate_candidate(runner,args.expected_dll_sha256,args.candidate_binary)
        targets={};before={};refunds={}
        for peer in ('host','client-1'):
            bindings=runner.single(peer+'-UI-binding',peer,'game_read_remove_bindings')
            if args.target_key:
                matches=[item for item in bindings.get('bindings',[]) if item.get('record_key')==args.target_key and item.get('type')==args.expected_type and item.get('variant')==args.expected_variant and item.get('revoked') is False and item.get('unknown') is False]
                if len(matches)!=1:raise AcceptanceError('exact shared target unavailable peer='+peer)
                targets[peer]=matches[0]
            else:targets[peer]=target(bindings)
            refunds[peer]=actual_refund(bindings,targets[peer],peer)
            if args.expected_refund is not None and refunds[peer]!=args.expected_refund:raise AcceptanceError('native refund metadata differs expected='+str(args.expected_refund)+' actual='+str(refunds[peer])+' peer='+peer)
        for field in ('snapshot_id','record_key','type','variant','x','y','z'):
            if targets['host'][field]!=targets['client-1'][field]:raise AcceptanceError('UI target differs field='+field)
        if refunds['host']!=refunds['client-1']:raise AcceptanceError('native refund metadata differs between peers; UI command skipped')
        runner.checked('UI-actual-targets',targets)
        runner.checked('UI-actual-native-refunds',refunds)
        for peer in ('host','client-1'):
            item=targets[peer]
            runner.single(peer+'-UI-focus',peer,'game_focus_test_camera',{'x':item['x'],'y':item['y'],'z':item['z'],'distance':150})
            before[peer]=runner.inspect(peer+'-UI-before',peer)
            started=before[peer].get('typed_actions',{}) if args.reuse_installed else runner.single(peer+'-UI-controller-start-once',peer,'game_start_typed_actions')
            if started.get('started') is not True or started.get('locked') is not False:
                raise AcceptanceError('UI controller start not confirmed peer='+peer)
            if args.prepare_tool:
                origin=before[peer]['native_open_origin']
                prepared=runner.single(peer+'-prepare-native-default',peer,'game_native_cancel_tool',{'acceptance_token':str(uuid.uuid4()),'snapshot_uuid':item['snapshot_id'],'expected_world_epoch':origin['world_epoch'],'expected_generation':origin['generation']})
                if prepared.get('confirmed') is not True or prepared.get('unknown') or prepared.get('exception'):
                    raise AcceptanceError('native default tool preparation failed peer='+peer+' cause='+str(prepared.get('error') or prepared.get('cause') or prepared))
            runner.capture(peer)
            enabled=before[peer].get('ui_remove',{}) if args.reuse_installed else runner.single(peer+'-UI-enable-once',peer,'game_enable_ui_remove')
            require_ui(enabled,0,0,peer)
            if started.get('pending') is not False or started.get('failed')!=0:raise AcceptanceError('prior Controller action incomplete peer='+peer)
        counters={field:before['client-1']['typed_actions'][field] if args.reuse_installed else 0 for field in ('attempted','confirmed','host_revision')}
        if args.reuse_installed and any(before['host']['typed_actions'].get(field)!=value for field,value in counters.items()):raise AcceptanceError('prior Controller counters differ')
        if ledger_state(before['host'])!=ledger_state(before['client-1']):raise AcceptanceError('UI initial ledgers differ; command skipped')
        if args.enable_live_host_state:
            for peer in ('client-1','host'):
                enabled=runner.single(peer+'-enable-live-state-once',peer,'game_enable_live_host_state')
                live=enabled.get('live_state',{})
                if live.get('active') is not True or live.get('locked') is not False or live.get('cause'):
                    raise AcceptanceError('live Host state activation failed peer='+peer+' cause='+str(live))
                runner.checked('actual-live-state-enabled',{'peer':peer,'state':live})
        diagnostic=str(uuid.uuid4());runner.summary['diagnostic_uuid']=diagnostic;runner.persist()
        admitted=runner.single('client-actual-BuildingWindow-command-once','client-1','game_probe_remove_ui',
            {'diagnostic_uuid':diagnostic,'snapshot_uuid':targets['client-1']['snapshot_id'],'record_key':targets['client-1']['record_key']})
        if admitted.get('confirmed') is not True or admitted.get('diagnostic_uuid')!=diagnostic:
            raise AcceptanceError('actual UI diagnostic admission missing')
        identifier=str(uuid.UUID(admitted['action_uuid']));runner.summary['action_uuid']=identifier;runner.persist()
        receipt=admitted.get('receipt',{})
        for field in ('selection_attempted','selection_returned','window_confirmed','record_constructed','record_label_confirmed',
                      'ui_command_attempted','ui_command_returned','ui_command_accepted','record_cleanup_attempted','record_cleanup_confirmed','intent_observed'):
            if receipt.get(field) is not True:raise AcceptanceError('actual UI/record receipt incomplete field='+field+' cause='+str(receipt.get('cause')))
        if any(type(receipt.get(field)) is not int or receipt[field]!=0 for field in ('native_exception','cleanup_exception')) or receipt.get('failed_stage') or receipt.get('cause'):
            raise AcceptanceError('actual UI/record native failure retained: '+str(receipt))
        for field,expected in (('ui_captured',1),('ui_replay_originals',0),('ui_origin',1),('ui_disposition',3)):
            if type(admitted.get(field)) is not int or admitted[field]!=expected:
                raise AcceptanceError('actual BuildingWindow capture differs field='+field+' actual='+str(admitted.get(field))+' required='+str(expected))
        admission=admitted.get('action_at_return',{})
        if admission.get('action_uuid')!=identifier or admission.get('pending') is not True or admission.get('attempted')!=counters['attempted']:
            raise AcceptanceError('Client native action executed before Host authority')
        runner.checked('actual-UI-admission-before-Host',admitted)
        deadline=time.monotonic()+args.step_timeout;after={}
        for peer in ('host','client-1'):
            count=0
            while True:
                if time.monotonic()>=deadline:raise AcceptanceError('UI completion observation timed out peer='+peer+'; no command replay')
                count+=1;engine=runner.inspect(f'{peer}-UI-completion-{count:02d}',peer);state=engine.get('typed_actions',{})
                if state.get('locked') is True:raise AcceptanceError('UI typed session Unknown peer='+peer+' cause='+state.get('cause',''))
                if state.get('action_uuid')==identifier and state.get('outcome')==1 and state.get('pending') is False:
                    for field in ('attempted','confirmed','host_revision'):
                        if type(state.get(field)) is not int or state[field]!=counters[field]+1:raise AcceptanceError('UI authoritative count differs peer='+peer+' field='+field)
                    if state.get('failed')!=0 or state.get('cause'):raise AcceptanceError('UI completion has failure peer='+peer)
                    if peer=='host' and (state.get('required_mask')!=3 or state.get('ack_mask')!=3):raise AcceptanceError('UI Host lacks both native ACKs')
                    require_ui(engine.get('ui_remove',{}),1 if peer=='client-1' else 0,1,peer)
                    removed=engine.get('native_remove',{});native_receipt=removed.get('receipts',{}).get(identifier,{})
                    if native_receipt.get('confirmed') is not True or native_receipt.get('binding_revoked') is not True:raise AcceptanceError('UI native removal receipt absent peer='+peer)
                    retirement_confirmed(removed.get('retirements',{}).get(identifier,{}),identifier,native_receipt['sequence'])
                    if any(prop.get('uid')==targets[peer]['local_uid'] for prop in engine.get('props',[])):raise AcceptanceError('UI target still present peer='+peer)
                    after[peer]=engine;runner.checked('actual-UI-authoritative-completion',{'peer':peer,'state':state,'ui':engine['ui_remove']});break
        if ledger_state(after['host'])!=ledger_state(after['client-1']):raise AcceptanceError('UI final ledgers differ')
        for peer in ('host','client-1'):
            if ledger_state(after[peer])['cash']!=ledger_state(before[peer])['cash']+refunds[peer]:raise AcceptanceError('UI native refund differs peer='+peer+' expected='+str(refunds[peer]))
        runner.checked('actual-UI-native-ledgers',ledger_state(after['host']))
        if args.enable_live_host_state:
            for peer in ('host','client-1'):
                live=after[peer]['typed_actions'].get('live_state',{})
                if live.get('active') is not True or live.get('locked') is not False or live.get('unknown') is not False or live.get('cause'):
                    raise AcceptanceError('live Host state failed during Remove peer='+peer+' cause='+str(live))
                if peer=='client-1' and (live.get('confirmed') is not True or type(live.get('completed_stores')) is not int or live['completed_stores']!=live.get('attempted_stores') or not 0<=live['completed_stores']<=52):
                    raise AcceptanceError('Client native Host state stores incomplete: '+str(live))
                runner.checked('actual-live-state-after-remove',{'peer':peer,'state':live})
        for peer in ('host','client-1'):runner.capture(peer)
        runner.summary.update(state='confirmed-native-UI-remove-two',passed=True,regular_UI_capture_proven=True,
                              client_request_replication_proven=True,holder_retirement_proven=True)
    except Exception as failure:
        runner.summary.update(state='failed-native-UI-remove-two',passed=False,error={'step':runner.summary['step'],
            'cause':f'{type(failure).__name__}: {failure}','behavior':'remaining mutations skipped; no UI command, native action or request replay'})
        log('error',f'UI Remove failed: {failure}')
    runner.summary['finished_utc']=utc();runner.persist()
    log('info' if runner.summary['passed'] else 'error',f'UI Remove passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__=='__main__':raise SystemExit(main())

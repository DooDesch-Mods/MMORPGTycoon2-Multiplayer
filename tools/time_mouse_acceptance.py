"""Prove current native HUD buttons, one Host effect and paused Client simulation."""
import argparse,hashlib,math,time,uuid
from pathlib import Path
from local_acceptance import AcceptanceRunner,AcceptanceError,ROOT,read_json,utc,log

from runtime import Runtime, FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind, candidate_binary_receipt

def validate_run(runner, expected_run_id):
    if runner.summary['runtime_run_id'] != expected_run_id:
        raise AcceptanceError('time mouse exact owned two-peer run differs')
    identities = {peer: read_json(runner.manifests[peer]) for peer in ('host', 'client-1')}
    kinds = {manifest_runtime_kind(identity) for identity in identities.values()}
    if len(kinds) != 1:
        raise AcceptanceError('time mouse mixed runtime kinds are not one owned run')
    if FRAMEWORK_PLUGIN_KIND not in kinds:
        if read_json(runner.root / 'run.json').get('players') != 2:
            raise AcceptanceError('time mouse exact owned two-peer run differs')
        return
    from framework_local_test import validate_manifest, safe_path
    selected = Path(runner.root)
    if selected.name != expected_run_id or selected.parent.name != 'runs':
        raise AcceptanceError('time mouse Framework selected root is not the exact runs/UUID directory')
    launcher_root = selected.parent.parent
    supplied = Path(runner.args.test_root).absolute()
    if supplied != selected or supplied.resolve() != supplied:
        raise AcceptanceError('time mouse Framework selected root contains an alias or noncanonical path')
    safe_path(selected, launcher_root)
    active_path = safe_path(launcher_root / 'run.json', launcher_root)
    active = read_json(active_path)
    validate_manifest(active, launcher_root)
    if active.get('run_id') != expected_run_id or type(active.get('players')) is not int or active['players'] != 2:
        raise AcceptanceError('time mouse active Framework run or two-player count differs')
    games = [record for record in active['processes'] if record.get('role') == 'game']
    if len(games) != 2 or sorted(record['peer_id'] for record in games) != [1, 2]:
        raise AcceptanceError('time mouse active Framework game identities are not exact peers 1 and 2')
    fields = ('schema_version', 'runtime_kind', 'run_id', 'owner', 'runtime_root', 'executable_path',
              'pid', 'process_start_time', 'role', 'peer_id', 'framework_profile', 'installed_binary_sha256')
    for number, peer in enumerate(('host', 'client-1'), 1):
        identity = identities[peer]
        if Path(runner.manifests[peer]) != selected / peer / 'instance.json' or Path(identity['runtime_root']) != selected / peer:
            raise AcceptanceError('time mouse selected Framework peer path differs peer=' + peer)
        record = next(item for item in games if item['peer_id'] == number)
        for field in fields:
            if record.get(field) != identity.get(field) or type(record.get(field)) is not type(identity.get(field)):
                raise AcceptanceError('time mouse active Framework identity differs peer=' + peer + ' field=' + field)
    runner.checked('exact-active-framework-run', {'path': str(active_path), 'run_id': expected_run_id, 'players': 2})


def validate_candidate(runner, expected_hash, candidate_binary):
    for peer in ('host', 'client-1'):
        path = runner.manifests[peer]
        identity = read_json(path)
        if manifest_runtime_kind(identity) == FRAMEWORK_PLUGIN_KIND:
            bound = Runtime(runner.root).bind(str(path))
            if bound.get('process_identity_verified') is not True:
                raise AcceptanceError('time mouse exact Framework process identity is not verified peer=' + peer)
            receipt = candidate_binary_receipt(identity, candidate_binary)
        elif candidate_binary is not None:
            receipt = candidate_binary_receipt(identity, candidate_binary)
            installed = Path(identity['runtime_root']) / 'game' / 'MT2_Multiplayer.dll'
            if hashlib.sha256(installed.read_bytes()).hexdigest() != receipt['sha256']:
                raise AcceptanceError('time mouse actual installed DLL differs from explicit candidate peer=' + peer)
        else:
            receipt = {'module': 'MT2_Multiplayer.dll', 'sha256': identity['installed_binary_sha256']['MT2_Multiplayer.dll']}
        if receipt['sha256'] != expected_hash:
            raise AcceptanceError('time mouse DLL differs peer=' + peer)
        runner.checked('selected-candidate-' + peer, receipt)



def finite_timeout(text):
    value=float(text)
    if not math.isfinite(value) or not 0<value<=65:raise argparse.ArgumentTypeError('step timeout requires a finite value above0 and at most65 seconds')
    return value

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-run-id',required=True)
    parser.add_argument('--expected-dll-sha256',required=True)
    parser.add_argument('--candidate-binary',type=Path,help='Exact selected mod artifact; required for Framework plugin mode')
    parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest')
    parser.add_argument('--step-timeout',type=finite_timeout,default=45)
    parser.add_argument('--intermediate-only',action='store_true',help='Cover only the remaining Client scale3 button and its subsequent pause; retain prior time proofs')
    args=parser.parse_args(argv);args.players=2;args.cleanup=False
    runner=AcceptanceRunner(args)
    runner.summary['scope']='new actual HUD ItemButton.Activate Mouse route on Client and Host; prior native keyboard/snapshot/persistence milestones carried forward'
    runner.summary['source_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    steps=(('client-1',3),('client-1',0)) if args.intermediate_only else (('client-1',9),('client-1',0),('host',1),('host',0))
    if args.intermediate_only:
        runner.summary['scope']='Remaining actual Client HUD intermediate scale3 and subsequent pause; prior high-speed, normal and pause proofs carried forward'
    try:
        runner.load_manifests()
        validate_run(runner,args.expected_run_id)
        validate_candidate(runner,args.expected_dll_sha256,args.candidate_binary)
        for peer in ('host','client-1'):
            state=runner.inspect(peer+'-mouse-baseline',peer)
            if state['overlay_visible']:runner.single(peer+'-close-owned-overlay',peer,'game_toggle_overlay')
            if state.get('hud_time_activation',{}).get('initialized') is not True:raise AcceptanceError('HUD Mouse adapter not initialized: '+peer)
        attempts=[]
        for index,(peer,scale) in enumerate(steps):
            state=runner.inspect(peer+f'-mouse-{index}-preflight',peer)
            runtime=state['shared_time']['runtime'];controller=runtime['controller']
            if runtime['phase']!=3 or controller['write_locked']:raise AcceptanceError('time authority not active: '+peer)
            current=controller['observed' if peer=='host' else 'applied']
            origin=state['native_open_origin']
            peer_id=1 if peer=='host' else 2
            token=str(uuid.uuid4())
            host_runtime=runner.inspect(f'host-mouse-{index}-setter-before','host')['shared_time']['runtime']
            fields={'acceptance_token':token,'snapshot_uuid':origin['uuid'],'scale':scale,
                'expected_epoch':runtime['epoch'],'expected_world_epoch':origin['world_epoch'],'expected_generation':origin['generation'],
                'expected_revision':current['revision'],'expected_request_sequence':host_runtime['controller']['processed_sequences'][peer_id-1]+1}
            runner.summary.setdefault('mouse_tokens',[]).append({'peer':peer,**fields});runner.persist()
            host_before=host_runtime['native_set_confirmed']
            admitted=runner.single(peer+f'-mouse-{index}-native-entry-once',peer,'game_native_time_button',fields)
            if admitted['state']!='waiting_for_host' or not admitted['native']['attempted'] or not admitted['native']['returned']:
                raise AcceptanceError('native time button admission incomplete: '+peer)
            deadline=time.monotonic()+args.step_timeout;count=0
            while True:
                if time.monotonic()>=deadline:raise AcceptanceError('native time mouse Host confirmation timeout; no button replay token='+token)
                count+=1
                receipt=runner.single(peer+f'-mouse-{index}-receipt-{count:02d}',peer,'game_read_time_mouse',{'acceptance_token':token})
                if receipt['state']=='confirmed':break
                if receipt['state']!='waiting_for_host':raise AcceptanceError('native time mouse failed: '+receipt.get('error',''))
            engine={}
            for role in ('host','client-1'):
                engine[role]=runner.inspect(role+f'-mouse-{index}-confirmed',role)
                if engine[role]['session_connected'] is not True:raise AcceptanceError('time mouse ended session: '+role)
                calendar=engine[role]['hud_calendar']
                if not calendar['bound'] or calendar['locked'] or calendar['transport']['locked'] or calendar['error'] or calendar['transport']['error']:
                    raise AcceptanceError('time mouse Calendar degraded: '+role)
                native_runtime=engine[role]['shared_time']['runtime']
                actual=native_runtime['controller']['observed' if role=='host' else 'applied']
                if actual['selected']!=scale or actual['revision']!=receipt['host_receipt']['revision']:
                    raise AcceptanceError('time mouse shared presentation differs: '+role)
            if engine['client-1']['clock']['scale']!=0 or engine['client-1']['shared_time']['runtime']['native_set_confirmed']!=0:
                raise AcceptanceError('time mouse Client native clock changed')
            if engine['host']['shared_time']['runtime']['native_set_confirmed']!=host_before+1:
                raise AcceptanceError('time mouse Host setter was not exactly once')
            runner.checked('actual-native-HUD-mouse-confirmed',{'peer':peer,'scale':scale,'receipt':receipt,
                'host_setter_before':host_before,'host_setter_after':host_before+1,'ClientNativeClock':0})
            attempts.append(receipt)
            if scale in (3,9):
                for role in ('host','client-1'):runner.capture(role)
        for role in ('host','client-1'):runner.capture(role)
        runner.summary.update(state='confirmed-native-HUD-mouse-two',passed=True,native_HUD_route_proven=True,
            client_original_suppressed=True,client_clock_zero=True,attempts=attempts)
    except Exception as cause:
        runner.summary.update(state='failed-native-HUD-mouse-two',passed=False,error={'step':runner.summary['step'],
            'cause':str(cause),'behavior':'remaining button mutations skipped; no command replay or clock compensation'})
        log('error','native time mouse acceptance: '+str(cause))
    runner.summary['finished_utc']=utc();runner.persist()
    log('info' if runner.summary['passed'] else 'error','native time mouse passed='+str(runner.summary['passed'])+' summary='+str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

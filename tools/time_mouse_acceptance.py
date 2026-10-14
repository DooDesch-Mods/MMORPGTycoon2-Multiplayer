"""Prove current native HUD buttons, one Host effect and paused Client simulation."""
import argparse,math,time,uuid
from pathlib import Path
from local_acceptance import AcceptanceRunner,AcceptanceError,ROOT,read_json,utc,log

def finite_timeout(text):
    value=float(text)
    if not math.isfinite(value) or not 0<value<=65:raise argparse.ArgumentTypeError('step timeout requires a finite value above0 and at most65 seconds')
    return value

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-run-id',required=True)
    parser.add_argument('--expected-dll-sha256',required=True)
    parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest')
    parser.add_argument('--step-timeout',type=finite_timeout,default=45)
    args=parser.parse_args(argv);args.players=2;args.cleanup=False
    runner=AcceptanceRunner(args)
    runner.summary['scope']='new actual HUD ItemButton.Activate Mouse route on Client and Host; prior native keyboard/snapshot/persistence milestones carried forward'
    try:
        runner.load_manifests()
        if runner.summary['runtime_run_id']!=args.expected_run_id or read_json(runner.root/'run.json')['players']!=2:
            raise AcceptanceError('time mouse exact owned two-peer run differs')
        for peer in ('host','client-1'):
            manifest=read_json(runner.manifests[peer])
            if manifest['installed_binary_sha256']['MT2_Multiplayer.dll']!=args.expected_dll_sha256:
                raise AcceptanceError('time mouse DLL differs: '+peer)
            state=runner.inspect(peer+'-mouse-baseline',peer)
            if state['overlay_visible']:runner.single(peer+'-close-owned-overlay',peer,'game_toggle_overlay')
            if state.get('hud_time_activation',{}).get('initialized') is not True:raise AcceptanceError('HUD Mouse adapter not initialized: '+peer)
        attempts=[]
        for index,(peer,scale) in enumerate((('client-1',9),('client-1',0),('host',1),('host',0))):
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
            if scale==9:
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

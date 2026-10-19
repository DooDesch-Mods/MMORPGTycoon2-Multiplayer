"""Retain current user worlds, then load the fixed two-peer small diagnostic world."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys,uuid
from local_acceptance import AcceptanceRunner,AcceptanceError,ROOT,read_json,validate_snapshot,utc,log
import argparse

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--freeze',type=Path,required=True)
    parser.add_argument('--expected-current-run-id',required=True)
    parser.add_argument('--retained-host-job',type=Path)
    parser.add_argument('--retained-worlds-summary',type=Path)
    args=parser.parse_args();args.test_root=ROOT/'_MPTest';args.players=2;args.cleanup=False;args.step_timeout=45
    runner=AcceptanceRunner(args)
    runner.summary['scope']='retain user-edited worlds and prepare new exact DLL fixture for HUD mouse acceptance; existing snapshot/persistence milestones carried forward'
    try:
        frozen=read_json(args.freeze)
        for name,digest in frozen['files'].items():
            if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=digest:raise AcceptanceError('time candidate source changed: '+name)
        dll=ROOT/'build/bin/Release/MT2_Multiplayer.dll'
        digest=hashlib.sha256(dll.read_bytes()).hexdigest()
        runner.summary['built_dll_sha256']=digest;runner.persist()
        runner.load_manifests()
        if runner.summary['runtime_run_id']!=args.expected_current_run_id:raise AcceptanceError('user world preservation current owned run differs')
        for peer in ('host','client-1'):
            if args.retained_worlds_summary:
                evidence=read_json(args.retained_worlds_summary)
                if evidence.get('passed') is not True or evidence.get('runtime_run_id')!=args.expected_current_run_id:
                    raise AcceptanceError('retained two-peer world run or result differs')
                expected_check='native-placed-world-file' if peer=='host' else 'native-client-world-file'
                saved=next(check['result'] for check in evidence['checks'] if check['check']==expected_check)
            elif peer=='host' and args.retained_host_job:
                evidence=read_json(args.retained_host_job)
                if evidence.get('passed') is not True:raise AcceptanceError('retained latest Host save job failed')
                response=next(call for call in evidence['calls'] if call['tool']=='game_capture_local_snapshot')['result']['structuredContent']
                if response.get('ok') is not True or response.get('run_id')!=args.expected_current_run_id:raise AcceptanceError('retained Host save job run differs')
                saved=response['result']
            else:saved=runner.single(peer+'-latest-user-local-save',peer,'game_capture_local_snapshot')
            saved=validate_snapshot(saved,saved['snapshot_id'],runner.root/peer)
            if saved.get('state')!='snapshot_file_complete' or saved.get('distribution_requested') is not False:
                raise AcceptanceError('user world local-only file receipt differs: '+peer)
            target=runner.directory/(peer+'_user_world.vrz');target.write_bytes(Path(saved['path']).read_bytes())
            if hashlib.sha256(target.read_bytes()).hexdigest()!=saved['sha256']:raise AcceptanceError('retained user world SHA differs: '+peer)
            runner.checked('latest-user-world-retained',{'peer':peer,'local_file':str(target),'snapshot':saved})
        command=[sys.executable,'-X','utf8',str(ROOT/'tools/native_open_acceptance.py'),
            '--snapshot-id','41f6e0b5-bcc2-4ef4-b930-ddfe6b3dffe2','--expected-input-bytes','208949',
            '--expected-input-sha256','fedec7bec52be5c40a362e0552390e804bbe86defc949524c6f9c4a20a519c1a',
            '--expected-dll-sha256',digest,'--players','2']
        runner.command('restart-and-load-small-fixture',command)
        current=read_json(runner.root/'run.json')
        runner.summary.update(state='prepared-time-mouse-two',passed=True,new_run_id=current['run_id'])
    except Exception as cause:
        runner.summary.update(state='failed-time-mouse-setup',passed=False,error={'step':runner.summary['step'],'cause':str(cause),'behavior':'remaining mutations skipped; no snapshot load replay'})
        log('error','time mouse setup: '+str(cause))
    runner.summary['finished_utc']=utc();runner.persist()
    log('info' if runner.summary['passed'] else 'error','time mouse setup passed='+str(runner.summary['passed'])+' summary='+str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

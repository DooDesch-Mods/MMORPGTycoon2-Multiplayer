"""Retain both exact owned worlds through local-only native game MCP saves."""
import argparse
from pathlib import Path
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, validate_snapshot, utc, log

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-run-id',required=True)
    parser.add_argument('--expected-dll-sha256',required=True)
    parser.add_argument('--peer',choices=('both','host','client-1'),default='both')
    args=parser.parse_args(); args.test_root=ROOT/'_MPTest'; args.players=2; args.cleanup=False; args.step_timeout=45
    runner=AcceptanceRunner(args); runner.summary['scope']='retain current owned worlds as complete local files, peer='+args.peer+'; no distribution, action replay or fresh regression claim'
    try:
        runner.load_manifests()
        if runner.summary['runtime_run_id']!=args.expected_run_id: raise AcceptanceError('owned retention run differs')
        peers=('host','client-1') if args.peer=='both' else (args.peer,)
        for peer in peers:
            if read_json(runner.manifests[peer])['installed_binary_sha256']['MT2_Multiplayer.dll']!=args.expected_dll_sha256: raise AcceptanceError('owned retention DLL differs: '+peer)
            engine=runner.inspect(peer+'-read-before-retention',peer)
            controller=engine.get('typed_actions',{})
            if controller.get('locked') or controller.get('pending'): raise AcceptanceError('native action unresolved before retention: '+peer)
            saved=runner.single(peer+'-local-save-once',peer,'game_capture_local_snapshot')
            saved=validate_snapshot(saved,saved['snapshot_id'],runner.root/peer)
            if saved.get('state')!='snapshot_file_complete' or saved.get('distribution_requested') is not False: raise AcceptanceError('local-only complete file receipt differs: '+peer)
            runner.checked('native-placed-world-file' if peer=='host' else 'native-client-world-file',saved)
        runner.summary.update(passed=True,state='retained-two-current-worlds')
    except Exception as error:
        runner.summary.update(passed=False,state='failed-world-retention',error={'step':runner.summary['step'],'cause':str(error),'behavior':'remaining mutations skipped; no Save replay'})
        log('error','world retention: '+str(error))
    runner.summary['finished_utc']=utc(); runner.persist(); print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1

if __name__=='__main__': raise SystemExit(main())

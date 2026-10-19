"""Capture the actual two-peer native view of an already confirmed object."""
import argparse
from pathlib import Path
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, utc, log
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--proof',type=Path,required=True)
    parser.add_argument('--expected-dll-sha256',required=True)
    args=parser.parse_args(); args.test_root=ROOT/'_MPTest'; args.players=2; args.cleanup=False; args.step_timeout=45
    runner=AcceptanceRunner(args); runner.summary['scope']='native framebuffer of confirmed two-peer object; original action not replayed'
    try:
        proof=read_json(args.proof)
        if proof.get('passed') is not True: raise AcceptanceError('confirmed object proof missing')
        runner.load_manifests()
        if runner.summary['runtime_run_id']!=proof['runtime_run_id']: raise AcceptanceError('confirmed object run differs')
        for peer in ('host','client-1'):
            if read_json(runner.manifests[peer])['installed_binary_sha256']['MT2_Multiplayer.dll']!=args.expected_dll_sha256: raise AcceptanceError('confirmed view DLL differs: '+peer)
            prop=proof['objects'][peer]
            runner.single(peer+'-focus',peer,'game_focus_test_camera',{'x':prop['x'],'y':prop['y'],'z':prop['z'],'distance':100})
            runner.capture(peer)
        runner.summary.update(passed=True,state='captured-confirmed-action-view')
    except Exception as error:
        runner.summary.update(passed=False,state='failed-action-view',error={'step':runner.summary['step'],'cause':str(error),'behavior':'remaining calls skipped; no action replay'})
        log('error','native action view: '+str(error))
    runner.summary['finished_utc']=utc(); runner.persist(); print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1
if __name__=='__main__': raise SystemExit(main())

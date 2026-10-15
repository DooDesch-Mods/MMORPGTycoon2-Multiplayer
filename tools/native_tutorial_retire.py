"""Retire the exact owned local tutorial fixture before selecting another world."""
import argparse
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from local_acceptance import AcceptanceError, AcceptanceRunner, ROOT, utc
from lobby_ui.native_local_acceptance import button, observe, left

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--expected-run-id',required=True)
parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest')
args=parser.parse_args()
args.players,args.cleanup=2,False
args.step_timeout,args.ready_timeout,args.wait_seconds=45,60,60
runner=AcceptanceRunner(args)
runner.summary['scope']='retire current owned fixture; no game behavior regression replay'
try:
    runner.load_manifests()
    if runner.summary['runtime_run_id']!=args.expected_run_id:
        raise AcceptanceError('Owned run differs; no Leave attempted')
    previous={}
    for peer in ('client-1','host'):
        before=runner.single('before-'+peer,peer,'game_read_native_lobby')
        if before.get('admitted') is not True:
            raise AcceptanceError('Expected admitted owned fixture on '+peer)
        previous[peer]=before['prepare']['leave_confirmed']
        button(runner,peer,'leave')
    observe(runner,'retired',lambda peer,value:left(value,previous[peer]))
    runner.summary.update(passed=True,state='owned-fixture-retired')
except Exception as error:
    runner.summary.update(passed=False,state='failed-no-replay',error={'step':runner.summary['step'],'cause':str(error)})
runner.summary['finished_utc']=utc();runner.persist();print(runner.summary_path)
raise SystemExit(0 if runner.summary['passed'] else 1)

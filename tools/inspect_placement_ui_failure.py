"""Observe an attempted native UI placement without repeating its callbacks."""
from argparse import ArgumentParser
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, utc
parser=ArgumentParser(description=__doc__)
parser.add_argument('--frames',action='store_true')
parser.add_argument('--expected-run')
args=parser.parse_args()
args.test_root=ROOT/'_MPTest';args.players=2;args.cleanup=False;args.step_timeout=45
runner=AcceptanceRunner(args)
runner.load_manifests()
if args.expected_run and runner.summary['runtime_run_id']!=args.expected_run:
    raise AcceptanceError('read-only placement diagnostic owned run differs')
for peer in ('host','client-1'):
    state=runner.inspect('read-only-UI-result-'+peer,peer)
    runner.checked('actual-UI-current-state',{'peer':peer,'typed_actions':state['typed_actions'],'ui_build':state['ui_build'],
        'simulation':state['simulation'],'props':[p for p in state['props'] if p.get('building')]})
    if args.frames: runner.capture(peer)
runner.summary.update(passed=True,state='observed-UI-result-no-replay',finished_utc=utc());runner.persist()
print(str(runner.summary_path))

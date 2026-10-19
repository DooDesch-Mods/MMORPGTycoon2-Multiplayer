"""Dismiss the retained native report through MCP and load an untouched owned title."""
import sys
from argparse import Namespace
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, utc, log
args = Namespace(test_root=ROOT / '_MPTest', players=2, cleanup=False, step_timeout=45)
runner = AcceptanceRunner(args)
try:
    runner.load_manifests()
    if runner.summary['runtime_run_id'] != '089524cf-1224-45dc-a693-03a5bafebc42':
        raise AcceptanceError('owned recovery run differs')
    for peer in ('host', 'client-1'):
        engine = runner.inspect('observe-title-' + peer, peer)
        world = engine['native_world']
        if world.get('crash_report_active') is True:
            runner.single('dismiss-retained-native-report-' + peer, peer, 'game_dismiss_crash_report')
        elif world.get('active_mode') != 2:
            raise AcceptanceError('expected untouched title or retained report, actual=' + str(world))
    digest = '6b01c4e1cc8b0ac4958126e4cbff1672da7af0d7bb10cbd957aeb2d0674393a5'
    runner.command('load-current-title-once', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/native_open_acceptance.py'),
        '--reuse-run-id', runner.summary['runtime_run_id'], '--snapshot-id', '41f6e0b5-bcc2-4ef4-b930-ddfe6b3dffe2',
        '--expected-input-bytes', '208949', '--expected-input-sha256', 'fedec7bec52be5c40a362e0552390e804bbe86defc949524c6f9c4a20a519c1a',
        '--expected-dll-sha256', digest, '--players', '2'])
    runner.summary.update(passed=True, state='recovered-owned-placement-fixture', new_run_id=runner.summary['runtime_run_id'])
except Exception as error:
    runner.summary.update(passed=False, state='failed-recovered-title-load', error={'step': runner.summary['step'], 'cause': str(error), 'behavior': 'remaining mutations skipped; no replay'})
    log('error', 'recovered title load: ' + str(error))
runner.summary['finished_utc'] = utc(); runner.persist()
print(str(runner.summary_path))
raise SystemExit(0 if runner.summary['passed'] else 1)

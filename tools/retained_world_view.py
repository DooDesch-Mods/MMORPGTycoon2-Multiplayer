"""Inspect a retained small building after restart and capture its native view."""
import argparse
from pathlib import Path
import uuid

from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, validate_snapshot, compare_ledgers, utc, log
from native_open_acceptance import origin


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-acceptance', type=lambda value: str(uuid.UUID(value)), required=True)
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--step-timeout', type=float, default=45)
    args = parser.parse_args()
    args.players, args.cleanup = 2, False
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'two loaded small worlds: native persistence readback and camera view only'
    try:
        baseline = read_json(ROOT / 'artifacts/acceptance' / args.native_acceptance / 'summary.json')
        if baseline.get('passed') is not True or baseline['parameters']['players'] != 2:
            raise AcceptanceError('native reload baseline must confirm exactly two instances')
        retained = {entry['result']['peer']: entry['result'] for entry in baseline['checks'] if entry['check'] == 'native-placed-building-reloaded'}
        if set(retained) != {'host', 'client-1'}:
            raise AcceptanceError('native building persistence receipts missing')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != baseline['runtime_run_id']:
            raise AcceptanceError('runtime changed since native reload')
        states = {}
        for peer in ('host', 'client-1'):
            if read_json(runner.manifests[peer])['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
                raise AcceptanceError('active DLL differs: ' + peer)
            engine = runner.inspect(peer + '-current-world', peer)
            snapshot_id = baseline['parameters']['snapshot_id'] if peer == 'host' else baseline['output_snapshot_id']
            record = validate_snapshot(read_json(runner.root / 'host/diagnostics' / f'snapshot-{snapshot_id}.json'), snapshot_id, runner.root / 'host')
            runner.checked('current-native-origin', {'peer': peer, 'origin': origin(engine, record)})
            expected = retained[peer]['object']
            matches = [prop for prop in engine.get('props', []) if prop == expected]
            if len(matches) != 1:
                raise AcceptanceError(f'persisted native building changed peer={peer} matches={len(matches)} required=1')
            states[peer] = engine
        ledger = compare_ledgers(states['host'], states['client-1'])
        if any(entry['ledger'] != ledger for entry in retained.values()):
            raise AcceptanceError('persisted ledger changed before camera view')
        for peer in ('host', 'client-1'):
            prop = retained[peer]['object']
            runner.single(peer + '-focus-once', peer, 'game_focus_test_camera', {'x': prop['x'], 'y': prop['y'], 'z': prop['z'], 'distance': 150})
            runner.capture(peer)
        after = {peer: runner.inspect(peer + '-after-view', peer) for peer in ('host', 'client-1')}
        if compare_ledgers(after['host'], after['client-1']) != ledger:
            raise AcceptanceError('ledger changed during camera view')
        for peer in after:
            if after[peer].get('props') != states[peer].get('props'):
                raise AcceptanceError('diagnostic prop readback changed during camera view: ' + peer)
        runner.checked('persisted-building-view-readback', retained)
        runner.summary.update(passed=True, state='confirmed-retained-world-view')
    except Exception as error:
        runner.summary.update(passed=False, state='failed-retained-world-view', error={'step': runner.summary['step'], 'cause': f'{type(error).__name__}: {error}', 'behavior': 'remaining calls skipped; no focus replay'})
        log('error', 'retained world view failed: ' + str(error))
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'retained world view passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

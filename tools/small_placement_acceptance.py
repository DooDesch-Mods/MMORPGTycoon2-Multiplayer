"""Place one diagnostic building in two already loaded small owned worlds."""
import argparse
from pathlib import Path
import uuid

from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, validate_snapshot, compare_ledgers, placement_expense, ready_reason, utc, log
from native_open_acceptance import origin


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--native-acceptance', type=lambda value: str(uuid.UUID(value)), required=True)
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--place-type', default='SmallInn')
    parser.add_argument('--place-variant', default='base')
    parser.add_argument('--x', type=float, default=10850)
    parser.add_argument('--z', type=float, default=6800)
    parser.add_argument('--camera-distance', type=float, default=150)
    parser.add_argument('--step-timeout', type=float, default=45)
    args = parser.parse_args()
    args.players = 2
    args.cleanup = False
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'two small owned worlds: one diagnostic client placement, native object and cost readbacks'
    try:
        baseline = read_json(ROOT / 'artifacts/acceptance' / args.native_acceptance / 'summary.json')
        if baseline.get('passed') is not True or baseline['parameters']['players'] != 2:
            raise AcceptanceError('native baseline must confirm exactly two instances')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != baseline['runtime_run_id']:
            raise AcceptanceError('owned runtime changed since native baseline; no placement attempted')
        states = {}
        for peer in ('host', 'client-1'):
            manifest = read_json(runner.manifests[peer])
            if manifest['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
                raise AcceptanceError('active DLL differs: ' + peer)
            engine = runner.inspect(peer + '-before-placement', peer)
            reason = ready_reason(engine)
            if reason:
                raise AcceptanceError(peer + ' readiness failed: ' + reason)
            snapshot_id = baseline['parameters']['snapshot_id'] if peer == 'host' else baseline['output_snapshot_id']
            record = validate_snapshot(read_json(runner.root / 'host/diagnostics' / f'snapshot-{snapshot_id}.json'), snapshot_id, runner.root / 'host')
            runner.checked('current-native-origin', {'peer': peer, 'origin': origin(engine, record)})
            if engine.get('simulation', {}).get('logged_in_subscribers') != 0:
                raise AcceptanceError('small fixture requires zero logged-in subscribers: ' + peer)
            if engine.get('placement_status') != 'Kein Platzierungsnachweis angefordert':
                raise AcceptanceError('prior diagnostic placement exists; no placement replay')
            states[peer] = engine
        before = compare_ledgers(states['host'], states['client-1'])
        runner.checked('paused-ledgers-before-placement', before)
        parameters = {'type': args.place_type, 'variant': args.place_variant, 'x': args.x, 'z': args.z}
        runner.single('client-readonly-placement-probe', 'client-1', 'game_probe_building_placement', parameters)
        placement = runner.single('client-place-exactly-once', 'client-1', 'game_place_test_building', parameters)
        host = runner.inspect('host-after-placement', 'host')
        client = runner.inspect('client-after-placement', 'client-1')
        after = compare_ledgers(host, client)
        runner.checked('paused-ledgers-after-placement', after)
        runner.checked('one-native-placement-expense', placement_expense(before, after))
        objects = runner.object_readback(host, client, placement)
        for peer in ('host', 'client-1'):
            prop = objects[peer]
            runner.single(peer + '-focus-once', peer, 'game_focus_test_camera',
                          {'x': prop['x'], 'y': prop['y'], 'z': prop['z'], 'distance': args.camera_distance})
            runner.capture(peer)
        runner.summary.update(passed=True, state='confirmed-small-diagnostic-placement')
    except Exception as error:
        runner.summary.update(passed=False, state='failed-small-diagnostic-placement',
                              error={'step': runner.summary['step'], 'cause': f'{type(error).__name__}: {error}',
                                     'behavior': 'remaining mutations skipped; no placement replay'})
        log('error', 'small placement failed: ' + str(error))
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'small placement passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

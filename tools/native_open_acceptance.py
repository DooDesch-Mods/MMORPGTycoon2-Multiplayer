"""Prove actual native file/worker origins using the retained small owned world."""
import argparse
import math
from pathlib import Path
import sys
import uuid
from local_acceptance import (AcceptanceRunner, AcceptanceError, ROOT, read_json, validate_snapshot,
                              ledger_state, compare_ledgers, utc, log)


def origin(engine, snapshot):
    value = engine.get('native_open_origin', {})
    for field in ('initialized', 'native_request_attempted', 'native_task_retired', 'origin_confirmed',
                  'reader_confirmed', 'completed_origin', 'world_witness_bound', 'current_origin_available',
                  'fresh_current_world_attempted', 'fresh_current_world_readback'):
        if value.get(field) is not True:
            raise AcceptanceError(f'native origin guard failed field={field} actual={value.get(field)} original={value}')
    if value.get('hook_mask') != 15 or value.get('held') is not False or value.get('error'):
        raise AcceptanceError(f'native origin hook/pin/error guard failed: {value}')
    for field in ('file_calls', 'file_returns', 'stream_calls', 'stream_returns', 'reader_calls',
                  'reader_returns', 'worker_calls', 'worker_returns', 'pin_close_calls', 'pin_close_confirmed'):
        if type(value.get(field)) is not int or value[field] != 1:
            raise AcceptanceError(f'native origin exactly-once guard failed field={field} actual={value.get(field)}')
    if type(snapshot.get('bytes')) is not int or snapshot['bytes'] <= 0:
        raise AcceptanceError('native origin expected file size must be a positive integer')
    for field in ('bytes', 'hash_bytes'):
        if type(value.get(field)) is not int or value[field] <= 0:
            raise AcceptanceError(f'native origin file size guard failed field={field} actual={value.get(field)} required=positive integer')
    for field, expected in (('uuid', snapshot['snapshot_id']), ('sha256', snapshot['sha256']),
                            ('bytes', snapshot['bytes']), ('hash_bytes', snapshot['bytes'])):
        if value.get(field) != expected:
            raise AcceptanceError(f'native origin file identity differs field={field} actual={value.get(field)} required={expected}')
    if type(value.get('world_epoch')) is not int or type(value.get('origin_world_epoch')) is not int or value['world_epoch'] <= 0 or value['world_epoch'] != value['origin_world_epoch']:
        raise AcceptanceError('native local world epoch differs from current origin')
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--snapshot-id', type=lambda value: str(uuid.UUID(value)),
                        default='f5acffea-9a64-49cd-8d06-1a5aa4ad6f4c')
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--expected-input-bytes', type=int, default=211573)
    parser.add_argument('--expected-input-sha256', default='e72e7777c65bfc75a7f6cbb613ecd0604f871ef4fd292526a7b4f6f00a043adf')
    parser.add_argument('--step-timeout', type=float, default=45)
    parser.add_argument('--ready-timeout', type=float, default=60)
    parser.add_argument('--players', type=int, choices=[2, 4], default=2)
    parser.add_argument('--reuse-run-id', type=lambda value: str(uuid.UUID(value)))
    parser.add_argument('--retained-capture', type=lambda value: str(uuid.UUID(value)))
    parser.add_argument('--purchase-region-x', type=float)
    parser.add_argument('--purchase-region-z', type=float)
    parser.add_argument('--capture-title-frames', action='store_true')
    args = parser.parse_args()
    region = (args.purchase_region_x, args.purchase_region_z)
    if (region[0] is None) != (region[1] is None) or any(value is not None and not math.isfinite(value) for value in region):
        parser.error('purchase region requires both finite x and z')
    if region[0] is not None and args.players != 2:
        parser.error('functional region preparation requires exactly two instances')
    args.cleanup = False
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'small world native Host/Client file origin and exact task retirement'
    try:
        source = read_json(runner.root / 'host/diagnostics' / f'snapshot-{args.snapshot_id}.json')
        source = validate_snapshot(source, args.snapshot_id, runner.root / 'host')
        if source['bytes'] != args.expected_input_bytes or source['sha256'] != args.expected_input_sha256:
            raise AcceptanceError('retained small fixture size/SHA differs; no process or game effect attempted')
        runner.checked('small-input-snapshot', source)
        persistence = None
        if args.retained_capture:
            saved = read_json(ROOT / 'artifacts/acceptance' / args.retained_capture / 'summary.json')
            if saved.get('passed') is not True or saved.get('state') != 'confirmed-small-world-local-file' or saved.get('output_snapshot_id') != args.snapshot_id or saved.get('output_snapshot_sha256') != source['sha256'] or saved.get('output_snapshot_bytes') != source['bytes']:
                raise AcceptanceError('retained capture file does not match requested reload; no game effect attempted')
            placed = read_json(ROOT / 'artifacts/acceptance' / saved['parameters']['placement'] / 'summary.json')
            if placed.get('passed') is not True or placed.get('runtime_run_id') != saved.get('runtime_run_id'):
                raise AcceptanceError('retained placement baseline differs from save')
            persistence = {
                'ledger': next(entry['result'] for entry in placed['checks'] if entry['check'] == 'paused-ledgers-after-placement'),
                'object': next(entry['result']['objects']['host'] for entry in placed['checks'] if entry['check'] == 'placed-object-native-readback'),
            }
            if region[0] is not None:
                raise AcceptanceError('persistence reload must not purchase another region')
        if args.reuse_run_id is None:
            runner.command('stop-owned', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/stop_local_test.py'), '--test-root', str(runner.root)])
            runner.command('start-owned', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/local_test.py'), '--test-root', str(runner.root), '--ready-timeout', str(args.ready_timeout), '--players', str(args.players)])
        runner.load_manifests()
        if args.reuse_run_id is not None:
            if runner.summary['runtime_run_id'] != args.reuse_run_id or read_json(runner.root / 'run.json').get('players') != args.players:
                raise AcceptanceError('reused run identity or player count differs; no load attempted')
            runner.checked('owned-title-run-reused', {'run_id': args.reuse_run_id, 'players': args.players, 'process_restart_skipped': True})
        peers = ['host', *[f'client-{peer}' for peer in range(1, args.players)]]
        for peer in peers:
            manifest = read_json(runner.manifests[peer])
            if manifest['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
                raise AcceptanceError(f'running native origin DLL differs peer={peer}')
            runner.poll_title(peer)
            if args.capture_title_frames:
                frame = runner.single(peer + '-title-frame', peer, 'game_capture_frame')
                runner.checked('native-title-frame-' + peer, frame)
        loaded = runner.single('host-load-once', 'host', 'game_load_snapshot', {'snapshot_id': args.snapshot_id})
        if loaded.get('state') != 'snapshot_world_active':
            raise AcceptanceError('host world load not acknowledged')
        host = runner.inspect('host-origin-readback', 'host')
        runner.checked('host-current-native-origin', origin(host, source))
        runner.checked('host-paused-ledger', ledger_state(host))
        if region[0] is not None:
            before_cash = ledger_state(host)['cash']
            purchased = runner.single('host-purchase-test-region-once', 'host', 'game_purchase_test_region',
                                      {'x': region[0], 'z': region[1]})
            if purchased.get('state') != 'test_region_owned' or purchased.get('owned') is not True:
                raise AcceptanceError('native test region ownership not acknowledged; no purchase replay')
            price = purchased.get('purchase_price')
            if not isinstance(price, int) or isinstance(price, bool) or price < 0 or purchased.get('cash') != before_cash - price:
                raise AcceptanceError('native test region cash/price differs; no purchase replay')
            if purchased.get('building_manager_present') is not True or purchased.get('path_manager_present') is not True:
                raise AcceptanceError('native test region managers not confirmed')
            runner.checked('native-test-region-purchased', purchased)
        captured = runner.single('host-capture-once', 'host', 'game_capture_snapshot')
        captured = validate_snapshot(captured, captured['snapshot_id'], runner.root / 'host')
        runner.checked('host-output-snapshot', captured)
        clients = {}
        for peer in peers[1:]:
            clients[peer] = runner.poll_client(captured['snapshot_id'], peer)
            runner.checked('client-current-native-origin', {'peer': peer, 'origin': origin(clients[peer], captured)})
        host = runner.inspect('host-after-client-load', 'host')
        runner.checked('host-origin-preserved-through-save', origin(host, source))
        for peer, client in clients.items():
            runner.checked('initial-host-client-ledger', {'peer': peer, 'ledger': compare_ledgers(host, client)})
        if persistence:
            original = persistence['object']
            for peer, engine in [('host', host), *clients.items()]:
                if ledger_state(engine) != persistence['ledger']:
                    raise AcceptanceError('native persisted ledger differs: ' + peer)
                matches = [prop for prop in engine.get('props', []) if prop.get('building') is True and prop.get('type') == original['type'] and prop.get('variant') == original['variant'] and all(abs(prop[field] - original[field]) < 0.001 for field in ('x', 'y', 'z'))]
                if len(matches) != 1:
                    raise AcceptanceError(f'persisted diagnostic building differs peer={peer} matches={len(matches)} required=1')
                runner.checked('native-placed-building-reloaded', {'peer': peer, 'object': matches[0], 'ledger': persistence['ledger']})
        for peer in peers:
            runner.capture(peer)
        runner.summary.update(state='confirmed-native-open-small', passed=True,
                              output_snapshot_id=captured['snapshot_id'], output_snapshot_bytes=captured['bytes'])
    except Exception as error:
        runner.summary.update(state='failed-native-open-small', passed=False,
                              error={'step': runner.summary['step'], 'cause': f'{type(error).__name__}: {error}',
                                     'behavior': 'remaining mutations skipped; no load replay'})
        log('error', f'native origin acceptance failed: {error}')
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'native origin passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

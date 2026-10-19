"""Retain the placed small world without distributing it to active clients."""
import argparse
from pathlib import Path
import uuid

from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, validate_snapshot, compare_ledgers, utc, log


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--placement', type=lambda value: str(uuid.UUID(value)), required=True)
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--step-timeout', type=float, default=45)
    parser.add_argument('--ready-timeout', type=float, default=60)
    parser.add_argument('--both-peers', action='store_true')
    args = parser.parse_args()
    args.cleanup = False
    args.players = 2
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'two small owned worlds: one local native save without client reload'
    try:
        baseline = read_json(ROOT / 'artifacts/acceptance' / args.placement / 'summary.json')
        if baseline['passed'] is not True:
            raise AcceptanceError('placement baseline is not confirmed')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != baseline['runtime_run_id']:
            raise AcceptanceError('owned runtime changed since placement baseline')
        for peer in runner.manifests:
            if read_json(runner.manifests[peer])['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
                raise AcceptanceError('active DLL differs: ' + peer)
        previous = {peer: runner.inspect(peer + '-before-retain', peer) for peer in ('host', 'client-1')}
        before = compare_ledgers(previous['host'], previous['client-1'])
        ui_proof = baseline.get('state') == 'confirmed-normal-building-UI-two'
        proof = next(entry['result'] for entry in baseline['checks'] if entry['check'] == ('normal-UI-building-confirmed' if ui_proof else 'placed-object-native-readback'))
        expected = proof['ledger'] if ui_proof else next(entry['result'] for entry in baseline['checks'] if entry['check'] == 'paused-ledgers-after-placement')
        if before != expected:
            raise AcceptanceError('financial state changed since placement baseline')
        captured = runner.single('host-local-save-exactly-once', 'host', 'game_capture_local_snapshot')
        captured = validate_snapshot(captured, captured['snapshot_id'], runner.root / 'host')
        if captured.get('distribution_requested') is not False or captured.get('run_id') != runner.summary['runtime_run_id'] or captured.get('state') != 'snapshot_file_complete':
            raise AcceptanceError('local file capture identity/distribution/state differs; no save replay')
        runner.checked('native-placed-world-file', captured)
        if args.both_peers:
            client_file = runner.single('client-local-save-exactly-once', 'client-1', 'game_capture_local_snapshot')
            client_file = validate_snapshot(client_file, client_file['snapshot_id'], runner.root / 'client-1')
            if client_file.get('distribution_requested') is not False or client_file.get('state') != 'snapshot_file_complete':
                raise AcceptanceError('Client local Save receipt differs; no save replay')
            runner.checked('native-client-world-file', client_file)
        client = runner.inspect('client-after-local-save', 'client-1')
        host = runner.inspect('host-after-retain', 'host')
        if compare_ledgers(host, client) != before:
            raise AcceptanceError('financial state differs after local save')
        original = proof['objects']['host']
        for peer, engine in (('host', host), ('client-1', client)):
            old_origin, new_origin = previous[peer].get('native_open_origin', {}), engine.get('native_open_origin', {})
            for field in ('generation', 'uuid', 'sha256', 'bytes', 'world_epoch', 'origin_world_epoch', 'worker_calls'):
                if old_origin.get(field) != new_origin.get(field):
                    raise AcceptanceError(f'loaded world origin changed after local save peer={peer} field={field}')
            if new_origin.get('fresh_current_world_readback') is not True or new_origin.get('current_origin_available') is not True:
                raise AcceptanceError(f'fresh current world readback failed after local save peer={peer}')
            if previous[peer].get('snapshot_progress') != engine.get('snapshot_progress'):
                raise AcceptanceError(f'network snapshot state changed after local save peer={peer}')
            matches = [prop for prop in engine['props'] if prop.get('building') is True and prop.get('type') == original['type'] and prop.get('variant') == original['variant'] and all(abs(prop[field] - original[field]) < 0.001 for field in ('x', 'y', 'z'))]
            if len(matches) != 1:
                raise AcceptanceError(f'placed building persistence differs peer={peer} matches={len(matches)} required=1')
            runner.checked('native-placed-building-retained', {'peer': peer, 'object': matches[0]})
        runner.summary.update(passed=True, state='confirmed-small-world-local-file', native_reload_proven=False, output_snapshot_id=captured['snapshot_id'], output_snapshot_bytes=captured['bytes'], output_snapshot_sha256=captured['sha256'])
    except Exception as error:
        runner.summary.update(passed=False, state='failed-small-world-retain', error={'step': runner.summary['step'], 'cause': f'{type(error).__name__}: {error}', 'behavior': 'remaining mutations skipped; no save replay'})
        log('error', 'small world retain failed: ' + str(error))
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'small world retain passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

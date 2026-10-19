"""Check only the new native holder/free path in an already loaded owned two-peer world."""
import argparse
from pathlib import Path
import uuid
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, ledger_state, utc, log


def confirmed(value, token, kind, epoch, generation):
    if value.get('token') != token or value.get('kind') != kind:
        raise AcceptanceError('holder receipt token or kind differs from current request')
    for field, expected in (('world_epoch', epoch), ('world_generation', generation)):
        if type(value.get(field)) is not int or value[field] != expected or expected <= 0:
            raise AcceptanceError(f'holder receipt binding differs field={field}')
    for field in ('confirmed', 'attempted'):
        if value.get(field) is not True:
            raise AcceptanceError(f'holder guard {field} actual={value.get(field)} required=True')
    for field in ('factory_attempted', 'factory_returned', 'constructor_confirmed'):
        if type(value.get(field)) is not int or value[field] != 1:
            raise AcceptanceError(f'holder count {field} actual={value.get(field)} required=1')
    deletion = value.get('deletion', {})
    for field in ('delete_attempted', 'delete_returned', 'sized_attempted', 'sized_returned', 'free_attempted', 'free_returned'):
        if type(deletion.get(field)) is not int or deletion[field] != 1:
            raise AcceptanceError(f'free count {field} actual={deletion.get(field)} required=1')
    if type(deletion.get('phase')) is not int or deletion['phase'] != 3 or type(deletion.get('seh_code')) is not int or deletion['seh_code'] != 0 or value.get('error'):
        raise AcceptanceError(f'holder native free incomplete: {value}')
    runtime = value.get('runtime', {})
    if type(runtime.get('phase')) is not int or runtime['phase'] != 2 or type(runtime.get('hook_mask')) is not int or runtime['hook_mask'] != 3 or any(runtime.get(key) is not True for key in (
        'crt_confirmed', 'hooks_confirmed', 'originals_published', 'trampoline_route', 'iat_protection_restored')):
        raise AcceptanceError(f'holder installation not confirmed: {runtime}')
    for field in ('allocation_generation', 'call'):
        if type(value.get(field)) is not int or type(deletion.get(field)) is not int or value[field] <= 0 or value[field] != deletion[field]:
            raise AcceptanceError(f'holder own allocation/call mismatch field={field}')
    for item, field, expected in ((deletion, 'world_generation', generation),
                                  (runtime, 'world_epoch', epoch), (runtime, 'world_generation', generation)):
        if type(item.get(field)) is not int or item[field] != expected:
            raise AcceptanceError(f'holder installation or free world differs field={field}')
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--expected-run-id', required=True, type=lambda value: str(uuid.UUID(value)))
    parser.add_argument('--step-timeout', type=float, default=45)
    args = parser.parse_args()
    args.players = 2
    args.cleanup = False
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'genuine private null holder Factory/readback/D0/sized/free on two current peers; no gameplay remove'
    try:
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != args.expected_run_id or read_json(runner.root / 'run.json').get('players') != 2:
            raise AcceptanceError('exact owned two-peer run differs; no probe attempted')
        for peer in ('host', 'client-1'):
            manifest = read_json(runner.manifests[peer])
            if manifest['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
                raise AcceptanceError(f'DLL differs peer={peer}; no probe attempted')
        for peer in ('host', 'client-1'):
            before = runner.inspect(f'{peer}-probe-baseline', peer)
            origin = before.get('native_open_origin', {})
            if origin.get('fresh_current_world_readback') is not True or origin.get('current_origin_available') is not True:
                raise AcceptanceError(f'fresh NativeOpen origin unavailable peer={peer}')
            epoch, generation = origin.get('world_epoch'), origin.get('generation')
            if type(epoch) is not int or epoch <= 0 or type(generation) is not int or generation <= 0:
                raise AcceptanceError(f'fresh NativeOpen epoch/generation invalid peer={peer}: {origin}')
            previous_generation = previous_call = 0
            for kind in ('prop', 'building'):
                token = str(uuid.uuid4())
                result = runner.single(f'{peer}-{kind}-holder-once', peer, 'game_probe_native_holder',
                    {'token': token, 'kind': kind, 'expected_world_epoch': epoch, 'expected_world_generation': generation})
                runner.checked('native-holder-confirmed', {'peer': peer, 'kind': kind, 'receipt': confirmed(result, token, kind, epoch, generation)})
                if result['allocation_generation'] <= previous_generation or result['call'] <= previous_call:
                    raise AcceptanceError(f'shared allocation/free sequence reused peer={peer}')
                previous_generation, previous_call = result['allocation_generation'], result['call']
            after = runner.inspect(f'{peer}-probe-after', peer)
            if ledger_state(before) != ledger_state(after) or before.get('props') != after.get('props'):
                raise AcceptanceError(f'private native probe altered paused gameplay peer={peer}; no retry')
            retained = runner.single(f'{peer}-probe-receipts', peer, 'game_read_native_free')
            receipts = retained.get('probe_receipts', {})
            if len(receipts) != 2 or any(value.get('confirmed') is not True for value in receipts.values()):
                raise AcceptanceError(f'current probe receipts incomplete peer={peer}')
            runner.checked('native-probe-gameplay-preserved', {'peer': peer, 'ledger': ledger_state(after), 'receipts': retained})
            runner.capture(peer)
        runner.summary.update(state='confirmed-native-free-two', passed=True, native_holder_free_proven=True,
                              gameplay_remove_proven=False)
    except Exception as failure:
        runner.summary.update(state='failed-native-free-two', passed=False,
            error={'step': runner.summary['step'], 'cause': f'{type(failure).__name__}: {failure}',
                   'behavior': 'remaining mutations skipped; no holder factory or D0 replay'})
        log('error', f'native Free acceptance failed: {failure}')
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'native Free passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

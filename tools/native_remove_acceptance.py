"""Prove one native snapshot-bound SmallInn removal on each current peer, without transport claims."""
import argparse
from pathlib import Path
import uuid
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, ledger_state, utc, log


def target(value):
    if any(type(value.get(field)) is not int or value[field] <= 0 for field in ('world_epoch', 'world_generation')):
        raise AcceptanceError('native remove binding lacks exact current world')
    found = [item for item in value.get('bindings', []) if item.get('type') == 'SmallInn' and item.get('variant') == 'base'
             and item.get('revoked') is False and item.get('unknown') is False]
    if len(found) != 1:
        raise AcceptanceError(f'native remove requires one current SmallInn, actual={len(found)}')
    result = found[0]
    if type(result.get('local_uid')) is not int or result['local_uid'] <= 0 or not result.get('snapshot_id') or not result.get('record_key'):
        raise AcceptanceError('native remove binding metadata incomplete')
    return result


def retirement_confirmed(value, token, sequence):
    if value.get('token') != token or type(value.get('sequence')) is not int or value['sequence'] != sequence or value.get('confirmed') is not True or value.get('error'):
        raise AcceptanceError('native retirement token/sequence/confirmation differs; no replay')
    result, effects = value.get('result', {}), value.get('effects', {})
    pool = result.get('pool', {})
    if result.get('retirement_confirmed') is not True or result.get('unknown_locked') is not False or pool.get('unknown_lock') is not False:
        raise AcceptanceError('native retirement status Unknown or unconfirmed; no replay')
    for field, expected in (('leases', 1), ('held', 0), ('tombstones', 0), ('released', 1)):
        if type(pool.get(field)) is not int or pool[field] != expected:
            raise AcceptanceError('native retirement per-holder pool differs: ' + field)
    if effects.get('attempted') is not True or effects.get('confirmed') is not True or effects.get('unknown') is not False or type(effects.get('releases')) is not int or effects['releases'] != 1:
        raise AcceptanceError('native retirement exact release coverage absent; no replay')
    frees = effects.get('frees')
    if not isinstance(frees, list) or len(frees) != 1:
        raise AcceptanceError('native retirement own FreeACK coverage absent')
    ack = frees[0]
    for field in ('allocation_generation', 'world_generation', 'retirement_receipt', 'call'):
        if type(ack.get(field)) is not int or ack[field] <= 0:
            raise AcceptanceError('native retirement actual FreeACK binding absent: ' + field)
    for field, expected in [('phase', 3), ('bytes', 56), ('seh_code', 0), *[(key, 1) for key in ('delete_attempted', 'delete_returned', 'sized_attempted', 'sized_returned', 'free_attempted', 'free_returned')]]:
        if type(ack.get(field)) is not int or ack[field] != expected:
            raise AcceptanceError('native retirement current FreeACK counter differs: ' + field)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--expected-run-id', required=True, type=lambda value: str(uuid.UUID(value)))
    parser.add_argument('--step-timeout', type=float, default=45)
    parser.add_argument('--retire', action='store_true', help='Prove the new native retirement path; removal is its required fixture')
    args = parser.parse_args()
    args.players = 2
    args.cleanup = False
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'one own native SmallInn DestroyProp per peer; snapshot binding, graph/destructor/WeakNULL and selection; no client-request replication or holder retirement claim'
    if args.retire:
        runner.summary['scope'] = 'new per-holder native retirement on both peers; confirmed Remove is the required fixture and its previous acceptance is carried forward'
    try:
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != args.expected_run_id or read_json(runner.root / 'run.json').get('players') != 2:
            raise AcceptanceError('exact owned two-peer run differs; no remove attempted')
        sources, targets = {}, {}
        for peer in ('host', 'client-1'):
            manifest = read_json(runner.manifests[peer])
            if manifest['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
                raise AcceptanceError(f'DLL differs peer={peer}; no remove attempted')
            sources[peer] = runner.single(f'{peer}-remove-bindings', peer, 'game_read_remove_bindings')
            targets[peer] = target(sources[peer])
        for field in ('snapshot_id', 'record_key', 'type', 'variant', 'x', 'y', 'z'):
            if targets['host'][field] != targets['client-1'][field]:
                raise AcceptanceError(f'host/client native target differs field={field}; no remove attempted')
        runner.checked('current-native-target-bindings', targets)
        after = {}
        for peer in ('host', 'client-1'):
            item, source = targets[peer], sources[peer]
            runner.single(f'{peer}-remove-focus-once', peer, 'game_focus_test_camera',
                          {'x': item['x'], 'y': item['y'], 'z': item['z'], 'distance': 150})
            runner.capture(peer)
            before = runner.inspect(f'{peer}-remove-before', peer)
            token = str(uuid.uuid4())
            removed = runner.single(f'{peer}-native-remove-once', peer, 'game_probe_remove_snapshot_building',
                {'token': token, 'snapshot_id': item['snapshot_id'], 'record_key': item['record_key'],
                 'expected_world_epoch': source['world_epoch'], 'expected_world_generation': source['world_generation']})
            if removed.get('token') != token or removed.get('confirmed') is not True or removed.get('binding_revoked') is not True:
                raise AcceptanceError(f'native remove acknowledgement incomplete peer={peer}; no replay')
            result = removed.get('result', {})
            if any(result.get(field) is not True for field in ('attempted', 'returned', 'confirmed', 'target_retired', 'undo_confirmed', 'selection_confirmed')):
                raise AcceptanceError(f'native remove graph/lifetime/selection incomplete peer={peer}: {result}')
            if result.get('unknown_locked') is not False or result.get('error') or type(result.get('seh_code')) is not int or result['seh_code'] != 0:
                raise AcceptanceError(f'native remove unknown/fault peer={peer}: {result}')
            if result.get('retirement_confirmed') is not False or result.get('pool', {}).get('tombstones') != 1:
                raise AcceptanceError(f'native remove ownership receipt differs peer={peer}: {result}')
            runner.checked('native-remove-confirmed', {'peer': peer, 'receipt': removed})
            if args.retire:
                retired = runner.single(f'{peer}-native-retirement-once', peer, 'game_retire_snapshot_remove', {'token': token})
                runner.checked('native-retirement-confirmed', {'peer': peer, 'receipt': retirement_confirmed(retired, token, removed['sequence'])})
            after[peer] = runner.inspect(f'{peer}-remove-after', peer)
            old = [prop for prop in before.get('props', []) if prop.get('uid') == item['local_uid']]
            remaining = [prop for prop in after[peer].get('props', []) if prop.get('uid') == item['local_uid']]
            if len(old) != 1 or remaining:
                raise AcceptanceError(f'native target presence before/after differs peer={peer} before={len(old)} after={len(remaining)}')
            runner.checked('native-target-absent', {'peer': peer, 'local_uid': item['local_uid'], 'ledger': ledger_state(after[peer])})
            runner.capture(peer)
        if ledger_state(after['host']) != ledger_state(after['client-1']):
            raise AcceptanceError('native removal ledgers differ between peers')
        runner.summary.update(state='confirmed-native-remove-two', passed=True, native_remove_proven=True,
            client_request_replication_proven=False, holder_retirement_proven=args.retire)
        if args.retire:
            runner.summary['state']='confirmed-native-retirement-two'
    except Exception as failure:
        runner.summary.update(state='failed-native-remove-two', passed=False,
            error={'step': runner.summary['step'], 'cause': f'{type(failure).__name__}: {failure}',
                   'behavior': 'remaining mutations skipped; no DestroyProp replay'})
        log('error', f'native remove acceptance failed: {failure}')
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'native remove passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

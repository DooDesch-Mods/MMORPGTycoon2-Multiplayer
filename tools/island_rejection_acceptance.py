"""Prove native rejection of the known water point without a building or purchase."""
import argparse
from pathlib import Path
import uuid

from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, compare_ledgers, extract_result, utc, log


def expected_rejection(runner, name, peer, tool, arguments):
    try:
        runner.single(name, peer, tool, arguments)
    except AcceptanceError as error:
        job = runner.summary['jobs'][-1]
        if job.get('state') != 'failed' or not job.get('evidence_path'):
            raise
        receipt = read_json(job['evidence_path'])
        bound = extract_result(receipt, 'bind_runtime')
        manifest = read_json(runner.manifests[peer])
        for field in ('runtime_root', 'executable_path'):
            manifest[field] = str(Path(manifest[field]).resolve())
        if bound.get('process_identity_verified') is not True or bound.get('bound') != manifest:
            raise AcceptanceError('expected rejection has no exact owned binding') from error
        calls = [call for call in receipt['calls'] if call['tool'] == tool]
        if len(calls) != 1 or calls[0].get('result', {}).get('isError') is not True:
            raise AcceptanceError('expected rejection has no single failed native response') from error
        native = calls[0]['result']['structuredContent']['native_response']
        message = native.get('error', {}).get('message', '')
        required = ['stage=unwalkable: no native island at position', 'exception=0 ', 'purchase_applied=0 ',
                    'region_active=1 ', 'island_lookup_returned=1 ', 'island_found=0 ',
                    'island_unlock_exception=0 ', 'island_lock_unknown=0']
        if native.get('ok') is not False or native.get('run_id') != runner.summary['runtime_run_id'] or any(text not in message for text in required):
            raise AcceptanceError('rejection has a different native cause: ' + message) from error
        runner.checked('expected-native-unwalkable-rejection', {'peer': peer, 'tool': tool, 'native': native})
        log('info', f'expected unwalkable rejection peer={peer} tool={tool}; original failed response retained')
        return
    raise AcceptanceError('known water point was accepted; no rejection proof')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-acceptance', type=lambda value: str(uuid.UUID(value)), required=True)
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--step-timeout', type=float, default=30)
    args = parser.parse_args()
    args.cleanup = False
    args.players = 2
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'two small owned worlds: known water probes and one invalid client placement rejected before purchase'
    try:
        baseline = read_json(ROOT / 'artifacts/acceptance' / args.native_acceptance / 'summary.json')
        if baseline.get('passed') is not True or baseline['parameters']['players'] != 2:
            raise AcceptanceError('native baseline does not confirm exactly two instances')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != baseline['runtime_run_id']:
            raise AcceptanceError('owned runtime changed since native baseline')
        before = {}
        for peer in ('host', 'client-1'):
            if read_json(runner.manifests[peer])['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
                raise AcceptanceError('active DLL differs: ' + peer)
            before[peer] = runner.inspect(peer + '-before-island-probe', peer)
        ledger = compare_ledgers(before['host'], before['client-1'])
        arguments = {'type': 'SmallInn', 'variant': 'base', 'x': 10850, 'z': 6800}
        for peer in ('host', 'client-1'):
            expected_rejection(runner, peer + '-water-probe-once', peer, 'game_probe_building_placement', arguments)
        expected_rejection(runner, 'client-invalid-place-once', 'client-1', 'game_place_test_building', arguments)
        after = {peer: runner.inspect(peer + '-after-island-rejection', peer) for peer in ('host', 'client-1')}
        if compare_ledgers(after['host'], after['client-1']) != ledger:
            raise AcceptanceError('rejected placement changed financial state')
        for peer in before:
            if after[peer]['props'] != before[peer]['props'] or after[peer]['native_world']['edits'] != 0:
                raise AcceptanceError('rejected placement changed diagnostic object list or edit state: ' + peer)
        runner.checked('no-purchase-or-object-effect', {'ledger': ledger, 'diagnostic_props_unchanged': True,
                                                      'full_object_census_proven': False})
        runner.summary.update(passed=True, state='confirmed-native-island-rejection')
    except Exception as error:
        runner.summary.update(passed=False, state='failed-native-island-rejection',
                              error={'step': runner.summary['step'], 'cause': f'{type(error).__name__}: {error}',
                                     'behavior': 'remaining operations skipped; no placement replay'})
        log('error', 'island rejection acceptance failed: ' + str(error))
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'island rejection passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

"""Read candidate positions in an owned small world until native validation accepts one."""
import argparse
from pathlib import Path
import uuid

from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, compare_ledgers, utc, log


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-acceptance', type=lambda value: str(uuid.UUID(value)), required=True)
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--candidates', type=Path, required=True)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--step-timeout', type=float, default=30)
    args = parser.parse_args()
    args.cleanup = False
    args.players = 2
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'two small owned worlds: read-only native ground, ownership and intersection probes'
    try:
        candidates = read_json(args.candidates)
        if not isinstance(candidates, list) or not 1 <= len(candidates) <= 16:
            raise AcceptanceError('candidate diagnostic batch must contain 1..16 positions')
        baseline = read_json(ROOT / 'artifacts/acceptance' / args.native_acceptance / 'summary.json')
        runner.load_manifests()
        if baseline.get('passed') is not True or baseline['parameters']['players'] != 2 or baseline['runtime_run_id'] != runner.summary['runtime_run_id']:
            raise AcceptanceError('exact two-peer native baseline differs')
        for peer in runner.manifests:
            if read_json(runner.manifests[peer])['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
                raise AcceptanceError('active DLL differs: ' + peer)
        before = compare_ledgers(runner.inspect('host-before-search', 'host'), runner.inspect('client-before-search', 'client-1'))
        accepted = None
        for index, position in enumerate(candidates):
            if not isinstance(position, dict) or set(position) != {'x', 'z'}:
                raise AcceptanceError('candidate position requires exactly x,z')
            arguments = {'type': 'SmallInn', 'variant': 'base', **position}
            try:
                grounded = runner.single(f'host-position-{index:02d}', 'host', 'game_probe_building_placement', arguments)
            except AcceptanceError as error:
                job = runner.summary['jobs'][-1]
                if job.get('state') != 'failed' or not job.get('evidence_path'):
                    raise
                receipt = read_json(job['evidence_path'])
                calls = [call for call in receipt['calls'] if call['tool'] == 'game_probe_building_placement']
                native = calls[0].get('result', {}).get('structuredContent', {}).get('native_response', {}) if len(calls) == 1 else {}
                message = native.get('error', {}).get('message', '')
                reasons = ['stage=unwalkable: no native island at position', 'stage=region inactive', 'stage=building intersection', 'stage=path intersection']
                if native.get('run_id') != runner.summary['runtime_run_id'] or native.get('error', {}).get('code') != 'PLACEMENT_PROBE_REJECTED' or 'exception=0 ' not in message or 'purchase_applied=0 ' not in message or 'island_unlock_exception=0 ' not in message or 'island_lock_unknown=0' not in message or not any(reason in message for reason in reasons):
                    raise AcceptanceError('native search stopped on a different failure: ' + message) from error
                runner.checked('candidate-rejected-without-purchase', {'position': position, 'native': native})
                log('info', f'candidate rejected position={position} cause={message}; next read changes coordinates')
                continue
            if grounded.get('state') != 'placement_validated_without_mutation':
                raise AcceptanceError('successful probe lacks native no-mutation acknowledgement')
            client = runner.single('client-confirm-ground', 'client-1', 'game_probe_building_placement', arguments)
            if client != grounded:
                raise AcceptanceError('client and host grounded probe differ')
            accepted = grounded
            runner.checked('native-grounded-position-on-both-peers', grounded)
            break
        after = compare_ledgers(runner.inspect('host-after-search', 'host'), runner.inspect('client-after-search', 'client-1'))
        if before != after:
            raise AcceptanceError('read-only search changed ledger')
        runner.checked('read-only-ledger-preserved', after)
        if accepted is None:
            raise AcceptanceError('no position in the bounded diagnostic batch passed native validation')
        runner.summary.update(passed=True, state='confirmed-native-grounded-position', position=accepted)
    except Exception as error:
        runner.summary.update(passed=False, state='failed-native-ground-search',
                              error={'step': runner.summary['step'], 'cause': f'{type(error).__name__}: {error}', 'behavior': 'remaining reads skipped; no placement attempted'})
        log('error', 'ground search failed: ' + str(error))
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'ground search passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

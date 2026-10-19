"""Capture read-only current evidence after a bounded native origin acceptance."""
import argparse
from pathlib import Path
import sys
import uuid
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, utc, log
from native_open_acceptance import origin


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--acceptance', type=lambda value: str(uuid.UUID(value)), required=True)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--step-timeout', type=float, default=45)
    args = parser.parse_args()
    args.cleanup = False
    source_path = ROOT / 'artifacts/acceptance' / args.acceptance / 'summary.json'
    source = read_json(source_path)
    snapshots = {entry['check']: entry['result'] for entry in source['checks']
                 if entry['check'] in ('small-input-snapshot', 'host-output-snapshot')}
    if len(snapshots) != 2:
        raise AcceptanceError('original source/output receipts absent; no game operation attempted')
    runner = AcceptanceRunner(args)
    runner.summary.update(scope='current native origin and visual readback only; no world mutation',
                          source_acceptance_id=args.acceptance, source_acceptance_state=source['state'])
    try:
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != source['runtime_run_id']:
            raise AcceptanceError('native origin run changed; no readback attempted')
        states = {}
        for peer, record in (('host', snapshots['small-input-snapshot']), ('client-1', snapshots['host-output-snapshot'])):
            state = runner.inspect(f'{peer}-current-origin', peer)
            states[peer] = state
            runner.checked(f'{peer}-current-native-origin', origin(state, record))
            runner.capture(peer)
        runner.checked('current-calendar-readback', {peer: state['clock'] for peer, state in states.items()})
        runner.summary.update(state='confirmed-read-only-origin', passed=True, native_origin_proven=True,
                              ledger_equality_proven=False, full_coop_proven=False)
    except Exception as error:
        runner.summary.update(state='failed-read-only-origin', passed=False,
                              error={'cause': f'{type(error).__name__}: {error}', 'behavior': 'remaining reads skipped; no load replay'})
        log('error', f'native origin readback failed: {error}')
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'native origin readback passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

"""Retain the completed UI action and restart the owned fixture for Move."""
import argparse
import hashlib
import shutil
import sys
from pathlib import Path
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, validate_snapshot, utc, log

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--freeze', type=Path, required=True)
    parser.add_argument('--expected-current-run-id', required=True)
    args = parser.parse_args()
    args.test_root = ROOT / '_MPTest'; args.players = 2; args.cleanup = False; args.step_timeout = 45
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'retain completed native UI placement evidence and last complete fixture; restart exact owned processes for new Move code'
    try:
        frozen = read_json(args.freeze)
        for name, digest in frozen['files'].items():
            if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest:
                raise AcceptanceError('candidate changed: ' + name)
        if hashlib.sha256((ROOT / 'build/bin/Release/MT2_Multiplayer.dll').read_bytes()).hexdigest() != frozen['dll_sha256']:
            raise AcceptanceError('candidate DLL changed')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != args.expected_current_run_id:
            raise AcceptanceError('owned run differs')
        runner.checked('live-state-retention-limit', {'cause':'old UI diagnostic Unknown blocks native Save despite confirmed Controller action', 'behavior':'retain baseline and actual post-action readback; no full post-action snapshot claimed'})
        for name in ('run.json', 'host/instance.json', 'client-1/instance.json', 'host/game/mt2mp.log', 'client-1/game/mt2mp.log', 'host/diagnostics/status.json', 'client-1/diagnostics/status.json'):
            source = runner.root / name
            target = runner.directory / 'retained' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        for evidence_id in ('e49e6110-ca74-4a2c-b6bb-38d2b39febaa','ecac525e-11c1-49d4-99a8-3e2728813212'):
            source = ROOT / 'artifacts/acceptance' / evidence_id / 'summary.json'
            shutil.copy2(source, runner.directory / (evidence_id + '.json'))
        snapshot_id = '41f6e0b5-bcc2-4ef4-b930-ddfe6b3dffe2'
        saved = validate_snapshot(read_json(runner.root / 'host/diagnostics' / ('snapshot-' + snapshot_id + '.json')), snapshot_id, runner.root / 'host')
        target = runner.directory / 'complete_fixture.vrz'
        shutil.copy2(saved['path'], target)
        if hashlib.sha256(target.read_bytes()).hexdigest() != saved['sha256']:
            raise AcceptanceError('retained fixture SHA differs')
        runner.checked('complete-fixture-retained', saved)
        runner.command('restart-owned-small-fixture', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/native_open_acceptance.py'), '--snapshot-id', snapshot_id, '--expected-input-bytes', '208949', '--expected-input-sha256', 'fedec7bec52be5c40a362e0552390e804bbe86defc949524c6f9c4a20a519c1a', '--expected-dll-sha256', frozen['dll_sha256'], '--players', '2'])
        runner.summary.update(passed=True, state='prepared-move-two', new_run_id=read_json(runner.root / 'run.json')['run_id'])
    except Exception as error:
        runner.summary.update(passed=False, state='failed-move-setup', error={'step':runner.summary['step'], 'cause':str(error), 'behavior':'remaining mutations skipped; no action replay'})
        log('error', 'Move setup: ' + str(error))
    runner.summary['finished_utc'] = utc(); runner.persist(); print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1

if __name__ == '__main__': raise SystemExit(main())

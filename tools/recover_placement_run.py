"""Retain a stalled owned fixture and restart it without replaying its action."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import sys
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, validate_snapshot, utc, log

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-current-run-id', required=True)
    parser.add_argument('--freeze', type=Path, required=True)
    args = parser.parse_args()
    args.test_root = ROOT / '_MPTest'; args.players = 2; args.cleanup = False; args.step_timeout = 45
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'retain known complete snapshot and failed action; restart only manifest-owned test processes'
    try:
        frozen = read_json(args.freeze)
        for name, digest in frozen['files'].items():
            if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest:
                raise AcceptanceError('candidate changed: ' + name)
        dll = ROOT / 'build/bin/Release/MT2_Multiplayer.dll'
        digest = hashlib.sha256(dll.read_bytes()).hexdigest()
        if digest != frozen['dll_sha256']:
            raise AcceptanceError('candidate DLL changed')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != args.expected_current_run_id:
            raise AcceptanceError('owned run differs; no stop attempted')
        snapshot_id = '33826a80-cd62-47ab-a70c-1246bbdf6c40'
        saved = read_json(runner.root / 'host/diagnostics' / ('snapshot-' + snapshot_id + '.json'))
        saved = validate_snapshot(saved, snapshot_id, runner.root / 'host')
        if saved['sha256'] != 'afda34f7fca7817ec71598f174a124c748b5f1c66df3b690fbc6fe707ef9ae3d' or saved['bytes'] != 213244:
            raise AcceptanceError('confirmed baseline differs; no stop attempted')
        target = runner.directory / 'confirmed_baseline.vrz'
        shutil.copy2(saved['path'], target)
        if hashlib.sha256(target.read_bytes()).hexdigest() != saved['sha256']:
            raise AcceptanceError('retained snapshot differs')
        runner.checked('confirmed-baseline-retained', {'path': str(target), 'snapshot': saved})
        runner.command('retain-failed-native-attempt', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/retain_placement_failure.py')])
        runner.command('restart-and-load-owned-fixture', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/native_open_acceptance.py'),
            '--snapshot-id', '41f6e0b5-bcc2-4ef4-b930-ddfe6b3dffe2', '--expected-input-bytes', '208949',
            '--expected-input-sha256', 'fedec7bec52be5c40a362e0552390e804bbe86defc949524c6f9c4a20a519c1a',
            '--expected-dll-sha256', digest, '--players', '2'])
        runner.summary.update(passed=True, state='recovered-owned-placement-fixture', new_run_id=read_json(runner.root / 'run.json')['run_id'])
    except Exception as error:
        runner.summary.update(passed=False, state='failed-placement-recovery', error={'step': runner.summary['step'], 'cause': str(error), 'behavior': 'remaining mutations skipped; no replay'})
        log('error', 'placement recovery: ' + str(error))
    runner.summary['finished_utc'] = utc(); runner.persist()
    print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1

if __name__ == '__main__':
    raise SystemExit(main())

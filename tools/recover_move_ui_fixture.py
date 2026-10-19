"""Retain a failed UI observation and reload the complete authoritative Host file."""
import argparse
import hashlib
import shutil
import sys
from pathlib import Path
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, validate_snapshot, utc, log

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--freeze', type=Path, required=True)
    parser.add_argument('--retained-host', type=Path, required=True)
    parser.add_argument('--capture-title-frames', action='store_true')
    args = parser.parse_args()
    args.test_root = ROOT / '_MPTest'; args.players = 2; args.cleanup = False; args.step_timeout = 45
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'retain failed Client UI diagnostics and logs; reload complete Host file into two owned runtimes; no Client save or action replay'
    try:
        frozen = read_json(args.freeze)
        for name, digest in frozen['files'].items():
            if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest:
                raise AcceptanceError('candidate source differs: ' + name)
        if hashlib.sha256((ROOT / 'build/bin/Release/MT2_Multiplayer.dll').read_bytes()).hexdigest() != frozen['dll_sha256']:
            raise AcceptanceError('built DLL differs')
        retained = read_json(args.retained_host)
        if retained.get('passed') is not True or retained.get('parameters', {}).get('peer') != 'host':
            raise AcceptanceError('complete Host-only retention proof required')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != retained['runtime_run_id']:
            raise AcceptanceError('owned run differs from retained Host file')
        saved = next(check['result'] for check in retained['checks'] if check['check'] == 'native-placed-world-file')
        saved = validate_snapshot(saved, saved['snapshot_id'], runner.root / 'host')
        if saved.get('state') != 'snapshot_file_complete' or saved.get('distribution_requested') is not False:
            raise AcceptanceError('Host file is not complete local-only retention')
        destination = runner.directory / 'authoritative_host_world.vrz'
        shutil.copy2(saved['path'], destination)
        if hashlib.sha256(destination.read_bytes()).hexdigest() != saved['sha256']:
            raise AcceptanceError('retained Host copy checksum differs')
        runner.checked('authoritative-host-file-retained', {'snapshot': saved, 'copy': str(destination)})
        for peer in ('host', 'client-1'):
            state = runner.inspect('retained-current-state-' + peer, peer)
            runner.checked('current-state-' + peer, state)
            manifest = read_json(runner.manifests[peer])
            for index, name in enumerate(manifest['game_log_paths']):
                source = Path(name)
                if source.exists():
                    shutil.copy2(source, runner.directory / (peer + '-' + str(index) + '-' + source.name))
                else:
                    runner.checked('log-absent-' + peer + '-' + str(index), {'path': name, 'state': 'not-created'})
        runner.checked('client-save-deliberately-skipped', {'reason': 'retained unresolved Move UI observation; Host file is authoritative', 'claimed_client_save': False})
        command = [sys.executable, '-X', 'utf8', str(ROOT / 'tools/native_open_acceptance.py'),
            '--snapshot-id', saved['snapshot_id'], '--expected-input-bytes', str(saved['bytes']),
            '--expected-input-sha256', saved['sha256'], '--expected-dll-sha256', frozen['dll_sha256'], '--players', '2']
        if args.capture_title_frames:
            command.append('--capture-title-frames')
        runner.command('reload-authoritative-host-file', command)
        runner.summary.update(passed=True, state='prepared-normal-Move-two', new_run_id=read_json(runner.root / 'run.json')['run_id'])
    except Exception as cause:
        runner.summary.update(passed=False, state='failed-normal-Move-recovery', error={'step': runner.summary['step'], 'cause': str(cause), 'behavior': 'remaining operations skipped; no action replay'})
        log('error', 'Move UI recovery: ' + str(cause))
    runner.summary['finished_utc'] = utc(); runner.persist(); print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1

if __name__ == '__main__':
    raise SystemExit(main())

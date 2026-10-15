"""Prove the original Starting Point completion and Commit in an isolated Host."""
import argparse
from pathlib import Path
import time
import uuid

from local_acceptance import AcceptanceError, AcceptanceRunner, ROOT, hash_file, read_json, utc
from lobby_ui.native_local_acceptance import button, observe, left

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--expected-run-id', required=True)
parser.add_argument('--test-root', type=Path, required=True)
parser.add_argument('--phase', choices=('prepare', 'prove', 'retire'), required=True)
parser.add_argument('--snapshot', default='71b654a4-e005-4ad6-af5c-9c2974f9d41f')
args = parser.parse_args()
args.players, args.cleanup = 2, False
args.step_timeout, args.ready_timeout, args.wait_seconds = 45, 60, 60
runner = AcceptanceRunner(args)
runner.summary['scope'] = 'isolated Host original Starting Point completion and ordinary Commit; no peer replication claim'

try:
    runner.load_manifests()
    if runner.summary['runtime_run_id'] != args.expected_run_id:
        raise AcceptanceError('Owned run differs; no mutation attempted')
    digest = hash_file(ROOT / 'build/bin/Release/MT2_Multiplayer.dll')
    for peer, manifest in runner.manifests.items():
        if read_json(manifest)['installed_binary_sha256']['MT2_Multiplayer.dll'] != digest:
            raise AcceptanceError('Candidate DLL differs on ' + peer)
    runner.summary['dll_sha256'] = digest
    if args.phase == 'prepare':
        for peer in ('host', 'client-1'):
            runner.poll_title(peer)
            state = runner.single('idle-' + peer, peer, 'game_read_native_lobby')
            if state.get('admitted') is not False or state['controller'].get('initialized') is not False:
                raise AcceptanceError('Fixture requires an unadmitted, uninitialized peer: ' + peer)
        runner.checked('load-once', runner.single('load-once', 'host', 'game_load_snapshot', {'snapshot_id': args.snapshot}))
        deadline = time.monotonic() + 60
        while True:
            state = runner.inspect('loaded-host', 'host')
            world = state.get('native_world', {})
            if world.get('active_mode') == 3 and world.get('mode_state') == 4 and world.get('map_task_pending') is False:
                break
            if time.monotonic() >= deadline:
                raise AcceptanceError('Host load did not reach idle mode3/state4 within60s; no replay')
        runner.checked('loaded-development', runner.single('loaded-development', 'host', 'game_read_tutorial_development'))
    elif args.phase == 'prove':
        baseline = runner.single('baseline', 'host', 'game_read_tutorial_development')
        runner.checked('baseline', baseline)
        release = baseline['startingpoint_release']
        if release['feature_count'] != 0 or release['feature_present'] is not False:
            raise AcceptanceError('Starting Point Release feature must be absent before Commit')
        current = baseline
        for operation in ('show', 'complete', 'commit'):
            request = {'token': str(uuid.uuid4()), 'native_origin_uuid': current['snapshot_uuid'],
                       'world_epoch': current['world_epoch'], 'world_generation': current['world_generation'],
                       'operation': operation, 'expected_development': current}
            result = runner.single(operation + '-once', 'host', 'game_tutorial_startingpoint', request)
            for field in ('attempted', 'returned', 'confirmed'):
                if result.get(field) is not True:
                    raise AcceptanceError(operation + ' lacks native ' + field + '; no replay')
            if result.get('unknown') is not False:
                raise AcceptanceError(operation + ' effect is unknown; no replay')
            runner.checked(operation, result)
            current = runner.single('after-' + operation, 'host', 'game_read_tutorial_development')
            runner.checked('after-' + operation, current)
            runner.checked('frame-' + operation, runner.single('frame-' + operation, 'host', 'game_capture_frame'))
        actual = current['startingpoint_release']
        if actual['feature_count'] != 1 or actual['feature_present'] is not True:
            raise AcceptanceError('Original Commit did not insert exactly one native Starting Point Release feature')
        if actual['mutation_counter'] != (release['mutation_counter'] + 1) % (1 << 32):
            raise AcceptanceError('Original Commit Release mutation counter delta differs from1')
        runner.summary.update(before=baseline, after=current, native_host_commit_proven=True)
    else:
        previous = {}
        for peer in ('client-1', 'host'):
            state = runner.single('before-leave-' + peer, peer, 'game_read_native_lobby')
            previous[peer] = state['prepare']['leave_confirmed']
            button(runner, peer, 'leave')
        observe(runner, 'retired', lambda peer, value: left(value, previous[peer]))
    runner.summary.update(passed=True, state='startingpoint-' + args.phase + '-confirmed')
except Exception as error:
    runner.summary.update(passed=False, state='failed-no-replay', error={'step': runner.summary['step'], 'cause': str(error)})
runner.summary['finished_utc'] = utc()
runner.persist()
print(runner.summary_path)
raise SystemExit(0 if runner.summary['passed'] else 1)

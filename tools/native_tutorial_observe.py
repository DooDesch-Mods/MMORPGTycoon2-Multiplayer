"""Read tutorial cursors in two owned peers, optionally preparing one new world."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from local_acceptance import AcceptanceError, AcceptanceRunner, ROOT, hash_file, read_json, utc
from lobby_ui.native_local_acceptance import button, observe, ready

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--expected-run-id', required=True)
parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
parser.add_argument('--start', action='store_true')
parser.add_argument('--world', help='Select an existing owned fixture instead of creating a world')
parser.add_argument('--fixture-snapshot', help='Load one verified diagnostic snapshot after admission')
parser.add_argument('--development', action='store_true', help='Read the joined Starting Point project, record and inventory baseline')
args = parser.parse_args()
args.players, args.cleanup = 2, False
args.step_timeout, args.ready_timeout, args.wait_seconds = 45, 60, 60
runner = AcceptanceRunner(args)
runner.summary['scope'] = ('read-only Starting Point development and inventory baseline on two owned peers'
                           if args.development else 'read-only native tutorial observation on two owned peers; no shared tutorial progression claim')
try:
    runner.load_manifests()
    if runner.summary['runtime_run_id'] != args.expected_run_id:
        raise AcceptanceError('Owned run differs; no game operation attempted')
    expected = hash_file(ROOT / 'build/bin/Release/MT2_Multiplayer.dll')
    for peer, manifest in runner.manifests.items():
        if read_json(manifest)['installed_binary_sha256']['MT2_Multiplayer.dll'] != expected:
            raise AcceptanceError('Observer DLL differs on ' + peer)
    runner.summary['dll_sha256'] = expected
    if args.start:
        for peer in ('host', 'client-1'):
            runner.poll_title(peer)
            initial = runner.single('idle-' + peer, peer, 'game_read_native_lobby')
            if initial.get('admitted') is not False or initial.get('controller', {}).get('initialized') is not False:
                raise AcceptanceError('New-world preparation requires idle unadmitted Title on ' + peer)
        for peer in ('host', 'client-1'):
            button(runner, peer, 'open')
        button(runner, 'host', 'host')
        button(runner, 'client-1', 'join')
        if args.fixture_snapshot:
            runner.checked('loaded-fixture', runner.single('load-fixture-once', 'host', 'game_load_snapshot',
                           {'snapshot_id': args.fixture_snapshot}))
        elif args.world:
            button(runner, 'host', 'pick', world=args.world)
        else:
            world = runner.single('create-tutorial-fixture', 'host', 'game_create_test_world')
            if world.get('state') != 'world_active' or not world.get('name', '').startswith('MPTest'):
                raise AcceptanceError('New small world acknowledgement absent')
            runner.checked('created-small-world', world)
        if args.world and not args.fixture_snapshot:
            button(runner, 'host', 'start')
        observe(runner, 'playing', lambda peer, state: ready(state))
    elif args.fixture_snapshot:
        for peer in ('host', 'client-1'):
            state = runner.single('admitted-title-' + peer, peer, 'game_read_native_lobby')
            if state.get('admitted') is not True or state.get('title_active') is not True:
                raise AcceptanceError('Fixture continuation requires admitted Title on ' + peer)
        runner.checked('loaded-fixture', runner.single('load-fixture-once', 'host', 'game_load_snapshot',
                       {'snapshot_id': args.fixture_snapshot}))
        observe(runner, 'playing', lambda peer, state: ready(state))
    values = {}
    for peer in ('host', 'client-1'):
        value = runner.single('tutorial-' + peer, peer, 'game_read_tutorial')
        if value.get('schema') != 1 or value.get('mainthread_confirmed') is not True or value.get('lifetime_confirmed') is not True:
            raise AcceptanceError('Native tutorial observation incomplete on ' + peer)
        runner.checked('tutorial-' + peer, value)
        values[peer] = value
        runner.checked('tutorial-frame-' + peer, runner.single('frame-' + peer, peer, 'game_capture_frame'))
    if args.development:
        development = {}
        for peer in ('host', 'client-1'):
            value = runner.single('development-' + peer, peer, 'game_read_tutorial_development')
            if value.get('schema') != 1 or value.get('mainthread_confirmed') is not True or value.get('lifetime_confirmed') is not True:
                raise AcceptanceError('Native development observation incomplete on ' + peer)
            for field in ('snapshot_uuid', 'world_epoch', 'world_generation'):
                if value.get(field) != values[peer][field]:
                    raise AcceptanceError('Development and tutorial local origin differs on ' + peer)
            runner.checked('development-' + peer, value)
            development[peer] = value
        runner.summary['development'] = development
        for field in ('project', 'startingpoint'):
            if development['host'][field] != development['client-1'][field]:
                raise AcceptanceError('Joined development baseline differs for ' + field)
    contexts = {peer: runner.single('session-context-' + peer, peer, 'game_read_native_lobby')
                for peer in ('host', 'client-1')}
    if not all(ready(context) for context in contexts.values()):
        raise AcceptanceError('Tutorial observations require both peers still Playing')
    shared = contexts['host']['prepare']['snapshot_id']
    if not shared or shared != contexts['client-1']['prepare']['snapshot_id']:
        raise AcceptanceError('Session snapshot differs between peers')
    runner.checked('shared-session-context', {'snapshot_uuid': shared, 'local_origins':
                   {peer: value['snapshot_uuid'] for peer, value in values.items()}})
    runner.summary.update(passed=True, state='native-tutorial-development-observed' if args.development else 'native-tutorial-observed', tutorials=values)
except Exception as error:
    runner.summary.update(passed=False, state='failed-no-replay', error={'step': runner.summary['step'], 'cause': str(error)})
runner.summary['finished_utc'] = utc()
runner.persist()
print(runner.summary_path)
raise SystemExit(0 if runner.summary['passed'] else 1)

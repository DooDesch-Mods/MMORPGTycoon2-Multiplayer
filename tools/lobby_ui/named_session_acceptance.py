"""Prove named-world automatic start and Leave/rejoin on an exact owned pair."""
import argparse
import json
from pathlib import Path
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from local_acceptance import AcceptanceError, AcceptanceRunner, ROOT, compare_ledgers, hash_file, read_json, utc
from local_test import safe_path
from lobby_ui.native_local_acceptance import button, observe, ready, left
from time_mouse_acceptance import main as check_hud_buttons

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--expected-run-id', required=True)
parser.add_argument('--test-root', type=Path, required=True)
args = parser.parse_args()
args.players, args.cleanup = 2, False
args.step_timeout, args.ready_timeout, args.wait_seconds = 45, 60, 60
runner = AcceptanceRunner(args)
runner.summary['scope'] = 'named snapshot folders, automatic two-peer preparation, native live state, Leave and same-process rejoin'
try:
    runner.load_manifests()
    if runner.summary['runtime_run_id'] != args.expected_run_id:
        raise AcceptanceError('Owned run differs; no mutation attempted')
    binary = hash_file(ROOT / 'build/bin/Release/MT2_Multiplayer.dll')
    for peer, manifest in runner.manifests.items():
        if read_json(manifest)['installed_binary_sha256']['MT2_Multiplayer.dll'] != binary:
            raise AcceptanceError('Candidate DLL differs on ' + peer)
        runner.poll_title(peer)
    runner.summary['dll_sha256'] = binary
    fixture = ROOT / 'tools/original_saves/fixture/roaming/VectorStorm/MMORPG Tycoon 2/76561198013473990/save/World of Stories/autosave.vrz'
    digest = 'e06531ef6fc0036a2cb4362bfe17140d69c47b63f13af249d252e84d6ef48334'
    if hash_file(fixture) != digest:
        raise AcceptanceError('Retained small fixture hash differs')
    snapshots = []
    for cycle, (world, route) in enumerate((('Worlds of Titans', 'snapshot'), ('Worlds of Titans', 'picker'))):
        for peer in ('host', 'client-1'):
            button(runner, peer, 'open')
        button(runner, 'host', 'host')
        button(runner, 'client-1', 'join')
        if route == 'snapshot':
            host = read_json(runner.manifests['host'])
            runtime = Path(host['runtime_root'])
            snapshot = str(uuid.uuid4())
            target = safe_path(runtime / 'data/76561198013473990/save' / world / ('snapshot-' + snapshot + '.vrz'), runtime)
            manifest = safe_path(runtime / 'diagnostics' / ('snapshot-' + snapshot + '.json'), runtime)
            if target.exists() or manifest.exists():
                raise AcceptanceError('New fixture identity already exists; no overwrite')
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(fixture.read_bytes())
            manifest.write_text(json.dumps({'snapshot_id': snapshot, 'world_name': world, 'path': str(target),
                                'bytes': target.stat().st_size, 'sha256': digest}, indent=2) + '\n', encoding='utf-8')
            runner.checked('fixture-' + str(cycle), {'world': world, 'snapshot_id': snapshot, 'sha256': hash_file(target)})
            runner.checked('load-fixture-once', runner.single('load-' + str(cycle), 'host', 'game_load_snapshot', {'snapshot_id': snapshot}))
        else:
            button(runner, 'host', 'pick', world=world)
            selected = runner.single('selected-world-before-start', 'host', 'game_read_native_lobby')
            if selected.get('selected_world') != world or not selected.get('selected_file'):
                raise AcceptanceError('Native save picker did not confirm the retained world')
            runner.checked('single-start-button-frame', runner.single('single-start-button-frame', 'host', 'game_capture_frame'))
            button(runner, 'host', 'start')
        # Opening an admitted Host world invokes the same start flow as the visible primary button.
        values = observe(runner, 'playing-' + str(cycle), lambda peer, value: ready(value))
        shared = values['host']['prepare']['snapshot_id']
        if not shared or values['client-1']['prepare']['snapshot_id'] != shared or shared in snapshots:
            raise AcceptanceError('One new shared session snapshot was not acknowledged')
        snapshots.append(shared)
        for peer in ('host', 'client-1'):
            peer_runtime = Path(read_json(runner.manifests[peer])['runtime_root'])
            received = read_json(peer_runtime / 'diagnostics' / ('snapshot-' + shared + '.json'))
            if received.get('world_name') != world:
                raise AcceptanceError('Actual snapshot folder differs on ' + peer)
        engines = {peer: runner.inspect('native-values-' + str(cycle) + '-' + peer, peer) for peer in ('host', 'client-1')}
        runner.checked('equal-native-ledger-' + str(cycle), compare_ledgers(engines['host'], engines['client-1']))
        for peer in ('host', 'client-1'):
            runner.checked('frame-' + str(cycle) + '-' + peer, runner.single('frame-' + str(cycle) + '-' + peer, peer, 'game_capture_frame'))
        if cycle == 1:
            result = check_hud_buttons(['--test-root', str(args.test_root), '--expected-run-id', args.expected_run_id,
                                       '--expected-dll-sha256', binary])
            if result != 0:
                raise AcceptanceError('Native HUD acceptance failed; no Leave or button replay')
            runner.checked('native-hud-buttons', {'passed': True, 'runtime_run_id': args.expected_run_id})
        previous = {peer: value['prepare']['leave_confirmed'] for peer, value in values.items()}
        for peer in ('client-1', 'host'):
            button(runner, peer, 'leave')
        observe(runner, 'left-' + str(cycle), lambda peer, value: left(value, previous[peer]))
    runner.summary.update(passed=True, state='named-world-start-and-rejoin-confirmed', snapshots=snapshots)
except Exception as error:
    runner.summary.update(passed=False, state='failed-no-replay', error={'step': runner.summary['step'], 'cause': str(error)})
runner.summary['finished_utc'] = utc()
runner.persist()
print(runner.summary_path)
raise SystemExit(0 if runner.summary['passed'] else 1)

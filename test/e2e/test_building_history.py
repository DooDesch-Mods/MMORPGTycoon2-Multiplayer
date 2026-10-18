"""Offline checks for the one-placement Undo/Redo E2E receipt logic."""
from __future__ import annotations

from pathlib import Path
import copy
import json
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.e2e import building_history as history
from runtime import RuntimeErrorDetail


UID = "11111111-1111-4111-8111-111111111111"
MUTATION = "22222222-2222-4222-8222-222222222222"


def ledger():
    return {"clock_day": 2, "cash": 9000, "record_present": True, "record_day": 2,
            "record_cash": 9000, "record_income": 300, "record_expense": 1000,
            "line_counts": [1 if index == 6 else 0 for index in range(20)],
            "line_income": [-1000 if index == 6 else 0 for index in range(20)]}


class FakeRunner:
    def __init__(self):
        self.calls = []

    def checked(self, *_):
        pass

    def single(self, step, peer, tool, args):
        self.calls.append((step, peer, tool, args))
        return {"state": "queued", "action_uuid": MUTATION, "target_action_uuid": UID,
                "direction": "undo", "ui_hook_calls": 1, "ui_intents_queued": 1}


class BuildingHistoryTests(unittest.TestCase):
    def test_shared_ready_requires_matching_adopted_frames(self):
        frame = dict(zip(history.FRAME_ID, (1, UID, 2, 5, 3, 4)))
        engines = {peer: {'typed_actions': {
            'initialized': True, 'started': True, 'pending': False, 'locked': False, 'failed': 0,
            'live_state': {'active': True, 'locked': False, 'confirmed': True, 'cause': ''},
            'network_frame_revision': 5, 'native_economy': {'day': {'frame': dict(frame)}}}}
            for peer in history.PEERS}
        with patch.object(history, 'ready_reason', return_value=''):
            self.assertEqual(history.shared_ready(engines), {peer: frame for peer in history.PEERS})
            engines['client-1']['typed_actions']['native_economy']['day']['frame']['revision'] = 4
            with self.assertRaisesRegex(history.AcceptanceError, 'adopted gameplay Frames differ'):
                history.shared_ready(engines)

    def test_shared_ready_rejects_unconfirmed_client_authority(self):
        engines = {peer: {'typed_actions': {
            'initialized': True, 'started': True, 'pending': False, 'locked': False, 'failed': 0,
            'live_state': {'active': True, 'locked': False, 'confirmed': False, 'cause': ''},
            'network_frame_revision': 5, 'native_economy': {'day': {'frame': {
                **dict(zip(history.FRAME_ID, (1, UID, 2, 5, 3, 4)))}}}}}
            for peer in history.PEERS}
        with patch.object(history, 'ready_reason', return_value=''):
            with self.assertRaisesRegex(history.AcceptanceError, 'client-1 Controller/LiveBridge unhealthy'):
                history.shared_ready(engines)

    def test_resume_never_accepts_unknown_native_failure(self):
        runner = FakeRunner()
        runner.summary = {"runtime_run_id": UID}
        summary = {"runtime_run_id": UID, "state": "failed-native-building-undo-redo-two",
                   "error": {"cause": "Client Controller failed/Unknown"}}
        with patch.object(history, "read_json", return_value=summary):
            with self.assertRaisesRegex(history.AcceptanceError, "same runtime/placement ledger-oracle failure"):
                history.resume_initial(runner, history.ROOT / 'artifacts/acceptance/offline/summary.json',
                                       Path('placement.json'))
        self.assertEqual(runner.calls, [])

    def test_unknown_stops_before_binding_read_and_preserves_cash_cause(self):
        observed = {peer: {"engine": {"typed_actions": {
            "failed": 1, "locked": True, "cause": "AddCash edit scope differs"}}}
                    for peer in history.PEERS}
        runner = FakeRunner()
        moments = iter([0, 1])
        with patch.object(history, "pair", return_value=observed) as read:
            with self.assertRaisesRegex(history.AcceptanceError, "AddCash edit scope differs"):
                history.wait_complete(runner, {}, MUTATION, UID, "undo", ledger(),
                                      7, 1, "client-1", 10, now=lambda: next(moments))
        self.assertFalse(read.call_args.kwargs["include_bindings"])
        self.assertEqual(runner.calls, [])

    def test_history_uses_shared_snapshot_with_distinct_local_load_origins(self):
        binding = {"record_key": "action:" + UID, "local_uid": 101,
                   "type": "SmallInn", "variant": "base", "x": 8020, "y": 70, "z": 8550,
                   "rotation": [0, 0, 0, 1], "scale": [1, 1, 1],
                   "revoked": False, "unknown": False}
        snapshots = {peer: {
            "engine": {"native_open_origin": {"uuid": MUTATION if peer == "host" else UID},
                       "capabilities": {"native_building_history_ui": True},
                       "typed_actions": {"action_uuid": UID, "outcome": 1, "pending": False,
                                         "locked": False, "failed": 0, "host_revision": 7,
                                         "confirmed": 1}},
            "ui": {"snapshot_uuid": UID, "undo_target_action_uuid": UID, "active_graph": False},
            "bindings": {"bindings": [dict(binding)]}} for peer in history.PEERS}
        proof = {"ledger": ledger(), "objects": {peer: dict(binding) for peer in history.PEERS}}
        frames = {peer: {"snapshot_uuid": UID} for peer in history.PEERS}
        with patch.object(history, "shared_ready", return_value=frames), \
                patch.object(history, "compare_ledgers", return_value=ledger()):
            self.assertEqual(history.require_initial(snapshots, UID, proof), (ledger(), 7, 1))
            snapshots["host"]["ui"]["snapshot_uuid"] = MUTATION
            with self.assertRaisesRegex(history.AcceptanceError, "history UI/shared world snapshot differs"):
                history.require_initial(snapshots, UID, proof)

    def test_undo_and_redo_preserve_native_compensating_cash_entries(self):
        before = ledger()
        after = history.undo_ledger(before, 1000)
        self.assertEqual(after["cash"], 10000)
        self.assertEqual(after["record_cash"], 10000)
        self.assertEqual(after["record_expense"], 1000)
        self.assertEqual(after["record_income"], 1300)
        self.assertEqual(after["line_counts"], [2 if index == 6 else 0 for index in range(20)])
        self.assertEqual(after["line_income"], [0] * 20)
        restored = history.redo_ledger(after, 1000)
        self.assertEqual(restored["cash"], before["cash"])
        self.assertEqual(restored["record_expense"], before["record_expense"])
        self.assertEqual(restored["record_income"], before["record_income"])
        self.assertEqual(restored["line_income"], before["line_income"])
        self.assertEqual(restored["line_counts"][6], before["line_counts"][6] + 2)
        self.assertEqual(before, ledger())

    def test_confirmed_undo_requires_ack_stack_tombstone_and_same_uid(self):
        binding = {"record_key": "action:" + UID, "local_uid": 101, "revoked": False,
                   "unknown": False, "type": "Respawn", "variant": "base",
                   "x": 8140, "y": 2, "z": 8550, "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]}
        before = {}
        after = {}
        for peer in history.PEERS:
            before[peer] = {"binding": dict(binding),
                            "engine": {"typed_actions": {"attempted": 1}},
                            "ui": {"snapshot_uuid": UID, "undo_depth": 2, "redo_depth": 0,
                                   "ui_hook_calls": 0, "ui_intents_queued": 0}}
            ui = {"snapshot_uuid": UID, "active_graph": False, "undo_depth": 1,
                  "redo_depth": 1, "redo_target_action_uuid": UID,
                  "ui_hook_calls": 1 if peer == "client-1" else 0,
                  "ui_intents_queued": 1 if peer == "client-1" else 0}
            if peer == "client-1":
                ui.update(last_action_uuid=MUTATION, last_target_action_uuid=UID,
                          last_direction="undo")
            after[peer] = {"ui": ui, "bindings": {"bindings": [{**binding, "revoked": True}]},
                           "engine": {"typed_actions": {
                               "action_uuid": MUTATION, "outcome": 1, "host_revision": 8,
                               "confirmed": 2, "failed": 0, "attempted": 2,
                               "pending": False, "locked": False,
                               "ack_mask": 3 if peer == "host" else 0}}}
        expected = history.undo_ledger(ledger(), 1000)
        with patch.object(history, "shared_ready"), patch.object(history, "compare_ledgers", return_value=expected):
            self.assertEqual(history.phase_complete(after, before, MUTATION, UID, "undo", expected,
                                                    7, 1, "client-1"), expected)
            broken = copy.deepcopy(after)
            broken["host"]["bindings"]["bindings"][0]["local_uid"] = 999
            with self.assertRaisesRegex(history.AcceptanceError, "UID/full pose differs"):
                history.phase_complete(broken, before, MUTATION, UID, "undo", expected,
                                       7, 1, "client-1")
            broken = copy.deepcopy(after)
            broken["host"]["engine"]["typed_actions"]["ack_mask"] = 1
            with self.assertRaisesRegex(history.AcceptanceError, "lacks both native ACKs"):
                history.phase_complete(broken, before, MUTATION, UID, "undo", expected,
                                       7, 1, "client-1")

    def test_timeout_after_queued_native_entry_never_replays_mutation(self):
        binding = {"record_key": "action:" + UID, "local_uid": 101, "revoked": False}
        before = {peer: {"bindings": {"bindings": [dict(binding)]},
                         "ui": {"snapshot_uuid": UID, "ui_hook_calls": 0, "ui_intents_queued": 0}}
                  for peer in history.PEERS}
        observed = {peer: {"engine": {"typed_actions": {
            "action_uuid": UID, "confirmed": 1, "failed": 0, "locked": False, "pending": False}}}
                    for peer in history.PEERS}
        runner = FakeRunner()
        moments = iter([0, 1, 5])
        with patch.object(history, "pair", return_value=observed):
            with self.assertRaisesRegex(history.AcceptanceError, "timeout.*no replay"):
                history.mutate_once(runner, before, UID, "undo", "client-1", ledger(),
                                    7, 1, 4, now=lambda: next(moments))
        self.assertEqual(len(runner.calls), 1)
        self.assertEqual(runner.calls[0][2], "game_native_building_history_ui")


class FrameworkAdmissionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.candidate = self.root / 'MT2MultiplayerPlugin.dll'
        self.candidate.write_bytes(b'exact selected plugin fixture')
        self.digest = history.hash_file(self.candidate)
        self.freeze = self.root / 'freeze.json'
        self.freeze.write_text(json.dumps({'dll_sha256': self.digest}), encoding='utf-8')
        self.placement = self.root / 'placement.json'
        self.placement.write_text('{}', encoding='utf-8')
        self.manifests = {}
        for number, peer in enumerate(history.PEERS, 1):
            runtime = self.root / peer
            runtime.mkdir()
            manifest = runtime / 'instance.json'
            manifest.write_text(json.dumps({'schema_version': 2, 'runtime_kind': 'framework-plugin',
                'runtime_root': str(runtime), 'pid': number, 'process_start_time': str(100 + number),
                'installed_binary_sha256': {'MT2MultiplayerPlugin.dll': self.digest}}), encoding='utf-8')
            self.manifests[peer] = manifest
        class Runner:
            def __init__(self, root, manifests):
                self.root, self.manifests = root, manifests
                self.summary = {'runtime_run_id': UID}
            def load_manifests(self):
                pass
            def checked(self, *_):
                pass
        class Binding:
            def __init__(self, _):
                pass
            def bind(self, _):
                return {'process_identity_verified': True}
        self.runner, self.binding = Runner(self.root, self.manifests), Binding
        self.patch = patch.object(history, 'placement_evidence', return_value=({'runtime_run_id': UID}, MUTATION, {'objects': {}}))
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def test_framework_selected_artifact_succeeds_without_legacy_dll(self):
        action, _ = history.preflight(self.runner, self.placement, self.freeze, self.binding,
                                      candidate_binary=self.candidate)
        self.assertEqual(action, MUTATION)
        self.assertFalse((self.root / 'host/game/MT2_Multiplayer.dll').exists())

    def test_framework_requires_exact_artifact_and_frozen_content(self):
        for selected, content, freeze_hash, error in [
                (None, b'exact selected plugin fixture', self.digest, 'Explicit candidate binary'),
                (self.candidate, b'wrong build', self.digest, 'differs from installed'),
                (self.root / 'MT2_Multiplayer.dll', b'exact selected plugin fixture', self.digest, 'exact candidate filename'),
                (self.candidate, b'exact selected plugin fixture', '0' * 64, 'differs from candidate freeze')]:
            self.candidate.write_bytes(content)
            self.freeze.write_text(json.dumps({'dll_sha256': freeze_hash}), encoding='utf-8')
            with self.subTest(selected=selected, freeze=freeze_hash), self.assertRaisesRegex(
                    (history.AcceptanceError, RuntimeErrorDetail), error):
                history.preflight(self.runner, self.placement, self.freeze, self.binding, candidate_binary=selected)

    def test_legacy_default_keeps_real_installed_file_hash_check(self):
        for path in self.manifests.values():
            identity = json.loads(path.read_text(encoding='utf-8'))
            identity.update(schema_version=1, installed_binary_sha256={'MT2_Multiplayer.dll': self.digest})
            del identity['runtime_kind']
            path.write_text(json.dumps(identity), encoding='utf-8')
            installed = Path(identity['runtime_root']) / 'game/MT2_Multiplayer.dll'
            installed.parent.mkdir()
            installed.write_bytes(self.candidate.read_bytes())
        self.assertEqual(history.preflight(self.runner, self.placement, self.freeze, self.binding)[0], MUTATION)
        (self.root / 'client-1/game/MT2_Multiplayer.dll').write_bytes(b'changed installed file')
        with self.assertRaisesRegex(history.AcceptanceError, 'exact installed DLL differs'):
            history.preflight(self.runner, self.placement, self.freeze, self.binding)

    def test_cli_forwards_explicit_candidate_without_inferring_framework_build(self):
        with patch.object(history, 'execute', return_value=0) as execute:
            self.assertEqual(history.main(['--placement', 'placement.json', '--freeze', 'freeze.json',
                '--test-root', 'owned/runs/test', '--candidate-binary', str(self.candidate)]), 0)
            selected = execute.call_args.args[0]
            self.assertEqual(selected.candidate_binary, self.candidate)
            self.assertEqual(selected.test_root, Path('owned/runs/test'))


if __name__ == "__main__":
    unittest.main()

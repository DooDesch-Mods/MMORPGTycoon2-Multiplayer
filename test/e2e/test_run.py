"""Offline guards for the selected native E2E command; no game process is touched."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.e2e import run

def engine():
    frame = {key: 1 for key in run.FRAME_ID}
    frame["snapshot_uuid"] = "one-snapshot"
    return {"game_thread_ready": True, "session_connected": True, "in_game": True,
            "native_world": {"world_present": True, "mode_state": 4, "quitting": False,
                             "title_pending": False, "edits": 0, "saves": 0, "map_task_pending": False},
            "typed_actions": {"initialized": True, "started": True, "pending": False,
                              "locked": False, "failed": 0,
                              "live_state": {"active": True, "locked": False, "confirmed": True, "cause": ""},
                              "network_frame_revision": 1, "native_economy": {"day": {"frame": frame}}}}


def synthetic_receipt(case, peer):
    if case not in ("pass", "failed-prior") or peer not in run.PEERS:
        raise ValueError("unknown synthetic receipt case or peer")
    value = engine()
    controller = value["typed_actions"]
    controller["network_frame_revision"] = 6
    controller["native_economy"]["day"]["frame"] = {
        "session": 31, "snapshot_uuid": "11111111-2222-4333-8444-555555555555",
        "epoch": 2, "revision": 6, "base_action_revision": 4, "time_revision": 5,
    }
    if case == "failed-prior" and peer == "client-1":
        controller["failed"] = 1
    # The diagnostic call can succeed while the observed Controller has failed.
    return {"calls": [
        {"tool": "bind_instance", "state": "confirmed",
         "result": {"structuredContent": {"process_identity_verified": True}}},
        {"tool": "game_inspect_engine", "state": "confirmed",
         "result": {"isError": False, "structuredContent": {"ok": True, "result": value}}},
    ]}


def inspect_receipt(receipt, peer):
    # Replace only the transport; use the production inspect/single/result parser.
    runner = run.AcceptanceRunner.__new__(run.AcceptanceRunner)
    with patch.object(runner, "job", return_value=receipt) as job:
        value = runner.inspect("fixture-" + peer, peer)
        job.assert_called_once_with("fixture-" + peer, peer,
                                    [{"tool": "game_inspect_engine", "arguments": {}}])
        return value


def synthetic_engines(case):
    return {peer: inspect_receipt(synthetic_receipt(case, peer), peer) for peer in run.PEERS}


class SharedReadyTests(unittest.TestCase):
    def test_requires_unfailed_adopted_shared_frame(self):
        engines = {peer: engine() for peer in run.PEERS}
        self.assertEqual(run.shared_ready(engines)["client-1"]["revision"], 1)
        engines["client-1"]["typed_actions"]["failed"] = 1
        with self.assertRaisesRegex(run.AcceptanceError, "failed=1"):
            run.shared_ready(engines)
        engines["client-1"]["typed_actions"]["failed"] = 0
        engines["client-1"]["typed_actions"]["network_frame_revision"] = 0
        with self.assertRaisesRegex(run.AcceptanceError, "not adopted"):
            run.shared_ready(engines)

    def test_synthetic_pass_and_failed_prior(self):
        passed = synthetic_engines("pass")
        self.assertEqual(run.shared_ready(passed)["host"]["revision"], 6)
        failed = synthetic_engines("failed-prior")
        self.assertEqual(failed["client-1"]["typed_actions"]["failed"], 1)
        with self.assertRaisesRegex(run.AcceptanceError, "failed=1"):
            run.shared_ready(failed)

    def test_synthetic_pass_rejects_lost_bridge_frame_or_identity(self):
        passed = synthetic_engines("pass")
        lost_bridge = copy.deepcopy(passed)
        lost_bridge["client-1"]["typed_actions"]["live_state"]["confirmed"] = False
        with self.assertRaisesRegex(run.AcceptanceError, "live_confirmed=False"):
            run.shared_ready(lost_bridge)
        unadopted = copy.deepcopy(passed)
        unadopted["client-1"]["typed_actions"]["network_frame_revision"] = 0
        with self.assertRaisesRegex(run.AcceptanceError, "not adopted"):
            run.shared_ready(unadopted)
        changed_identity = copy.deepcopy(passed)
        changed_identity["client-1"]["typed_actions"]["native_economy"]["day"]["frame"]["snapshot_uuid"] = "other"
        with self.assertRaisesRegex(run.AcceptanceError, "Frames differ"):
            run.shared_ready(changed_identity)

    def test_receipt_parser_rejects_unconfirmed_or_ambiguous_engine(self):
        for fault, cause in (("unknown", "not confirmed"), ("is-error", "not confirmed"),
                             ("not-ok", "acknowledgement failed"),
                             ("duplicate", "exactly1.*actual=2"), ("missing", "exactly1.*actual=0")):
            with self.subTest(fault=fault):
                receipt = synthetic_receipt("pass", "client-1")
                call = receipt["calls"][1]
                if fault == "unknown":
                    call["state"] = "unknown"
                elif fault == "is-error":
                    call["result"]["isError"] = True
                elif fault == "not-ok":
                    call["result"]["structuredContent"]["ok"] = False
                elif fault == "duplicate":
                    receipt["calls"].append(copy.deepcopy(call))
                else:
                    receipt["calls"].pop()
                with self.assertRaisesRegex(run.AcceptanceError, cause):
                    inspect_receipt(receipt, "client-1")


class PreflightTests(unittest.TestCase):
    def test_exact_process_and_installed_dll_before_native_inspect(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temporary:
            root = Path(temporary) / "_MPTest"
            digest = hashlib.sha256(b"candidate").hexdigest()
            manifests = {}
            for peer in run.PEERS:
                peer_root = root / peer
                game = peer_root / "game"
                game.mkdir(parents=True)
                (game / "MT2_Multiplayer.dll").write_bytes(b"candidate")
                manifest = peer_root / "instance.json"
                manifest.write_text(json.dumps({"schema_version": 1, "runtime_root": str(peer_root), "pid": 123,
                                                "process_start_time": "456",
                                                "installed_binary_sha256": {"MT2_Multiplayer.dll": digest}}), encoding="utf-8")
                manifests[peer] = manifest
            freeze = Path(temporary) / "freeze.json"
            setup = Path(temporary) / "setup.json"
            freeze.write_text(json.dumps({"dll_sha256": digest}), encoding="utf-8")
            setup.write_text(json.dumps({"passed": True, "runtime_run_id": "run"}), encoding="utf-8")

            class Runner:
                def __init__(self):
                    self.root = root
                    self.summary = {}
                    self.manifests = manifests
                    self.inspected = []

                def load_manifests(self):
                    self.summary["runtime_run_id"] = "run"

                def checked(self, *args):
                    pass

                def inspect(self, label, peer):
                    self.inspected.append(peer)
                    return engine()

            class Identity:
                def __init__(self, root):
                    pass

                def bind(self, path):
                    return {"process_identity_verified": True}

            runner = Runner()
            run.preflight(runner, freeze, setup, Identity)
            self.assertEqual(runner.inspected, list(run.PEERS))
            (root / "client-1" / "game" / "MT2_Multiplayer.dll").write_bytes(b"changed")
            runner.inspected.clear()
            with self.assertRaisesRegex(run.AcceptanceError, "installed DLL differs"):
                run.preflight(runner, freeze, setup, Identity)
            with self.assertRaisesRegex(run.AcceptanceError, "installed DLL differs"):
                run.preflight(runner, freeze, setup, Identity, candidate_binary=root / "host/game/MT2_Multiplayer.dll")
            self.assertEqual(runner.inspected, [])


class DispatchTests(unittest.TestCase):
    def args(self):
        return argparse.Namespace(scenario="building-ui", setup=Path("fixture.json"), freeze=Path("freeze.json"),
                                  type="Respawn", variant="base", expected_cost=1000, x=8100, z=8550)

    def test_building_ui_requires_explicit_fresh_site(self):
        args = self.args()
        args.x = None
        with self.assertRaisesRegex(run.AcceptanceError, "explicit finite --x and --z"):
            run.command(args)

    def test_chat_uses_identity_preflight_without_world_or_subprocess(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temporary:
            class Runner:
                def __init__(self, args):
                    self.directory = Path(temporary)
                    self.summary_path = self.directory / "summary.json"
                    self.summary = {"step": "preflight", "runtime_run_id": "run"}

                def persist(self):
                    self.summary_path.write_text(json.dumps(self.summary), encoding="utf-8")

                def checkpoint(self, step):
                    self.summary["step"] = step
                    self.persist()

            args = self.args()
            args.scenario = "chat-two"
            args.x = args.z = None
            args.chat_wait_seconds = 30
            preflights = []
            def preflight(*values, **options):
                preflights.append(options["require_world"])

            def process(*values, **options):
                raise AssertionError("chat scenario must not launch a child process")

            with patch.object(run, "AcceptanceRunner", Runner), patch.object(run, "preflight", side_effect=preflight), \
                    patch.object(run, "run_chat", return_value=[{"sender": "client-1"}, {"sender": "host"}]):
                self.assertEqual(run.execute(args, process=process), 0)
            self.assertEqual(preflights, [False])
            summary = json.loads((Path(temporary) / "summary.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["state"], "confirmed-native-chat-two-directions")

    def test_preflight_failure_never_dispatches(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temporary:
            class Runner:
                def __init__(self, args):
                    self.args = args
                    self.directory = Path(temporary)
                    self.summary_path = self.directory / "summary.json"
                    self.summary = {"step": "preflight", "runtime_run_id": "run"}

                def persist(self):
                    self.summary_path.write_text(json.dumps(self.summary), encoding="utf-8")

            called = []
            def process(*args, **kwargs):
                called.append((args, kwargs))
                raise AssertionError("scenario must not start")

            with patch.object(run, "AcceptanceRunner", Runner), patch.object(run, "preflight", side_effect=run.AcceptanceError("failed=1")):
                self.assertEqual(run.execute(self.args(), process=process), 1)
            self.assertEqual(called, [])
            summary = json.loads((Path(temporary) / "summary.json").read_text(encoding="utf-8"))
            self.assertIn("failed=1", summary["error"]["cause"])
            self.assertNotIn("dispatch", summary)

    def test_timeout_is_unknown_and_never_replayed(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temporary:
            class Runner:
                def __init__(self, args):
                    self.args = args
                    self.directory = Path(temporary)
                    self.summary_path = self.directory / "summary.json"
                    self.summary = {"step": "preflight", "runtime_run_id": "run"}

                def persist(self):
                    self.summary_path.write_text(json.dumps(self.summary), encoding="utf-8")

                def checkpoint(self, step):
                    self.summary["step"] = step
                    self.persist()

            calls = []
            def process(*args, **kwargs):
                calls.append(args)
                raise run.subprocess.TimeoutExpired(args[0], 600)

            with patch.object(run, "AcceptanceRunner", Runner), patch.object(run, "preflight", return_value=None):
                self.assertEqual(run.execute(self.args(), process=process), 1)
            self.assertEqual(len(calls), 1)
            summary = json.loads((Path(temporary) / "summary.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["dispatch"]["state"], "observation-timeout")
            self.assertIn("no retry", summary["error"]["behavior"])


class FrameworkCandidateTests(unittest.TestCase):
    def setUp(self):
        import uuid
        from runtime import Runtime, framework_module_paths
        self.temp = tempfile.TemporaryDirectory(dir=Path(__file__).parent)
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.root = self.directory / '_MPTest'
        self.run_id = str(uuid.uuid4())
        self.digest = hashlib.sha256(b'plugin-candidate').hexdigest()
        self.candidate = self.directory / 'MT2MultiplayerPlugin.dll'
        self.candidate.write_bytes(b'plugin-candidate')
        self.manifests, self.identities, self.processes, self.modules = {}, {}, {}, {}
        for number, peer in enumerate(run.PEERS, 1):
            root = self.root / peer
            value = dict(schema_version=2, runtime_kind='framework-plugin', framework_profile=str(root / 'profile'),
                         run_id=self.run_id, pid=200+number, process_start_time=str(133400000000000000+number),
                         runtime_root=str(root), executable_path=str(root / 'game/MT2.exe'), peer_id=number,
                         owner=dict(kind='MPTest', run_id=self.run_id))
            modules = framework_module_paths(value)
            for name, path in modules.items():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'plugin-candidate' if name == 'MT2MultiplayerPlugin.dll' else name.encode())
            value['installed_binary_sha256'] = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in modules.items()}
            manifest = root / 'instance.json'
            manifest.write_text(json.dumps(value), encoding='utf-8')
            self.manifests[peer], self.identities[peer] = manifest, value
            self.processes[value['pid']] = dict(value, owned_by_current_user=True)
            self.modules[value['pid']] = [str(path) for path in modules.values()]
        self.factory = lambda root: Runtime(root, inspector=lambda pid: self.processes.get(pid),
                                           module_inspector=lambda pid: self.modules[pid])
        self.freeze, self.setup = self.directory / 'freeze.json', self.directory / 'setup.json'
        self.freeze.write_text(json.dumps(dict(dll_sha256=self.digest)), encoding='utf-8')
        self.setup.write_text(json.dumps(dict(passed=True, runtime_run_id=self.run_id)), encoding='utf-8')
        from types import SimpleNamespace
        from unittest.mock import Mock
        self.runner = SimpleNamespace(root=self.root, manifests=self.manifests, summary=dict(runtime_run_id=self.run_id),
            load_manifests=Mock(), checked=Mock(), inspect=Mock(side_effect=AssertionError('No native inspect before identity completion')))

    def preflight(self, candidate=None):
        run.preflight(self.runner, self.freeze, self.setup, self.factory, require_world=False,
                      candidate_binary=self.candidate if candidate is None else candidate)

    def test_exact_framework_candidate_and_module_paths(self):
        self.preflight()
        candidates = [call.args[1] for call in self.runner.checked.call_args_list if call.args[0].startswith('selected-candidate-')]
        self.assertEqual(len(candidates), 2)
        self.assertTrue(all(item['module'] == 'MT2MultiplayerPlugin.dll' and item['sha256'] == self.digest for item in candidates))
        self.runner.inspect.assert_not_called()

    def test_framework_requires_explicit_candidate_before_native_read(self):
        from runtime import RuntimeErrorDetail
        with self.assertRaisesRegex(RuntimeErrorDetail, 'Explicit candidate'):
            run.preflight(self.runner, self.freeze, self.setup, self.factory, require_world=False)
        self.runner.inspect.assert_not_called()

    def test_wrong_candidate_name_hash_and_freeze_reject(self):
        from runtime import RuntimeErrorDetail
        wrong = self.directory / 'MT2_Multiplayer.dll'
        wrong.write_bytes(self.candidate.read_bytes())
        with self.assertRaisesRegex(RuntimeErrorDetail, 'exact candidate filename'):
            self.preflight(wrong)
        self.candidate.write_bytes(b'other')
        with self.assertRaisesRegex(RuntimeErrorDetail, 'differs from installed'):
            self.preflight()
        self.candidate.write_bytes(b'plugin-candidate')
        self.freeze.write_text(json.dumps(dict(dll_sha256='0'*64)), encoding='utf-8')
        with self.assertRaisesRegex(run.AcceptanceError, 'differs from freeze'):
            self.preflight()

    def test_actual_runtime_rejects_client_pid_and_module_drift(self):
        from runtime import RuntimeErrorDetail
        self.processes[202]['process_start_time'] = '9'
        with self.assertRaisesRegex(RuntimeErrorDetail, 'start token changed'):
            self.preflight()
        self.processes[202]['process_start_time'] = self.identities['client-1']['process_start_time']
        self.modules[202].clear()
        with self.assertRaisesRegex(RuntimeErrorDetail, 'exact selected module path'):
            self.preflight()
        self.runner.inspect.assert_not_called()

    def test_building_dispatch_forwards_selected_root_and_binary(self):
        args = argparse.Namespace(scenario='building-ui', expected_cost=1000, x=8100, z=8550,
            setup=self.setup, freeze=self.freeze, type='Respawn', variant='base', test_root=self.root,
            candidate_binary=self.candidate)
        command = run.command(args)
        self.assertEqual(command[command.index('--test-root')+1], str(self.root))
        self.assertEqual(command[command.index('--candidate-binary')+1], str(self.candidate))
        self.assertEqual(Path(command[3]).name, 'placement_ui_acceptance.py')
        self.assertIn('--reuse-installed', command)

    def test_cli_preserves_explicit_selection(self):
        with patch.object(run, 'execute', return_value=0) as execute:
            self.assertEqual(run.main(['--scenario','building-ui','--setup',str(self.setup),'--freeze',str(self.freeze),
                '--test-root',str(self.root),'--candidate-binary',str(self.candidate),'--expected-cost','1000','--x','8100','--z','8550']), 0)
        args = execute.call_args.args[0]
        self.assertEqual(args.test_root, self.root)
        self.assertEqual(args.candidate_binary, self.candidate)

    def test_underlying_placement_rechecks_candidate_before_actions(self):
        from tools import placement_ui_acceptance as placement
        from runtime import RuntimeErrorDetail
        with patch.object(placement, 'Runtime', side_effect=self.factory):
            placement.validate_candidate(self.runner, dict(dll_sha256=self.digest), self.candidate)
            with self.assertRaisesRegex(RuntimeErrorDetail, 'Explicit candidate'):
                placement.validate_candidate(self.runner, dict(dll_sha256=self.digest), None)
            self.processes[202]['process_start_time'] = '9'
            with self.assertRaisesRegex(RuntimeErrorDetail, 'start token changed'):
                placement.validate_candidate(self.runner, dict(dll_sha256=self.digest), self.candidate)
        self.runner.inspect.assert_not_called()


if __name__ == "__main__":
    unittest.main()

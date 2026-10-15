from __future__ import annotations

import json
import logging
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import uuid

WORKSPACE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(WORKSPACE / "tools" / "mcp"))
from runtime import Runtime, RuntimeErrorDetail, atomic_json, read_json, shared_open, windows_process
from server import dispatch, tools_for


class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent)
        self.addCleanup(self.temporary.cleanup)
        self.instances = Path(self.temporary.name) / "_MPTest"
        self.root = self.instances / "host"
        self.diagnostics = self.root / "diagnostics"
        self.diagnostics.mkdir(parents=True)
        self.game = self.root / "game"
        self.game.mkdir()
        self.manifest = {"schema_version": 1, "run_id": str(uuid.uuid4()), "pid": 222,
                         "process_start_time": "133400000000000000", "runtime_root": str(self.root),
                         "executable_path": str(self.game / "MT2.exe")}
        self.manifest["owner"] = {"kind": "MPTest", "run_id": self.manifest["run_id"]}
        self.manifest_path = self.root / "instance.json"
        atomic_json(self.manifest_path, self.manifest)
        self.process = {key: self.manifest[key] for key in ("pid", "process_start_time", "executable_path")}
        self.process["owned_by_current_user"] = True
        self.runtime = Runtime(self.instances, inspector=lambda pid: self.process, timeout=2)
        self.runtime.bind(str(self.manifest_path))
        self.write_status({"status": True, "inspect_engine": True, "set_time_scale": False})

    def write_status(self, capabilities):
        atomic_json(self.diagnostics / "status.json", {"schema_version": 1, "run_id": self.manifest["run_id"],
                                                       "pid": self.manifest["pid"], "game_thread_ready": True,
                                                       "capabilities": capabilities})

    def assert_error(self, code, operation):
        if code == "observation_timeout":
            self.runtime.timeout = 0.1
        with self.assertRaises(RuntimeErrorDetail) as caught:
            operation()
        self.assertEqual(caught.exception.code, code)
        return caught.exception

    def responder(self, ok=True):
        failures = []
        def worker():
            try:
                deadline = time.monotonic() + 2
                request_path = self.diagnostics / "request.json"
                while not request_path.exists():
                    if time.monotonic() > deadline:
                        raise AssertionError("request not observed")
                    time.sleep(0.005)
                request = read_json(request_path)
                response = {"schema_version": 1, "id": request["id"], "run_id": request["run_id"], "ok": ok}
                response.update({"result": {"clock": 123}} if ok else {"error": {"code": "not_loaded", "message": "No world loaded"}})
                atomic_json(self.diagnostics / "response.json", response)
            except Exception as exc:
                failures.append(exc)
        thread = threading.Thread(target=worker)
        thread.start()
        def finish():
            thread.join(3)
            self.assertFalse(thread.is_alive())
            self.assertEqual(failures, [])
        self.addCleanup(finish)

    def test_matching_main_thread_acknowledgement(self):
        self.responder()
        result = self.runtime.request("inspect_engine", {})
        self.assertTrue(result["ok"])
        self.assertEqual(result["result"]["clock"], 123)
        self.assertFalse((self.diagnostics / "request.lock").exists())

    def test_game_error_is_not_success(self):
        self.responder(ok=False)
        self.assert_error("not_loaded", lambda: self.runtime.request("status", {}))
        self.assertFalse((self.diagnostics / "request.lock").exists())

    def test_capability_false_hides_mutation_and_blocks_call(self):
        names = {tool.name for tool in tools_for(self.runtime)}
        self.assertNotIn("game_set_time_scale", names)
        self.assertNotIn("game_create_test_world", names)
        self.assertNotIn("game_exit_game", names)
        self.assertNotIn("game_capture_frame", names)
        self.assertNotIn("game_toggle_overlay", names)
        self.assertNotIn("game_dismiss_crash_report", names)
        self.assertNotIn("game_probe_building_placement", names)
        self.assertNotIn("game_place_test_building", names)
        self.assertNotIn("game_purchase_test_region", names)
        self.assertNotIn("game_focus_test_camera", names)
        self.assert_error("capability_unavailable", lambda: dispatch(self.runtime, "game_set_time_scale", {"scale": 2}))
        self.assertFalse((self.diagnostics / "request.json").exists())

    def test_verified_mutation_is_exposed_and_acknowledged(self):
        self.write_status({"set_time_scale": True})
        tools = {tool.name: tool for tool in tools_for(self.runtime)}
        self.assertIn("game_set_time_scale", tools)
        self.assertFalse(tools["game_set_time_scale"].annotations.readOnlyHint)
        self.assertFalse(tools["game_set_time_scale"].annotations.openWorldHint)
        self.responder()
        result = dispatch(self.runtime, "game_set_time_scale", {"scale": 2})
        self.assertTrue(result["ok"])
        self.assertEqual(read_json(self.diagnostics / "request.json")["args"], {"scale": 2})

    def test_owned_world_and_native_exit_require_explicit_capability(self):
        for operation in ("create_test_world", "exit_game"):
            self.assert_error("capability_unavailable", lambda: self.runtime.request(operation, {}))
        self.write_status({"create_test_world": True, "exit_game": True})
        names = {tool.name for tool in tools_for(self.runtime)}
        self.assertIn("game_create_test_world", names)
        self.assertIn("game_exit_game", names)
        self.responder()
        self.assertTrue(dispatch(self.runtime, "game_create_test_world", {})["ok"])

    def test_unready_game_thread_hides_operations(self):
        status = read_json(self.diagnostics / "status.json")
        status["game_thread_ready"] = False
        atomic_json(self.diagnostics / "status.json", status)
        self.assertEqual(self.runtime.available(), set())

    def test_timeout_retains_lock_and_prevents_replay(self):
        error = self.assert_error("observation_timeout", lambda: self.runtime.request("status", {}))
        self.assertIn("do not replay", str(error))
        first = read_json(self.diagnostics / "request.json")
        self.assertTrue(self.runtime.observe_pending()["pending"])
        second = Runtime(self.instances, inspector=lambda pid: self.process, timeout=0.1)
        second.bind(str(self.manifest_path))
        self.assert_error("mailbox_busy", lambda: second.request("status", {}))
        self.assertEqual(read_json(self.diagnostics / "request.json"), first)

    def test_late_matching_response_resolves_without_replay(self):
        self.assert_error("observation_timeout", lambda: self.runtime.request("status", {}))
        request = read_json(self.diagnostics / "request.json")
        atomic_json(self.diagnostics / "response.json", {"schema_version": 1, "run_id": request["run_id"],
                                                       "id": request["id"], "ok": True, "result": {"late": True}})
        result = self.runtime.observe_pending()
        self.assertFalse(result["pending"])
        self.assertEqual(result["response"]["result"], {"late": True})
        self.assertEqual(read_json(self.diagnostics / "request.json"), request)

    def test_stale_response_does_not_acknowledge(self):
        atomic_json(self.diagnostics / "response.json", {"schema_version": 1, "run_id": self.manifest["run_id"],
                                                       "id": str(uuid.uuid4()), "ok": True, "result": {}})
        self.assert_error("observation_timeout", lambda: self.runtime.request("status", {}))
        self.assertTrue((self.diagnostics / "request.lock").exists())

    def test_process_exit_releases_pending_lock(self):
        self.assert_error("observation_timeout", lambda: self.runtime.request("status", {}))
        self.process = None
        observed = self.runtime.observe_pending()
        self.assertTrue(observed["abandoned_after_process_exit"])
        self.assertFalse((self.diagnostics / "request.lock").exists())

    def test_process_query_failure_retains_pending_lock(self):
        self.assert_error("observation_timeout", lambda: self.runtime.request("status", {}))
        def denied(pid):
            raise RuntimeErrorDetail("process_query_failed", "Access denied")
        self.runtime.inspector = denied
        self.assert_error("process_query_failed", self.runtime.observe_pending)
        self.assertTrue((self.diagnostics / "request.lock").exists())

    def test_pid_reuse_is_not_original_process(self):
        self.process["process_start_time"] = "133400000000000001"
        self.assert_error("pid_reused", self.runtime.status)

    def test_pid_reuse_releases_only_original_pending_lock(self):
        self.assert_error("observation_timeout", lambda: self.runtime.request("status", {}))
        self.process["process_start_time"] = "133400000000000001"
        self.assertTrue(self.runtime.observe_pending()["abandoned_after_process_exit"])

    def test_invalid_matching_response_retains_lock(self):
        self.assert_error("observation_timeout", lambda: self.runtime.request("status", {}))
        request = read_json(self.diagnostics / "request.json")
        atomic_json(self.diagnostics / "response.json", {"schema_version": 1, "id": request["id"],
                                                       "run_id": request["run_id"], "ok": False, "error": {}})
        self.assert_error("invalid_response", self.runtime.observe_pending)
        self.assertTrue((self.diagnostics / "request.lock").exists())

    def test_oversize_request_never_publishes_and_releases_lock(self):
        self.assert_error("oversize_json", lambda: self.runtime.request("status", {"large": "x" * 262144}))
        self.assertFalse((self.diagnostics / "request.lock").exists())
        self.assertFalse((self.diagnostics / "request.json").exists())

    def test_failed_publication_releases_only_after_negative_identity_check(self):
        original = atomic_json
        def fail_write(path, value, **kwargs):
            if path.name == "request.json":
                raise RuntimeErrorDetail("write_failed", "Synthetic file sharing denial")
            return original(path, value, **kwargs)
        with patch("runtime.atomic_json", side_effect=fail_write):
            error = self.assert_error("write_failed", lambda: self.runtime.request("status", {}))
        self.assertIn("lock released", str(error))
        self.assertFalse((self.diagnostics / "request.lock").exists())

    def test_failed_publication_with_visible_uuid_retains_lock(self):
        original = atomic_json
        def fail_after_write(path, value, **kwargs):
            original(path, value, **kwargs)
            if path.name == "request.json":
                raise RuntimeErrorDetail("write_failed", "Synthetic ambiguous replacement failure")
        with patch("runtime.atomic_json", side_effect=fail_after_write):
            error = self.assert_error("write_failed", lambda: self.runtime.request("status", {}))
        self.assertIn("lock retained", str(error))
        self.assertEqual(read_json(self.diagnostics / "request.lock")["publication_state"], "failed")
        self.assertTrue(self.runtime.observe_pending()["pending"])

    def test_wrong_executable_rejects_binding(self):
        self.process["executable_path"] = str(self.root / "unowned.exe")
        self.assert_error("ownership_failed", lambda: self.runtime.bind(str(self.manifest_path)))

    def test_other_account_rejects_binding(self):
        self.process["owned_by_current_user"] = False
        self.assert_error("ownership_failed", lambda: self.runtime.bind(str(self.manifest_path)))

    def test_manifest_replacement_requires_rebind(self):
        altered = dict(self.manifest)
        altered["pid"] = 333
        atomic_json(self.manifest_path, altered)
        self.assert_error("manifest_changed", self.runtime.status)

    def test_stale_status_blocks_capability(self):
        status = read_json(self.diagnostics / "status.json")
        status["run_id"] = str(uuid.uuid4())
        atomic_json(self.diagnostics / "status.json", status)
        self.assert_error("stale_status", self.runtime.available)
        self.assertEqual({tool.name for tool in tools_for(self.runtime)},
                         {"list_instances", "bind_runtime", "runtime_status", "read_logs", "observe_pending", "recover_pending"})

    def test_wrong_runtime_manifest_rejected(self):
        manifest = dict(self.manifest)
        manifest["runtime_root"] = str(Path(self.temporary.name))
        atomic_json(self.manifest_path, manifest)
        self.assert_error("path_escape", lambda: self.runtime.bind(str(self.manifest_path)))

    def test_bounded_actual_mod_log_and_path_escape(self):
        (self.game / "mt2mp.log").write_bytes(b"old\nlatest\n")
        result = self.runtime.read_logs(max_bytes=7)
        self.assertEqual(result["text"], "latest\n")
        self.assertTrue(result["truncated"])
        self.assert_error("invalid_log", lambda: self.runtime.read_logs("../../private.txt"))

    @unittest.skipUnless(os.name == "nt", "Windows shared-delete file contract")
    def test_open_json_reader_does_not_block_atomic_replacement(self):
        path = self.diagnostics / "status.json"
        with shared_open(path) as reader:
            original = json.loads(reader.read())
            replacement = dict(original)
            replacement["game_thread_ready"] = False
            atomic_json(path, replacement)
            self.assertEqual(read_json(path), replacement)

    def test_invalid_snapshot_path_and_nonfinite_scale(self):
        self.assert_error("invalid_args", lambda: dispatch(self.runtime, "game_load_snapshot", {"snapshot_id": "../private"}))
        self.assert_error("invalid_args", lambda: dispatch(self.runtime, "game_set_time_scale", {"scale": float("nan")}))

    def test_unknown_tool_fails(self):
        self.assert_error("unknown_tool", lambda: dispatch(self.runtime, "execute_python", {}))

    def test_foreign_lock_is_never_cleared(self):
        atomic_json(self.diagnostics / "request.lock", {"id": str(uuid.uuid4()), "run_id": str(uuid.uuid4())})
        self.assert_error("foreign_lock", self.runtime.observe_pending)
        self.assertTrue((self.diagnostics / "request.lock").exists())

    def test_previous_owned_run_is_recovered_only_after_exit(self):
        self.assert_error("observation_timeout", lambda: self.runtime.request("status", {}))
        previous_run = self.manifest["run_id"]
        self.manifest["run_id"] = str(uuid.uuid4())
        self.manifest["owner"] = {"kind": "MPTest", "run_id": self.manifest["run_id"]}
        self.manifest["pid"] = 333
        atomic_json(self.manifest_path, self.manifest)
        old_process = dict(self.process)
        new_process = {**self.process, "pid": 333}
        processes = {222: old_process, 333: new_process}
        replacement = Runtime(self.instances, inspector=lambda pid: processes.get(pid), timeout=2)
        replacement.bind(str(self.manifest_path))
        self.assert_error("prior_process_alive", replacement.observe_pending)
        self.assertTrue((self.diagnostics / "request.lock").exists())
        processes[222] = None
        result = replacement.observe_pending()
        self.assertEqual(result["previous_run_id"], previous_run)
        self.assertTrue(result["abandoned_after_process_exit"])
        self.assertFalse((self.diagnostics / "request.lock").exists())

    def test_explicit_old_manifest_recovers_legacy_lock_without_live_binding(self):
        record = {"id": str(uuid.uuid4()), "run_id": self.manifest["run_id"], "pid": self.manifest["pid"],
                  "process_start_time": self.manifest["process_start_time"], "op": "create_test_world"}
        atomic_json(self.diagnostics / "request.lock", record)
        unbound = Runtime(self.instances, inspector=lambda pid: self.process, timeout=2)
        self.assert_error("prior_process_alive", lambda: unbound.recover_pending(str(self.manifest_path)))
        self.process = None
        result = unbound.recover_pending(str(self.manifest_path))
        self.assertTrue(result["process_exit_verified"])
        self.assertFalse((self.diagnostics / "request.lock").exists())

    def test_legacy_receipt_mismatch_never_releases_lock(self):
        atomic_json(self.diagnostics / "request.lock", {"id": str(uuid.uuid4()), "run_id": str(uuid.uuid4()),
                                                       "pid": self.manifest["pid"], "process_start_time": self.manifest["process_start_time"]})
        self.process = None
        self.assert_error("receipt_mismatch", lambda: self.runtime.recover_pending(str(self.manifest_path)))
        self.assertTrue((self.diagnostics / "request.lock").exists())

    @unittest.skipUnless(os.name == "nt", "Windows process contract")
    def test_real_windows_process_identity(self):
        identity = windows_process(os.getpid())
        self.assertEqual(identity["pid"], os.getpid())
        self.assertTrue(identity["owned_by_current_user"])
        self.assertTrue(identity["process_start_time"].isdigit())
        self.assertEqual(Path(identity["executable_path"]).resolve(), Path(sys.executable).resolve())


if __name__ == "__main__":
    logging.basicConfig(level=logging.WARNING)
    unittest.main()

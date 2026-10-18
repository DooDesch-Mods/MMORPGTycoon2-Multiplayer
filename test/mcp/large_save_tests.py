"""Large-save acceptance guards using owned temporary fixtures, without games."""
import argparse
import contextlib
import hashlib
import io
import json
import os
import re
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "tools"))
import large_save_acceptance as gate
from local_acceptance import AcceptanceError, AcceptanceRunner, write_json


def arguments(root):
    args = gate.parser().parse_args(["--test-root", str(root), "--no-memory-counters", "--poll-interval", "0.01"])
    args.cleanup, args.snapshot_id = False, gate.INPUT_ID
    return args


def progress(direction="send", phase="transferring", acknowledged=0, queued=100, **changes):
    value = {"direction": direction, "id": gate.INPUT_ID, "phase": phase, "state": "fixture",
             "bytes_total": 1000, "chunks_total": 100, "bytes_queued": queued * 10 if direction == "send" else 0,
             "chunks_queued": queued if direction == "send" else 0,
             "acknowledged_min": acknowledged if direction == "send" else 0,
             "bytes_acknowledged_min": acknowledged * 10 if direction == "send" else 0,
             "peers_expected": 1 if direction == "send" else 0, "peers_ready": 1 if direction == "send" else 0,
             "peers_verified": 0, "peers_loaded": 0,
             "bytes_written": acknowledged * 10 if direction == "receive" else 0,
             "chunks_received": acknowledged if direction == "receive" else 0,
             "pending_load": phase == "awaiting_load", "transfer_active": phase in {"awaiting_receivers", "transferring"},
             "first_failure": "", "discarded_after_failure": 0}
    value.update(changes)
    return value


def receipt(manifest, tool, request_id, cause="observation_timeout", op=None):
    return {"passed": False, "calls": [
        {"tool": "bind_runtime", "state": "confirmed", "result": {"isError": False,
            "structuredContent": {"bound": manifest, "process_identity_verified": True}}},
        {"tool": tool, "state": "failed", "result": {"isError": True, "content": [{"type": "text", "text":
            f"{cause}: Request id={request_id} op={op or tool.removeprefix('game_')}: no matching response within 20s; lock retained; do not replay"}]}}]}


class Guards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="large_save_fixture_", dir=Path(__file__).parent)
        self.addCleanup(self.temp.cleanup)
        self.workspace = Path(self.temp.name)
        self.root = self.workspace / "_MPTest"
        self.run_id = str(uuid.uuid4())
        self.manifest = {"schema_version": 1, "run_id": self.run_id, "pid": 123,
            "process_start_time": "133400000000000000", "peer_id": 1, "role": "game",
            "runtime_root": str(self.root / "host"), "executable_path": str(self.root / "host/game/MT2.exe"),
            "owner": {"kind": "MPTest", "run_id": self.run_id}}
        self.manifest_path = self.root / "host/instance.json"
        write_json(self.manifest_path, self.manifest)
        self.runner = gate.LargeSaveRunner(arguments(self.root), self.workspace)
        self.runner.manifests["host"] = self.manifest_path
        self.runner.summary["runtime_run_id"] = self.run_id

    def test_root_outside_owned_workspace_rejected(self):
        with self.assertRaisesRegex(AcceptanceError, "exact owned _MPTest"):
            gate.LargeSaveRunner(arguments(self.workspace / "foreign"), self.workspace)

    def test_arbitrary_input_sha_rejected_before_file_or_game(self):
        write_json(self.root / "host/diagnostics" / f"snapshot-{gate.INPUT_ID}.json",
                   {"snapshot_id": gate.INPUT_ID, "bytes": gate.INPUT_BYTES, "sha256": "0" * 64, "path": "private.vrz"})
        with self.assertRaisesRegex(AcceptanceError, "authorized imported snapshot receipt differs"):
            self.runner.input_snapshot()
        self.assertEqual(self.runner.summary["mutation_attempts"], {})

    def test_original_path_not_allowed(self):
        write_json(self.root / "host/diagnostics" / f"snapshot-{gate.INPUT_ID}.json",
                   {"snapshot_id": gate.INPUT_ID, "bytes": gate.INPUT_BYTES, "sha256": gate.INPUT_SHA, "path": str(self.workspace / "private.vrz")})
        with self.assertRaisesRegex(AcceptanceError, "snapshot path differs"):
            self.runner.input_snapshot()

    def test_mutation_once_even_after_unknown_failure(self):
        with patch.object(self.runner, "single", side_effect=AcceptanceError("lost observation")) as call:
            with self.assertRaisesRegex(AcceptanceError, "lost observation"):
                self.runner.once("load", "host", "game_load_snapshot", {"snapshot_id": gate.INPUT_ID})
            with self.assertRaisesRegex(AcceptanceError, "mutation replay blocked"):
                self.runner.once("load-again", "host", "game_load_snapshot", {"snapshot_id": gate.INPUT_ID})
            self.assertEqual(call.call_count, 1)

    def test_placement_and_generic_action_tools_excluded(self):
        for tool in ("game_place_test_building", "game_execute_action", "game_purchase_test_region"):
            with self.subTest(tool=tool), self.assertRaisesRegex(AcceptanceError, "excluded"):
                self.runner.single("foreign", "host", tool)

    def test_pending_uuid_wrong_native_op_rejected(self):
        request = str(uuid.uuid4())
        with self.assertRaisesRegex(AcceptanceError, "matching native observation timeout"):
            gate.pending_id(receipt(self.manifest, "game_load_snapshot", request, op="capture_snapshot"), "game_load_snapshot")

    def test_publication_failure_does_not_enter_observer(self):
        request = str(uuid.uuid4())
        value = receipt(self.manifest, "game_capture_snapshot", request, cause="publication_failed")
        with self.assertRaisesRegex(AcceptanceError, "matching native observation timeout"):
            gate.pending_id(value, "game_capture_snapshot")

    def test_pending_ack_checks_uuid_and_run(self):
        request = str(uuid.uuid4())
        for field in ("id", "run_id"):
            value = {"pending": False, "response": {"schema_version": 1, "id": request, "run_id": self.run_id, "ok": True, "result": {}}}
            value["response"][field] = str(uuid.uuid4())
            with self.subTest(field=field), self.assertRaisesRegex(AcceptanceError, "identity differs"):
                gate.matching_pending(value, request, self.run_id)

    def test_pending_abandoned_is_not_success(self):
        with self.assertRaisesRegex(AcceptanceError, "ended without acknowledgement"):
            gate.matching_pending({"pending": False, "abandoned_after_process_exit": True}, str(uuid.uuid4()), self.run_id)

    def test_native_capture_timeout_is_not_acknowledged(self):
        request = str(uuid.uuid4())
        value = {"pending": False, "response": {"schema_version": 1, "id": request, "run_id": self.run_id,
            "ok": False, "error": {"code": "CAPTURE_TIMEOUT", "message": "native bound30s"}}}
        with self.assertRaisesRegex(AcceptanceError, "CAPTURE_TIMEOUT"):
            gate.matching_pending(value, request, self.run_id)

    def test_matching_delayed_ack_no_mutation_replay(self):
        request = str(uuid.uuid4())
        path = self.workspace / "original-receipt.json"
        write_json(path, receipt(self.manifest, "game_load_snapshot", request))
        calls = []
        responses = [{"pending": True, "request_id": request},
                     {"pending": False, "response": {"schema_version": 1, "id": request, "run_id": self.run_id, "ok": True,
                        "result": {"state": "snapshot_world_active", "snapshot_id": gate.INPUT_ID}}}]
        def fixture_single(runner, name, peer, tool, arguments=None):
            calls.append(tool)
            if tool == "game_load_snapshot":
                runner.summary["jobs"].append({"evidence_path": str(path), "state": "failed"})
                raise AcceptanceError("original matching observation timeout")
            if tool == "observe_pending":
                return responses.pop(0)
            if tool == "runtime_status":
                return {"native_world": {"active_mode": 4}}
            raise AssertionError(tool)
        with patch.object(AcceptanceRunner, "single", fixture_single), patch.object(self.runner, "memory"), patch.object(gate.time, "sleep"):
            value = self.runner.once("load", "host", "game_load_snapshot", {"snapshot_id": gate.INPUT_ID})
        self.assertEqual(value["state"], "snapshot_world_active")
        self.assertEqual(calls.count("game_load_snapshot"), 1)
        self.assertEqual(calls.count("observe_pending"), 2)
        self.assertEqual(self.runner.summary["jobs"][0]["state"], "confirmed-after-pending-ack")
        self.assertFalse(self.runner.summary["full_coop_proven"])

    def test_pending_timeout_bound_preserves_lock_no_replay(self):
        request = str(uuid.uuid4())
        self.runner.args.action_timeout = 1
        with patch.object(gate.time, "monotonic", side_effect=[0, 2]), patch.object(AcceptanceRunner, "single") as call:
            with self.assertRaisesRegex(AcceptanceError, "lock retained; no replay"):
                self.runner.await_pending("host", "game_capture_snapshot", request)
        call.assert_not_called()

    def test_bound_manifest_change_rejects_pending_observation(self):
        value = receipt({**self.manifest, "pid": 999}, "game_load_snapshot", str(uuid.uuid4()))
        with self.assertRaisesRegex(AcceptanceError, "bound identity differs"):
            self.runner.validate_bound(value, "host")

    def test_schema_missing_progress_blocks(self):
        with self.assertRaisesRegex(AcceptanceError, "structured progress is required"):
            gate.progress_schema(None)

    def test_queue_only_does_not_reset_idle_watch(self):
        watch = gate.ProgressWatch(gate.INPUT_ID, "send", 10, 90, 100, 0)
        self.assertFalse(watch.observe(progress(queued=1), 0))
        self.assertFalse(watch.observe(progress(queued=50), 9))
        with self.assertRaisesRegex(AcceptanceError, "idle=11.000s required<=10s"):
            watch.observe(progress(queued=100), 11)

    def test_ack_progress_can_exceed60_seconds_total(self):
        watch = gate.ProgressWatch(gate.INPUT_ID, "send", 10, 90, 500, 0)
        for second in range(0, 200, 5):
            self.assertFalse(watch.observe(progress(acknowledged=second // 5), second))

    def test_active_requires_all_receipt_and_loaded_acks(self):
        watch = gate.ProgressWatch(gate.INPUT_ID, "send", 10, 90, 100, 0)
        self.assertFalse(watch.observe(progress(phase="active", acknowledged=100, peers_verified=1), 1))
        self.assertTrue(watch.observe(progress(phase="active", acknowledged=100, peers_verified=1, peers_loaded=1), 2))

    def test_receive_requires_matching_uuid_and_full_written_file(self):
        watch = gate.ProgressWatch(gate.INPUT_ID, "receive", 10, 90, 100, 0)
        self.assertFalse(watch.observe(progress("receive", "active", 100, id=str(uuid.uuid4())), 1))
        self.assertTrue(watch.observe(progress("receive", "active", 100), 2))

    def test_native_first_failure_kept(self):
        watch = gate.ProgressWatch(gate.INPUT_ID, "send", 10, 90, 100, 0)
        with self.assertRaisesRegex(AcceptanceError, "disk full original fixture"):
            watch.observe(progress(phase="failed", first_failure="disk full original fixture"), 1)

    def test_ack_regression_rejected(self):
        watch = gate.ProgressWatch(gate.INPUT_ID, "send", 10, 90, 100, 0)
        watch.observe(progress(acknowledged=50), 1)
        with self.assertRaisesRegex(AcceptanceError, "progress regressed"):
            watch.observe(progress(acknowledged=49), 2)

    def test_awaiting_load_uses_action_bound_but_total_stays_bounded(self):
        watch = gate.ProgressWatch(gate.INPUT_ID, "send", 10, 90, 100, 0)
        watch.observe(progress(phase="awaiting_load", acknowledged=100, peers_verified=1), 1)
        self.assertFalse(watch.observe(progress(phase="awaiting_load", acknowledged=100, peers_verified=1), 70))
        with self.assertRaisesRegex(AcceptanceError, "elapsed=101.000s required<=100s"):
            watch.observe(progress(phase="awaiting_load", acknowledged=100, peers_verified=1), 101)

    def test_native_subscriber_needs_guards(self):
        correct = {"serialized_id": 0, "subscriber_count": gate.SUBSCRIBER_COUNT, "needs": [1] * 7}
        self.assertEqual(gate.subscriber_state(correct, 0), correct)
        for changes in ({"needs": [1] * 6}, {"needs": [float("nan")] * 7}, {"needs": [True] * 7},
                        {"subscriber_count": 1}, {"serialized_id": 4}):
            with self.subTest(changes=changes), self.assertRaises(AcceptanceError):
                gate.subscriber_state({**correct, **changes}, 0)

    def test_client_software_requires_real_replica_ack(self):
        with self.assertRaisesRegex(AcceptanceError, "replica acknowledgement absent"):
            gate.software_state({"software": {"rating": 1, "fun": 2}}, client=True)

    @unittest.skipUnless(os.name == "nt", "Windows own-process memory counter check")
    def test_memory_counters_read_only_current_python_pid(self):
        sys.path.insert(0, str(ROOT / "tools/mcp"))
        from runtime import windows_process
        identity = windows_process(os.getpid())
        value = gate.owned_peak_memory(identity)
        self.assertEqual(value["pid"], os.getpid())
        self.assertGreater(value["peak_working_set_bytes"], 0)
        self.assertGreaterEqual(value["peak_working_set_bytes"], value["working_set_bytes"])

    def test_cli_nonfinite_or_unbounded_step_is_rejected_before_runner(self):
        for args in (["--action-timeout", "nan"], ["--step-timeout", "66"], ["--poll-interval", "11"]):
            with self.subTest(args=args), patch.object(gate, "LargeSaveRunner") as runner, contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as caught:
                gate.main(args)
            self.assertEqual(caught.exception.code, 2)
            runner.assert_not_called()

    def full_fixture(self, read_rejected=False, capture_failed=False):
        client_manifest = {**self.manifest, "pid": 124, "peer_id": 2,
            "runtime_root": str(self.root / "client-1"), "executable_path": str(self.root / "client-1/game/MT2.exe")}
        write_json(self.root / "client-1/instance.json", client_manifest)
        snapshot_id = str(uuid.uuid4())
        raw = b"captured owned fixture" * 50
        captured = {"snapshot_id": snapshot_id, "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
        for peer in ("host", "client-1"):
            path = self.root / peer / "data/save/MPTestSession" / f"snapshot-{snapshot_id}.vrz"
            path.parent.mkdir(parents=True)
            path.write_bytes(raw)
            write_json(self.root / peer / "diagnostics" / f"snapshot-{snapshot_id}.json", {**captured, "path": str(path)})
        captured["path"] = str(self.root / "host/data/save/MPTestSession" / f"snapshot-{snapshot_id}.vrz")
        engine = {"game_thread_ready": True, "session_connected": True, "in_game": True,
            "capabilities": {"read_subscriber_needs": True},
            "native_world": {"mode_state": 4, "world_present": True, "quitting": False, "title_pending": False,
                             "edits": 0, "saves": 0, "map_task_pending": False},
            "clock": {"scale": 0, "paused": True, "day": 7},
            "simulation": {"cash": 1000, "record_present": True, "record_day": 7, "record_cash": 1000,
                           "record_income": 0, "record_expense": 0, "line_counts": [0] * 20, "line_income": [0] * 20},
            "software": {"rating": 1.25, "fun": 2.75, "replica_armed": True, "replica_bound": True,
                         "applied_revision": 1, "replica_error": ""},
            "load_identity": {"state": "incomplete", "scenery_supported": False}}
        calls = []
        did_capture = [False]
        def single(name, peer, tool, arguments=None):
            calls.append((peer, tool, arguments or {}))
            if tool == "observe_pending":
                return {"pending": False}
            if tool == "runtime_status":
                if not did_capture[0]:
                    return {"snapshot_progress": progress(direction="send" if peer == "host" else "receive", phase="idle", id="", queued=0)}
                value = progress("send" if peer == "host" else "receive", "active", 100, id=snapshot_id, bytes_total=len(raw))
                if peer == "host":
                    value.update(bytes_queued=len(raw), bytes_acknowledged_min=len(raw), peers_verified=1, peers_loaded=1)
                else:
                    value["bytes_written"] = len(raw)
                return {"snapshot_progress": value}
            if tool == "game_load_snapshot":
                return {"state": "snapshot_world_active", "snapshot_id": gate.INPUT_ID}
            if tool == "game_set_time_scale":
                return {"scale": 0}
            if tool == "game_inspect_engine":
                return engine
            if tool == "game_read_subscriber_needs":
                number = arguments["serialized_id"]
                if read_rejected and number == 0:
                    request = str(uuid.uuid4())
                    path = self.workspace / f"read-rejected-{peer}.json"
                    manifest = self.manifest if peer == "host" else client_manifest
                    value = receipt(manifest, tool, request, cause="SUBSCRIBER_READ_REJECTED")
                    write_json(path, value)
                    self.runner.summary["jobs"].append({"state": "failed", "evidence_path": str(path)})
                    raise AcceptanceError("SUBSCRIBER_READ_REJECTED: subscriber has no active brain")
                return {"serialized_id": number, "subscriber_count": gate.SUBSCRIBER_COUNT, "needs": [number] * 7}
            if tool == "game_capture_snapshot":
                if capture_failed:
                    raise AcceptanceError("CAPTURE_TIMEOUT: original native30s bound")
                did_capture[0] = True
                return captured
            raise AssertionError((name, peer, tool))
        original = {"snapshot_id": gate.INPUT_ID, "bytes": gate.INPUT_BYTES, "sha256": gate.INPUT_SHA, "file_verified": True}
        with patch.object(self.runner, "single", single), patch.object(self.runner, "poll_title"), \
                patch.object(self.runner, "input_snapshot", return_value=original) as preserved, \
                patch.object(self.runner, "command") as command:
            exit_code = self.runner.run()
        command.assert_not_called()
        self.assertEqual(preserved.call_count, 2)
        self.assertEqual(sum(tool == "game_load_snapshot" for _, tool, _ in calls), 1)
        self.assertEqual(sum(tool == "game_capture_snapshot" for _, tool, _ in calls), 1)
        self.assertFalse(self.runner.summary["full_coop_proven"])
        self.assertFalse(any(tool in {"game_place_test_building", "game_execute_action"} for _, tool, _ in calls))
        return exit_code, calls

    def test_full_fixture_load_capture_transfer_compare_without_placement(self):
        code, calls = self.full_fixture()
        self.assertEqual(code, 0)
        self.assertTrue(all(self.runner.summary["proof"].values()))
        self.assertEqual(sum(tool == "game_read_subscriber_needs" for _, tool, _ in calls), 8)
        self.assertEqual(self.runner.summary["state"], "confirmed-bounded-large-save-readback")
        self.assertEqual(self.runner.summary["load_identity"]["host"]["state"], "incomplete")

    def test_acknowledged_native_read_rejection_keeps_independent_transfer(self):
        code, calls = self.full_fixture(read_rejected=True)
        self.assertEqual(code, 1)
        self.assertTrue(self.runner.summary["proof"]["host_native_capture"])
        self.assertTrue(self.runner.summary["proof"]["transfer_and_client_load"])
        self.assertTrue(self.runner.summary["proof"]["software_equal"])
        self.assertTrue(self.runner.summary["proof"]["ledger_equal"])
        self.assertFalse(self.runner.summary["proof"]["seven_needs_equal"])
        self.assertEqual(len(self.runner.summary["readback_failures"]), 2)
        self.assertEqual(sum(tool == "game_read_subscriber_needs" for _, tool, _ in calls), 8)

    def test_native_capture_failure_stops_transfer_without_replay(self):
        code, calls = self.full_fixture(capture_failed=True)
        self.assertEqual(code, 1)
        self.assertIn("native30s bound", self.runner.summary["error"]["cause"])
        self.assertFalse(self.runner.summary["proof"]["transfer_and_client_load"])
        self.assertEqual(sum(peer == "client-1" and tool == "game_read_subscriber_needs" for peer, tool, _ in calls), 0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--label", default="large_save_guards")
    args = parser.parse_args()
    if not args.label.startswith("large_save") or not args.label.replace("_", "").replace("-", "").isalnum():
        parser.error("label requires large_save prefix and letters/digits/hyphens/underscores")
    log_path = Path(__file__).parent / f"{args.label}.log"
    with log_path.open("w", encoding="utf-8") as stream, contextlib.redirect_stdout(stream):
        result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Guards))
    value = {"state": "passed" if result.wasSuccessful() else "failed", "tests": result.testsRun,
             "failures": len(result.failures), "errors": len(result.errors), "scope": "fixtures only; no games or MCP live acceptance",
             "fixture_cleanup": not list(Path(__file__).parent.glob("large_save_fixture_*")), "log": str(log_path)}
    write_json(Path(__file__).parent / f"{args.label}.json", value)
    report = ROOT / "_drafts/large_save_acceptance.md"
    body = report.read_text(encoding="utf-8")
    value["report_links"] = {link: (report.parent / link).resolve().exists() for link in re.findall(r"\]\(([^)]+)\)", body)}
    value["plain_dashes"] = all("\u2013" not in path.read_text(encoding="utf-8") and "\u2014" not in path.read_text(encoding="utf-8") for path in (
        report, ROOT / "tools/large_save_acceptance.py", Path(__file__)))
    write_json(Path(__file__).parent / f"{args.label}.json", value)
    print(json.dumps(value, indent=2))
    if not result.wasSuccessful() or not value["fixture_cleanup"] or not value["plain_dashes"] or not all(value["report_links"].values()):
        raise SystemExit(1)


if __name__ == "__main__":
    main()

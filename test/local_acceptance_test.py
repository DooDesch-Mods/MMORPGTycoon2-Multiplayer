import contextlib
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import uuid

SPEC = importlib.util.spec_from_file_location("local_acceptance", Path(__file__).resolve().parents[1] / "tools/local_acceptance.py")
acceptance = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(acceptance)


def engine_state(peer, run_id, ledger_cash=95000):
    return {"schema_version": 1, "run_id": run_id, "game_thread_ready": True, "session_connected": True, "in_game": True,
            "capabilities": {"place_test_building": True}, "peer_id": peer,
            "native_world": {"world_present": True, "mode_state": 4, "quitting": False, "title_pending": False,
                             "edits": 0, "saves": 0, "map_task_pending": False},
            "clock": {"day": 1, "seconds": 449, "scale": 0, "paused": True}, "props": [],
            "simulation": {"cash": ledger_cash, "record_present": True, "record_day": 1, "record_cash": ledger_cash,
                           "record_income": 150000, "record_expense": 5000, "line_counts": [0] * 20, "line_income": [0] * 20}}


class SavedMCPAcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="local_acceptance-", dir=Path(__file__).resolve().parent)
        self.workspace = Path(self.temp.name)
        self.root = self.workspace / "_MPTest"
        self.run_id, self.input_id, self.new_id = str(uuid.uuid4()), str(uuid.uuid4()), str(uuid.uuid4())
        self.snapshot(self.input_id)
        for peer, name in ((1, "host"), (2, "client-1")):
            directory = self.root / name
            directory.mkdir(parents=True, exist_ok=True)
            identity = {"schema_version": 1, "run_id": self.run_id, "pid": 100 + peer, "peer_id": peer,
                        "process_start_time": str(1000 + peer), "owner": {"kind": "MPTest", "run_id": self.run_id},
                        "runtime_root": str(directory), "executable_path": str(directory / "game/MT2.exe")}
            acceptance.write_json(directory / "instance.json", identity)
        acceptance.write_json(self.root / "run.json", {"run_id": self.run_id})
        self.host, self.client = engine_state(1, self.run_id), engine_state(2, self.run_id)
        self.host["capabilities"]["place_test_building"] = False
        self.calls, self.commands = [], []
        self.fail_tool, self.fail_cleanup, self.mismatch, self.missing_active_marker = None, False, False, False
        self.timeout_tool, self.bad_spec_hash, self.bad_bind = None, False, False
        self.post_mismatch, self.run_changed_at_cleanup = False, False
        self.host_loaded, self.client_loaded = False, False
        self.startup_false_polls, self.title_crash = 0, False
        self.output = contextlib.redirect_stdout(io.StringIO())
        self.output.__enter__()

    def tearDown(self):
        self.output.__exit__(None, None, None)
        self.assertTrue(self.workspace.resolve().is_relative_to(Path(__file__).resolve().parent))
        self.temp.cleanup()

    def snapshot(self, identifier):
        path = self.root / "host/data/profile/save/MPTestSession" / f"snapshot-{identifier}.vrz"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"native snapshot fixture")
        record = {"snapshot_id": identifier, "bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                  "path": str(path), "run_id": self.run_id}
        acceptance.write_json(self.root / "host/diagnostics" / f"snapshot-{identifier}.json", record)
        return record

    def arguments(self, *extra):
        return acceptance.parser().parse_args(["--snapshot-id", self.input_id, "--test-root", str(self.root), "--ready-timeout", "0.05", *extra])

    def fake_process(self, command, **kwargs):
        if command[3].endswith("run_job.py"):
            spec_path = Path(command[command.index("--spec") + 1])
            manifest = Path(command[command.index("--manifest") + 1])
            label = command[command.index("--label") + 1]
            identity = acceptance.read_json(manifest)
            peer = identity["peer_id"]
            spec = acceptance.read_json(spec_path)
            bound = {"process_identity_verified": True, "bound": {**identity, "pid": 999} if self.bad_bind else identity}
            calls = [{"tool": "bind_runtime", "state": "confirmed", "result": {"structuredContent": bound}, "index": 0}]
            failed = False
            for index, call in enumerate(spec["calls"], 1):
                tool = call["tool"]
                self.calls.append((peer, tool, call["arguments"]))
                if tool == self.fail_tool:
                    calls.append({"tool": tool, "state": "failed", "cause": "original native ledger mismatch", "index": index})
                    failed = True
                    break
                result = self.response(peer, tool, call["arguments"])
                calls.append({"tool": tool, "state": "confirmed", "index": index,
                              "result": {"structuredContent": {"schema_version": 1, "id": str(uuid.uuid4()), "run_id": self.run_id, "ok": True, "result": result} if tool.startswith("game_") else result}})
            evidence = {"spec_path": str(spec_path), "spec_sha256": "wrong" if self.bad_spec_hash else acceptance.hash_file(spec_path),
                        "passed": not failed, "calls": calls, "error": {"cause": "original native ledger mismatch"} if failed else None}
            path = self.workspace / "test/mcp/evidence" / f"{label}_fixture.json"
            acceptance.write_json(path, evidence)
            path.with_suffix(".log").write_text("original MCP fixture log\n", encoding="utf-8")
            kwargs["stdout"].write(json.dumps({"passed": not failed, "evidence": str(path)}) + "\n")
            if any(call["tool"] == self.timeout_tool for call in spec["calls"]):
                raise subprocess.TimeoutExpired(command, kwargs["timeout"])
            return subprocess.CompletedProcess(command, 1 if failed else 0)
        self.commands.append(command)
        kwargs["stdout"].write("owned command fixture\n")
        return subprocess.CompletedProcess(command, 1 if self.fail_cleanup and command[3].endswith("stop_local_test.py") else 0)

    def response(self, peer, tool, arguments):
        if tool == "game_load_snapshot":
            self.host_loaded = True
            return {"state": "snapshot_world_active", "snapshot_id": arguments["snapshot_id"]}
        if tool == "game_set_time_scale":
            return self.host
        if tool == "game_capture_snapshot":
            self.host["capabilities"]["place_test_building"] = True
            self.client_loaded = True
            return self.snapshot(self.new_id)
        if tool == "game_inspect_engine":
            result = copy.deepcopy(self.host if peer == 1 else self.client)
            if not (self.host_loaded if peer == 1 else self.client_loaded):
                result["in_game"] = False
                result["native_world"].update({"active_mode": 26 if self.title_crash else 2, "world_present": False,
                                               "crash_report_active": self.title_crash, "mode_state": -1})
                result["capabilities"]["load_snapshot"] = self.startup_false_polls == 0
                if peer == 1 and self.startup_false_polls > 0:
                    self.startup_false_polls -= 1
            if peer == 2 and (self.mismatch or self.post_mismatch and result["props"]):
                result["simulation"]["line_income"][19] = 1
            return result
        if tool == "read_logs":
            return {"text": "waiting" if self.missing_active_marker else f"[SNAPSHOT] WORLD_ACTIVE after native snapshot load id={self.new_id}", "truncated": False}
        if tool == "game_probe_building_placement":
            return {"state": "placement_validated_without_mutation"}
        if tool == "game_place_test_building":
            operation = str(uuid.uuid4())
            for engine, uid in ((self.host, 321), (self.client, 654)):
                engine["simulation"]["cash"] -= 50
                engine["simulation"]["record_cash"] -= 50
                engine["simulation"]["record_expense"] += 50
                engine["simulation"]["line_counts"][6] += 1
                engine["simulation"]["line_income"][6] -= 50
                engine["props"] = [{"uid": uid, "type": arguments["type"], "variant": arguments["variant"],
                                   "building": True, "region_owned": True, "x": arguments["x"], "y": 90, "z": arguments["z"]}]
            return {"state": "host_result_applied", "placement": {"operation_id": operation, "host_uid": 321, "local_uid": 654, "revision": 1}}
        if tool == "game_focus_test_camera":
            return {"state": "camera_focused", **arguments}
        if tool == "game_capture_frame":
            if self.run_changed_at_cleanup:
                acceptance.write_json(self.root / "run.json", {"run_id": str(uuid.uuid4())})
            name = "host" if peer == 1 else "client-1"
            path = self.root / name / "diagnostics" / f"frame-{uuid.uuid4()}.bmp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"native frame fixture")
            return {"path": str(path), "bytes": path.stat().st_size, "width": 1280, "height": 720, "pixel_sha256": "fixture"}
        raise AssertionError(f"unexpected tool {tool}")

    def execute(self, *extra):
        runner = acceptance.AcceptanceRunner(self.arguments(*extra), workspace=self.workspace)
        with patch.object(acceptance.subprocess, "run", side_effect=self.fake_process):
            code = runner.run()
        return code, acceptance.read_json(runner.summary_path)

    def test_compound_job_allows_repeated_tool_without_replay(self):
        runner = acceptance.AcceptanceRunner(self.arguments(), workspace=self.workspace)
        runner.load_manifests()
        planned = [
            {"tool": "game_set_time_scale", "arguments": {"scale": 1}},
            {"tool": "game_inspect_engine", "arguments": {}},
            {"tool": "game_set_time_scale", "arguments": {"scale": 0}},
        ]
        with patch.object(acceptance.subprocess, "run", side_effect=self.fake_process):
            receipt = runner.job("ordinary-time-window", "host", planned)
        self.assertTrue(receipt["passed"])
        self.assertEqual(runner.summary["jobs"][-1]["state"], "confirmed")
        self.assertEqual(self.calls, [(1, call["tool"], call["arguments"]) for call in planned])

    def test_repeated_tool_checks_second_native_acknowledgement(self):
        tool = "game_set_time_scale"
        planned = [{"tool": tool}, {"tool": tool}]
        receipt = {"calls": [{"tool": "bind_runtime", "index": 0}, *[
            {"tool": tool, "index": index, "state": "confirmed", "result": {"structuredContent": {
                "ok": index == 1, "result": {}, "error": "original pause rejection"}}}
            for index in (1, 2)]]}
        with self.assertRaisesRegex(acceptance.AcceptanceError, "original pause rejection"):
            acceptance.validate_job_results(receipt, planned)

    def test_compound_job_rejects_missing_or_reordered_acknowledgement(self):
        planned = [{"tool": "game_set_time_scale"}, {"tool": "game_inspect_engine"}]
        calls = [{"tool": "bind_runtime", "index": 0}, *[
            {"tool": call["tool"], "index": index, "state": "confirmed", "result": {
                "structuredContent": {"ok": True, "result": {}}}}
            for index, call in enumerate(planned, 1)]]
        for altered in (calls[:-1], [calls[0], calls[2], calls[1]]):
            with self.subTest(indices=[call["index"] for call in altered]):
                with self.assertRaises(acceptance.AcceptanceError):
                    acceptance.validate_job_results({"calls": altered}, planned)

    def test_complete_saved_jobs_check_before_place_reference_original_receipts_and_leave_windows(self):
        code, summary = self.execute("--camera-distance", "150")
        self.assertEqual(code, 0)
        self.assertTrue(summary["passed"])
        self.assertFalse(summary["full_coop_proven"])
        self.assertEqual(summary["cleanup"]["state"], "deliberately-skipped")
        self.assertEqual(self.commands, [])
        self.assertEqual(sum(tool == "game_place_test_building" for _, tool, _ in self.calls), 1)
        self.assertFalse(any(tool == "game_purchase_test_region" for _, tool, _ in self.calls))
        self.assertEqual(sum(tool == "game_focus_test_camera" for _, tool, _ in self.calls), 2)
        self.assertEqual(sum(tool == "game_capture_frame" for _, tool, _ in self.calls), 2)
        self.assertIn("host-client-ledger-before-placement", [check["check"] for check in summary["checks"]])
        for job in summary["jobs"]:
            self.assertTrue(Path(job["evidence_path"]).is_file())
            self.assertTrue(Path(job["spec_path"]).is_file())
            self.assertEqual(acceptance.hash_file(Path(job["evidence_path"])), job["evidence_sha256"])
            self.assertLessEqual(job["wait_bound_seconds"], 60)

    def test_startup_capability_false_then_true_is_observed_before_one_load(self):
        self.startup_false_polls = 1
        code, summary = self.execute("--ready-timeout", "1")
        self.assertEqual(code, 0)
        load_index = next(index for index, (_, tool, _) in enumerate(self.calls) if tool == "game_load_snapshot")
        self.assertEqual([tool for peer, tool, _ in self.calls[:load_index] if peer == 1], ["game_inspect_engine", "game_inspect_engine"])
        self.assertEqual(sum(tool == "game_load_snapshot" for _, tool, _ in self.calls), 1)
        self.assertIn("client-1-native-title-load-ready", [check["check"] for check in summary["checks"]])

    def test_startup_capability_remains_false_timeout_has_no_load(self):
        self.startup_false_polls = 100
        code, summary = self.execute()
        self.assertEqual(code, 1)
        self.assertIn("load_snapshot capability actual=false", summary["error"]["cause"])
        self.assertFalse(any(tool == "game_load_snapshot" for _, tool, _ in self.calls))
        self.assertFalse(any(tool == "game_place_test_building" for _, tool, _ in self.calls))

    def test_crash_dialog_requires_explicit_dismiss_and_runs_no_mutation(self):
        self.title_crash = True
        code, summary = self.execute()
        self.assertEqual(code, 1)
        self.assertIn("no dismiss operation requested", summary["error"]["cause"])
        self.assertTrue(all(tool == "game_inspect_engine" for _, tool, _ in self.calls))

    def test_before_placement_mismatch_blocks_mutation_and_optional_cleanup_is_owned(self):
        self.mismatch = True
        code, summary = self.execute("--cleanup")
        self.assertEqual(code, 1)
        self.assertIn("line_income", summary["error"]["cause"])
        self.assertEqual(summary["error"]["step"], "guard-ledger-before-placement")
        self.assertFalse(any(tool == "game_place_test_building" for _, tool, _ in self.calls))
        self.assertEqual(len(self.commands), 1)
        self.assertTrue(self.commands[0][3].endswith("stop_local_test.py"))
        self.assertEqual(summary["cleanup"]["state"], "confirmed")

    def test_failed_placement_preserves_original_cause_and_is_never_replayed(self):
        self.fail_tool = "game_place_test_building"
        code, summary = self.execute()
        self.assertEqual(code, 1)
        self.assertIn("original native ledger mismatch", summary["error"]["cause"])
        self.assertEqual(sum(tool == self.fail_tool for _, tool, _ in self.calls), 1)
        self.assertEqual(summary["mutation_retries"], 0)
        self.assertEqual(summary["jobs"][-1]["state"], "failed")
        self.assertTrue(Path(summary["jobs"][-1]["evidence_path"]).exists())
        self.assertFalse(any(tool == "game_capture_frame" for _, tool, _ in self.calls))

    def test_postplacement_ledger_mismatch_is_retained_without_replaying(self):
        self.post_mismatch = True
        code, summary = self.execute()
        self.assertEqual(code, 1)
        self.assertEqual(summary["error"]["step"], "guard-ledger-after-placement")
        self.assertEqual(sum(tool == "game_place_test_building" for _, tool, _ in self.calls), 1)
        self.assertFalse(any(tool == "game_capture_frame" for _, tool, _ in self.calls))

    def test_cleanup_refuses_changed_run(self):
        self.run_changed_at_cleanup = True
        code, summary = self.execute("--cleanup")
        self.assertEqual(code, 1)
        self.assertIn("runtime run changed", summary["cleanup"]["cause"])
        self.assertEqual(self.commands, [])

    def test_missing_new_snapshot_marker_times_out_without_placement(self):
        self.missing_active_marker = True
        code, summary = self.execute()
        self.assertEqual(code, 1)
        self.assertIn("native WORLD_ACTIVE", summary["error"]["cause"])
        self.assertFalse(any(tool == "game_place_test_building" for _, tool, _ in self.calls))

    def test_wrong_snapshot_hash_blocks_restart_and_all_mcp_calls(self):
        path = self.root / "host/diagnostics" / f"snapshot-{self.input_id}.json"
        record = acceptance.read_json(path)
        record["sha256"] = "0" * 64
        acceptance.write_json(path, record)
        code, summary = self.execute("--restart")
        self.assertEqual(code, 1)
        self.assertIn("SHA256 differs", summary["error"]["cause"])
        self.assertEqual(self.calls, [])
        self.assertEqual(self.commands, [])

    def test_explicit_restart_runs_owned_stop_then_start(self):
        code, summary = self.execute("--restart")
        self.assertEqual(code, 0)
        self.assertTrue(self.commands[0][3].endswith("stop_local_test.py"))
        self.assertTrue(self.commands[1][3].endswith("local_test.py"))
        self.assertEqual(len(self.commands), 2)

    def test_timeout_keeps_original_partial_receipt_without_mutation_retry(self):
        self.timeout_tool = "game_place_test_building"
        code, summary = self.execute()
        self.assertEqual(code, 1)
        self.assertIn("effects unconfirmed, no replay", summary["error"]["cause"])
        self.assertEqual(summary["jobs"][-1]["state"], "observation-timeout")
        self.assertTrue(Path(summary["jobs"][-1]["evidence_path"]).exists())
        self.assertEqual(sum(tool == self.timeout_tool for _, tool, _ in self.calls), 1)

    def test_process_start_failure_stays_distinct_from_observation_timeout(self):
        runner = acceptance.AcceptanceRunner(self.arguments(), workspace=self.workspace)
        with patch.object(acceptance.subprocess, "run", side_effect=PermissionError("process creation denied")):
            code = runner.run()
        summary = acceptance.read_json(runner.summary_path)
        self.assertEqual(code, 1)
        self.assertEqual(summary["jobs"][0]["state"], "failed-to-start")
        self.assertIn("process creation denied", summary["error"]["cause"])
        self.assertFalse(summary["jobs"][0].get("calls"))

    def test_late_readiness_poll_keeps_a_useful_native_reply_window(self):
        runner = acceptance.AcceptanceRunner(self.arguments(), workspace=self.workspace)
        runner.load_manifests()
        with patch.object(acceptance.subprocess, "run", side_effect=self.fake_process):
            runner.job("late-ready", "client-1", [{"tool": "game_inspect_engine", "arguments": {}}], remaining=5)
        job = runner.summary["jobs"][-1]
        spec = acceptance.read_json(job["spec_path"])
        self.assertEqual(spec["timeout_seconds"], 4)
        self.assertEqual(job["wait_bound_seconds"], 5)

    def test_cleanup_failure_is_visible_and_fails_acceptance(self):
        self.fail_cleanup = True
        code, summary = self.execute("--cleanup")
        self.assertEqual(code, 1)
        self.assertEqual(summary["state"], "cleanup-failed")
        self.assertEqual(summary["cleanup"]["state"], "failed")
        self.assertIn("exit=1", summary["cleanup"]["cause"])

    def test_forged_receipt_spec_hash_is_rejected_before_placement(self):
        self.bad_spec_hash = True
        code, summary = self.execute()
        self.assertEqual(code, 1)
        self.assertIn("spec identity differs", summary["error"]["cause"])
        self.assertFalse(any(tool == "game_place_test_building" for _, tool, _ in self.calls))

    def test_mismatched_bound_process_is_rejected_before_placement(self):
        self.bad_bind = True
        code, summary = self.execute()
        self.assertEqual(code, 1)
        self.assertIn("bound identity differs", summary["error"]["cause"])
        self.assertFalse(any(tool == "game_place_test_building" for _, tool, _ in self.calls))

    def test_ledger_guards_cover_record_day_and_all20_categories(self):
        self.assertEqual(acceptance.compare_ledgers(self.host, self.client)["clock_day"], 1)
        for field, value in (("record_day", 0), ("record_cash", 1), ("line_counts", [0] * 19)):
            with self.subTest(field=field):
                bad = copy.deepcopy(self.client)
                bad["simulation"][field] = value
                with self.assertRaises(acceptance.AcceptanceError):
                    acceptance.compare_ledgers(self.host, bad)

    def test_single_expense_guard_rejects_double_category_count(self):
        before = acceptance.ledger_state(self.host)
        after = copy.deepcopy(before)
        after["cash"] -= 50
        after["record_cash"] -= 50
        after["record_expense"] += 50
        after["line_counts"][6] += 2
        after["line_income"][6] -= 50
        with self.assertRaisesRegex(acceptance.AcceptanceError, "category=6 count=2 required=1"):
            acceptance.placement_expense(before, after)


if __name__ == "__main__":
    unittest.main()

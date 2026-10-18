import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch
import uuid

SPEC = importlib.util.spec_from_file_location("local_test", Path(__file__).resolve().parents[1] / "tools" / "local_test.py")
launcher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(launcher)


class LocalLauncherTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="local-launcher-test-", dir=Path(__file__).resolve().parent)
        self.base = Path(self.directory.name)
        self.source, self.build, self.root = self.base / "source", self.base / "build", self.base / "_MPTest"
        self.source.mkdir()
        self.build.mkdir()
        for name in ("MT2.exe", "steam_api64_original.dll", "SDL3.dll", "libphysfs.dll", "MT2_Multiplayer.dll", "steam_api64.dll", "private.vrz", "mt2mp.log"):
            (self.source / name).write_bytes(name.encode())
        (self.source / "Data").mkdir()
        (self.source / "Data" / "asset.dat").write_bytes(b"game asset")
        for name in launcher.OVERLAYS:
            (self.build / name).write_bytes(b"build:" + name.encode())
        self.hash_patch = patch.object(launcher, "EXPECTED_EXE_SHA256", launcher.sha256(self.source / "MT2.exe"))
        self.hash_patch.start()
        self.output = contextlib.redirect_stdout(io.StringIO())
        self.output.__enter__()

    def tearDown(self):
        self.output.__exit__(None, None, None)
        self.hash_patch.stop()
        self.assertTrue(self.base.resolve().is_relative_to(Path(__file__).resolve().parent))
        self.directory.cleanup()

    def args(self, players=2):
        return launcher.parser().parse_args(["--source", str(self.source), "--build", str(self.build), "--test-root", str(self.root), "-Players", str(players), "--ready-timeout", "0.01"])

    def test_copy_source_fidelity_overlay_private_exclusion_and_incremental(self):
        files = launcher.source_files(self.source, self.build)
        runtime = self.root / "host"
        launcher.prepare_instance(runtime, self.root, files)
        game = runtime / "game"
        self.assertEqual((game / "Data" / "asset.dat").read_bytes(), b"game asset")
        self.assertEqual((game / "steam_api64.dll").read_bytes(), b"build:steam_api64.dll")
        self.assertEqual((self.source / "steam_api64.dll").read_bytes(), b"steam_api64.dll")
        self.assertEqual((game / "steam_appid.txt").read_text(encoding="ascii").strip(), "486860")
        self.assertFalse((self.source / "steam_appid.txt").exists())
        self.assertFalse((game / "private.vrz").exists())
        self.assertFalse((game / "mt2mp.log").exists())
        with patch.object(launcher.shutil, "copy2") as copy:
            launcher.prepare_instance(runtime, self.root, files)
            copy.assert_not_called()
        (game / "SDL3.dll").write_bytes(b"tampered file")
        launcher.prepare_instance(runtime, self.root, files)
        self.assertEqual((game / "SDL3.dll").read_bytes(), b"SDL3.dll")
        (self.source / "Data" / "asset.dat").write_bytes(b"changed asset")
        launcher.prepare_instance(runtime, self.root, files)
        self.assertEqual((game / "Data" / "asset.dat").read_bytes(), b"changed asset")

    def test_executable_hash_rejected_before_copy(self):
        (self.source / "MT2.exe").write_bytes(b"unsupported")
        with self.assertRaisesRegex(launcher.LaunchError, "unsupported MT2.exe SHA256"):
            launcher.source_files(self.source, self.build)
        self.assertFalse(self.root.exists())

    def test_dropin_loader_is_not_mixed_with_local_steam_proxy(self):
        (self.source / "WSOCK32.dll").write_bytes(b"installed drop-in loader")
        files = launcher.source_files(self.source, self.build)
        self.assertFalse(any(Path(name).name.casefold() == "wsock32.dll" for name in files))
        self.assertEqual(files["steam_api64.dll"], self.build / "steam_api64.dll")
        self.assertEqual(files["steam_api64_original.dll"], self.source / "steam_api64_original.dll")

    def test_exclusive_preflight_preserves_existing_listener(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as endpoint:
            endpoint.bind(("127.0.0.1", 0))
            endpoint.listen(1)
            port = endpoint.getsockname()[1]
            with self.assertRaisesRegex(launcher.LaunchError, "exclusive loopback port preflight failed"):
                launcher.reserve_ports([port])
            self.assertEqual(endpoint.getsockname()[1], port)

    def test_stale_status_wrong_write_dir_and_thread_rejected(self):
        record = {"run_id": str(uuid.uuid4()), "pid": 123}
        data = self.root / "host" / "data"
        status = {"schema_version": 1, **record, "game_thread_ready": True, "game_thread_id": 17, "session_connected": True, "write_dir": str(data)}
        self.assertIsNone(launcher.game_readiness(status, record, data))
        self.assertIsNone(launcher.game_readiness({**status, "write_dir": str(data / "76561198000000000")}, record, data))
        self.assertIn("run_id/pid", launcher.game_readiness({**status, "run_id": str(uuid.uuid4())}, record, data))
        self.assertIn("PHYSFS", launcher.game_readiness({**status, "write_dir": str(self.source)}, record, data))
        self.assertIn("PHYSFS", launcher.game_readiness({**status, "write_dir": str(data / ".." / "outside")}, record, data))
        self.assertIn("PHYSFS", launcher.game_readiness({**status, "write_dir": "relative-data"}, record, data))
        self.assertIn("game thread", launcher.game_readiness({**status, "game_thread_id": 0}, record, data))
        self.assertIn("not connected", launcher.game_readiness({**status, "session_connected": False}, record, data))

    def test_sidecar_timeout_has_marker_log_and_required_duration(self):
        log = self.base / "sidecar.log"
        log.write_text("error: sidecar bind failed WSA=10048\n", encoding="utf-8")
        with self.assertRaisesRegex(launcher.LaunchError, "marker=CONTROL_READY required=0.001s") as failure:
            launcher.wait_marker(log, "CONTROL_READY", [], 0.001)
        self.assertIn("WSA=10048", str(failure.exception))

    def test_transient_status_reads_are_bounded_and_recovery_is_visible(self):
        record = {"pid": 123, "run_id": str(uuid.uuid4()), "peer_id": 1, "runtime_root": str(self.root / "host")}
        status = {"schema_version": 1, "run_id": record["run_id"], "pid": record["pid"], "game_thread_ready": True,
                  "game_thread_id": 17, "session_connected": True, "write_dir": str(self.root / "host" / "data" / "profile")}
        output = io.StringIO()
        with patch.object(launcher, "read_status_json", side_effect=[PermissionError("atomic replacement in progress")] * 20 + [status]), patch.object(launcher.time, "sleep"), contextlib.redirect_stdout(output):
            launcher.wait_games([record], [], 1)
        text = output.getvalue()
        self.assertEqual(text.count("warn: status read"), 1)
        self.assertIn("status read recovered peer=1 failures=20", text)

    @unittest.skipUnless(os.name == "nt", "Windows file sharing API")
    def test_shared_status_reader_atomic_replacement_recovers_and_invalid_json_is_visible(self):
        path = self.base / "status.json"
        path.write_text("invalid json", encoding="utf-8")
        with self.assertRaises(json.JSONDecodeError):
            launcher.read_status_json(path)
        launcher.atomic_json(path, {"sequence": 0})
        failures, transient_errors = [], []

        def writer():
            try:
                api = launcher.ctypes.WinDLL("kernel32", use_last_error=True)
                api.MoveFileExW.argtypes = [launcher.wintypes.LPCWSTR, launcher.wintypes.LPCWSTR, launcher.wintypes.DWORD]
                for sequence in range(1, 100):
                    temporary = path.with_suffix(".json.tmp")
                    temporary.write_text(json.dumps({"sequence": sequence}), encoding="utf-8")
                    for attempt in range(100):
                        if api.MoveFileExW(str(temporary), str(path), 0x1 | 0x8):
                            break
                        code = launcher.ctypes.get_last_error()
                        error = launcher.ctypes.WinError(code)
                        if code not in (5, 32):
                            raise error
                        transient_errors.append(str(error))
                        if len(transient_errors) == 1:
                            launcher.log("warn", f"test atomic replacement contention: {error}")
                        threading.Event().wait(0.001)
                    else:
                        raise OSError(f"test atomic replacement stalled attempts=100 cause={transient_errors[-1]}")
                if transient_errors:
                    launcher.log("info", f"test atomic replacement recovered transient_failures={len(transient_errors)}")
            except OSError as error:
                failures.append(str(error))

        thread = threading.Thread(target=writer)
        thread.start()
        try:
            for _ in range(200):
                self.assertIsInstance(launcher.read_status_json(path)["sequence"], int)
        finally:
            thread.join(timeout=5)
        self.assertFalse(thread.is_alive())
        self.assertEqual(failures, [])
        self.assertEqual(launcher.read_status_json(path)["sequence"], 99)

    def fake_launch(self, players=2, fail_peer=0):
        created = []

        class FakeProcess:
            def __init__(inner, command, **kwargs):
                inner.pid = 200 + len(created)
                inner.command, inner.returncode, inner.killed, inner.kwargs = command, None, False, kwargs
                created.append(inner)
                if "--mode" in command:
                    kwargs["stdout"].write(b"RELAY_READY\n" if "server" in command else b"CONTROL_READY\n")
                    kwargs["stdout"].flush()
                else:
                    env = kwargs["env"]
                    runtime = Path(env["MT2MP_RUNTIME_ROOT"])
                    status = {"schema_version": 1, "run_id": env["MT2MP_RUN_ID"], "pid": inner.pid,
                              "game_thread_ready": True, "game_thread_id": 17, "session_connected": True,
                              "write_dir": env["MT2MP_TEST_ROOT"]}
                    launcher.atomic_json(runtime / "diagnostics" / "status.json", status)
                    if int(env["MT2MP_PEER_ID"]) == fail_peer:
                        inner.returncode = 42

            def poll(inner):
                return inner.returncode

            def kill(inner):
                inner.killed, inner.returncode = True, 1

            def wait(inner, timeout=None):
                return inner.returncode

        class FakeIdentities:
            def identity(inner, process):
                return {"pid": process.pid, "process_start_time": str(10000 + process.pid), "executable_path": process.command[0]}

        with patch.object(launcher.subprocess, "Popen", FakeProcess), patch.object(launcher, "reserve_ports", return_value=[unittest.mock.Mock() for _ in range(players + 1)]):
            if fail_peer:
                with self.assertRaisesRegex(launcher.LaunchError, "exited code=42"):
                    launcher.launch(self.args(players), FakeIdentities())
            else:
                self.assertEqual(launcher.launch(self.args(players), FakeIdentities()), 0)
        return created, launcher.read_json(self.root / "run.json")

    def test_two_and_four_players_owned_manifest_ready_games_left_alive(self):
        for players in (2, 4):
            with self.subTest(players=players):
                if (self.root / "run.json").exists():
                    (self.root / "run.json").unlink()
                created, manifest = self.fake_launch(players)
                self.assertEqual(len(created), players * 2 + 1)
                self.assertEqual(manifest["state"], "transport-ready")
                self.assertFalse(manifest["world_ready"])
                self.assertTrue(all(not process.killed for process in created))
                launcher.validate_manifest(manifest, self.root)
                games = [record for record in manifest["processes"] if record["role"] == "game"]
                self.assertEqual([record["peer_id"] for record in games], list(range(1, players + 1)))
                self.assertEqual(len({record["runtime_root"] for record in games}), players)
                for game in games:
                    instance = launcher.read_json(Path(game["runtime_root"]) / "instance.json")
                    self.assertEqual(instance["process_start_time"], game["process_start_time"])
                    self.assertEqual(instance["owner"], manifest["owner"])
                sidecars = [process for process in created if "--mode" in process.command]
                self.assertTrue(all(process.kwargs["creationflags"] == (subprocess.CREATE_NO_WINDOW | subprocess.CREATE_BREAKAWAY_FROM_JOB) for process in sidecars))
                for process in created:
                    if "--mode" not in process.command:
                        self.assertEqual(process.kwargs["env"]["SteamAppId"], "486860")
                        self.assertEqual(process.kwargs["env"]["SteamGameId"], "486860")

    def test_failed_game_preserves_failure_exit_code_and_cleans_only_started(self):
        created, manifest = self.fake_launch(fail_peer=2)
        self.assertEqual(manifest["state"], "failed")
        self.assertIn("exited code=42", manifest["failure"])
        self.assertEqual(created[-1].returncode, 42)
        self.assertFalse(created[-1].killed)
        self.assertTrue(all(process.killed for process in created[:-1]))
        self.assertEqual(manifest["processes"][-1]["exit_code"], 42)

    def test_previous_live_run_prevents_copy_or_relaunch(self):
        _, manifest = self.fake_launch()
        identities = unittest.mock.Mock()
        identities.inspect.return_value = "alive"
        with patch.object(launcher, "prepare_instance") as prepare, self.assertRaisesRegex(launcher.LaunchError, "previous run still owns"):
            launcher.launch(self.args(), identities)
        prepare.assert_not_called()

    def test_previous_failure_manifest_and_native_logs_are_archived(self):
        _, manifest = self.fake_launch(fail_peer=2)
        native_log = self.root / "host" / "game" / "mt2mp.log"
        native_log.write_text("error: first failure\n", encoding="utf-8")
        launcher.archive_previous(self.root, manifest)
        native_log.write_text("new run\n", encoding="utf-8")
        archive = self.root / "runs" / manifest["run_id"]
        self.assertIn("first failure", (archive / "host" / "game" / "mt2mp.log").read_text(encoding="utf-8"))
        self.assertEqual(launcher.read_json(archive / "run.json")["failure"], manifest["failure"])

    def test_stop_validates_ownership_and_preserves_mismatched_process(self):
        _, manifest = self.fake_launch()
        identities = unittest.mock.Mock()
        identities.inspect.side_effect = ["identity-mismatch", "stopped:1", "stopped:1", "stopped:1", "stopped:1"]
        self.assertEqual(launcher.stop_manifest(self.root, identities), 0)
        saved = launcher.read_json(self.root / "run.json")
        self.assertEqual(saved["processes"][-1]["stop_state"], "identity-mismatch")
        manifest["processes"][0]["executable_path"] = str(self.source / "MT2.exe")
        launcher.atomic_json(self.root / "run.json", manifest)
        identities.reset_mock()
        with self.assertRaisesRegex(launcher.LaunchError, "outside test root"):
            launcher.stop_manifest(self.root, identities)
        identities.inspect.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "Windows process identity APIs")
    def test_exited_windows_process_does_not_query_unavailable_image(self):
        process = subprocess.Popen([sys.executable, "-c", "raise SystemExit(53)"], creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            self.assertEqual(process.wait(timeout=5), 53)
            processes = launcher.WindowsProcesses()
            record = {"pid": process.pid, "process_start_time": "1", "executable_path": str(self.source / "MT2.exe")}
            with patch.object(processes, "_identity", side_effect=OSError(31, "image unavailable for exited process")) as identity:
                self.assertEqual(processes.inspect(record), "exited:53")
                identity.assert_not_called()
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)

    @unittest.skipUnless(os.name == "nt", "Windows process identity APIs")
    def test_real_windows_creation_token_mismatch_does_not_terminate(self):
        process = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"], creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            processes = launcher.WindowsProcesses()
            identity = processes.identity(process)
            self.assertEqual(processes.inspect(identity), "alive")
            mismatch = {**identity, "process_start_time": str(int(identity["process_start_time"]) + 1)}
            self.assertEqual(processes.inspect(mismatch, terminate=True), "identity-mismatch")
            self.assertIsNone(process.poll())
            self.assertEqual(processes.inspect(identity, terminate=True), "stopped:1")
            self.assertEqual(process.wait(timeout=5), 1)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()

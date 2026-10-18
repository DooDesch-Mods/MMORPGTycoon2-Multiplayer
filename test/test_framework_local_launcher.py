"""Filesystem, fake-process and owned socket tests. No real processes are started."""
import copy
import importlib.util
import json
from pathlib import Path
import socket
import tempfile
import unittest
from unittest import mock

TOOL = Path(__file__).resolve().parents[1] / "tools/framework_local_test.py"
spec = importlib.util.spec_from_file_location("framework_launcher", TOOL)
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class FrameworkLauncherTests(unittest.TestCase):
    def setUp(self):
        fixtures = Path(__file__).resolve().parent / ".fixtures"
        fixtures.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="framework-launcher-", dir=fixtures)
        self.addCleanup(self.temporary.cleanup)
        self.home = Path(self.temporary.name)
        self.source, self.root = self.home / "source", self.home / "_MPTest"
        self.source.mkdir()
        for name in ("MT2.exe", "steam_api64.dll", "SDL3.dll", "steam_api64_original.dll", "MT2_Multiplayer.dll", "WSOCK32.dll"):
            (self.source / name).write_bytes(name.encode())
        (self.source / "Data").mkdir()
        (self.source / "Data/asset.dat").write_bytes(b"game asset")
        (self.source / "private.vrz").write_bytes(b"user save must not copy")
        self.framework = self.home / "framework"
        self.framework.mkdir()
        for name in ("WSOCK32.dll", "MT2Framework.dll"):
            (self.framework / name).write_bytes(("framework-" + name).encode())
        self.package = self.home / "multiplayer"
        self.package.mkdir()
        launcher.atomic_json(self.package / "mod.json", {"id": "doodesch.multiplayer", "type": "native", "entry": "MT2MultiplayerPlugin.dll"})
        self.binary = self.home / "MT2MultiplayerPlugin.dll"
        self.binary.write_bytes(b"explicit candidate")
        self.sidecar = self.home / "mt2mp_sidecar.exe"
        self.sidecar.write_bytes(b"sidecar")
        self.args = launcher.parser().parse_args(["--source", str(self.source), "--test-root", str(self.root),
            "--candidate-binary", str(self.binary), "--multiplayer-package", str(self.package),
            "--framework-runtime", str(self.framework), "--sidecar", str(self.sidecar),
            "--stock-steam-sha256", launcher.sha256(self.source / "steam_api64.dll"), "--ready-timeout", "0.01"])
        patch = mock.patch.object(launcher, "EXPECTED_EXE_SHA256", launcher.sha256(self.source / "MT2.exe"))
        patch.start()
        self.addCleanup(patch.stop)
        self.started = []

    def fake_popen(self, command, **kwargs):
        process = mock.Mock()
        process.pid = 1000 + len(self.started)
        process.returncode = None
        process.poll.side_effect = lambda: process.returncode
        process.kill.side_effect = lambda: setattr(process, "returncode", -9)
        process.wait.side_effect = lambda **_: process.returncode
        runtime = Path(kwargs["cwd"]).parent
        if Path(command[0]).name == "MT2.exe":
            config = launcher.read_json(runtime / "profile/data/doodesch.multiplayer/local-test.json")
            status = {"schema_version": 1, "run_id": config["run_id"], "pid": process.pid,
                "peer_id": config["peer_id"], "runtime_kind": "framework-plugin", "framework_profile": str(runtime / "profile"),
                "installed_binary_sha256": {name: launcher.sha256(runtime / relative) for name, relative in launcher.MODULES.items()},
                "write_dir": str(runtime / "profile/game-data"), "game_thread_ready": True,
                "game_thread_id": 123, "session_connected": True}
            launcher.atomic_json(runtime / "profile/diagnostics/multiplayer/status.json", status)
        else:
            kwargs["stdout"].write(b"RELAY_READY\nCONTROL_READY\n")
            kwargs["stdout"].flush()
        self.started.append((process, command, kwargs))
        return process

    def launch(self, *, fake=None):
        processes = mock.Mock()
        processes.identity.side_effect = lambda process: {"pid": process.pid, "process_start_time": str(process.pid + 5000),
            "executable_path": next(command[0] for p, command, _ in self.started if p is process)}
        with mock.patch.object(launcher.subprocess, "Popen", side_effect=fake or self.fake_popen), \
                mock.patch.object(launcher, "reserve_ports", return_value=[mock.Mock(), mock.Mock(), mock.Mock()]), \
                mock.patch.dict(launcher.os.environ, {"MT2MP_TEST_ROOT": "foreign-profile", "MT2MP_RUN_ID": "foreign-run"}):
            result = launcher.launch(self.args, processes)
        return result, launcher.read_json(self.root / "run.json")

    def test_two_peers_exact_layout_config_and_receipts(self):
        result, manifest = self.launch()
        self.assertEqual(result, 0)
        self.assertEqual(len(self.started), 5)
        self.assertEqual(manifest["schema_version"], 1)
        self.assertEqual(manifest["state"], "transport-ready")
        self.assertFalse(manifest["world_ready"])
        games = [r for r in manifest["processes"] if r["role"] == "game"]
        self.assertEqual([r["peer_id"] for r in games], [1, 2])
        launcher.validate_manifest(manifest, self.root)
        for record in games:
            runtime = Path(record["runtime_root"])
            self.assertEqual(record["schema_version"], 2)
            config = launcher.read_json(runtime / "profile/data/doodesch.multiplayer/local-test.json")
            self.assertEqual(set(config), {"schema_version", "run_id", "owner", "runtime_root", "peer_id", "session_id", "control_port", "session_token", "transport"})
            self.assertIs(type(config["session_id"]), int)
            self.assertTrue(0 < config["session_id"] < 2**64)
            self.assertRegex(config["session_token"], r"^[0-9a-f]{32}$")
            self.assertEqual(config["owner"], manifest["owner"])
            self.assertEqual(config["run_id"], manifest["run_id"])
            self.assertEqual(record["installed_binary_sha256"]["MT2MultiplayerPlugin.dll"], launcher.sha256(self.binary))
            self.assertEqual((runtime / "game/steam_api64.dll").read_bytes(), b"steam_api64.dll")
            self.assertFalse((runtime / "game/steam_api64_original.dll").exists())
            self.assertFalse((runtime / "game/MT2_Multiplayer.dll").exists())
            self.assertFalse((runtime / "game/private.vrz").exists())
            self.assertEqual(launcher.read_json(runtime / "game/mt2fw.json")["profile"], str(runtime / "profile"))
            self.assertIn("profile/data/doodesch.multiplayer/mt2mp.log", record["game_log_paths"][0].replace("\\", "/"))
        for _, command, kwargs in self.started:
            if Path(command[0]).name == "MT2.exe":
                self.assertFalse(any(k.upper().startswith("MT2MP_") for k in kwargs["env"]))
                self.assertEqual(kwargs["env"]["SteamAppId"], "486860")
            elif command[command.index("--mode") + 1] == "client":
                index = command.index("--local-connect-timeout-ms")
                self.assertEqual(command[index + 1], "10")

    def status_fixture(self):
        _, manifest = self.launch()
        record = next(r for r in manifest["processes"] if r["role"] == "game")
        runtime = Path(record["runtime_root"])
        return record, launcher.read_json(runtime / "profile/diagnostics/multiplayer/status.json"), runtime / "profile/game-data"

    def test_wrong_identity_hash_profile_thread_session_and_save_path_block(self):
        record, status, data = self.status_fixture()
        for name, wrong in (("schema_version", 2), ("run_id", "wrong"), ("pid", 999), ("peer_id", 2),
                ("runtime_kind", "standalone"), ("framework_profile", str(data)), ("installed_binary_sha256", {}),
                ("game_thread_id", 0), ("game_thread_ready", False), ("session_connected", False),
                ("write_dir", str(data / "child")), ("pid", True)):
            with self.subTest(name=name, wrong=wrong):
                altered = {**status, name: wrong}
                self.assertIsNotNone(launcher.game_readiness(altered, record, data))
        self.assertIsNone(launcher.game_readiness(status, record, data))

    def test_five_hashes_exact_no_legacy_substitute(self):
        record, status, data = self.status_fixture()
        for key in launcher.MODULES:
            altered = copy.deepcopy(status)
            altered["installed_binary_sha256"][key] = "0" * 64
            self.assertIn("installed_binary_sha256", launcher.game_readiness(altered, record, data))
        status["installed_binary_sha256"]["MT2_Multiplayer.dll"] = status["installed_binary_sha256"].pop("MT2MultiplayerPlugin.dll")
        self.assertIsNotNone(launcher.game_readiness(status, record, data))

    def test_actual_numeric_account_write_root_is_authoritative(self):
        record, status, data = self.status_fixture()
        account = data / "76561198013473990"
        account.mkdir()
        status["write_dir"] = str(account) + "/"
        self.assertIsNone(launcher.game_readiness(status, record, data))
        for path in (data / "user", account / "nested", data.parent / "76561198013473990",
                     data / ("9" * 21), account / ".."):
            with self.subTest(path=path):
                status["write_dir"] = str(path)
                self.assertIsNotNone(launcher.game_readiness(status, record, data))

    def test_wrong_stock_receipt_rejected_before_copy(self):
        self.args.stock_steam_sha256 = "0" * 64
        with self.assertRaisesRegex(launcher.LaunchError, "stock steam_api64.dll"):
            self.launch()
        self.assertFalse(self.started)
        self.assertFalse(self.root.exists())

    def test_candidate_requires_exact_filename(self):
        self.args.candidate_binary = self.source / "MT2_Multiplayer.dll"
        with self.assertRaisesRegex(launcher.LaunchError, "exact native filenames"):
            self.launch()
        self.assertFalse(self.started)

    def test_package_wrong_identity_and_extra_dll_rejected(self):
        launcher.atomic_json(self.package / "mod.json", {"id": "foreign", "type": "native", "entry": "MT2MultiplayerPlugin.dll"})
        with self.assertRaisesRegex(launcher.LaunchError, "package identity"):
            self.launch()
        launcher.atomic_json(self.package / "mod.json", {"id": "doodesch.multiplayer", "type": "native", "entry": "MT2MultiplayerPlugin.dll"})
        (self.package / "foreign.dll").write_bytes(b"foreign")
        with self.assertRaisesRegex(launcher.LaunchError, "unexpected package DLL"):
            self.launch()

    def test_previous_alive_blocks_before_copy(self):
        _, previous = self.launch()
        processes = mock.Mock()
        processes.inspect.return_value = "alive"
        with self.assertRaisesRegex(launcher.LaunchError, "previous run still owns live"), \
                mock.patch.object(launcher, "prepare_instance") as prepare:
            launcher.launch(self.args, processes)
        prepare.assert_not_called()
        self.assertEqual(launcher.read_json(self.root / "run.json"), previous)

    def test_pending_old_and_framework_locks_retained(self):
        _, previous = self.launch()
        runtime = Path(next(r for r in previous["processes"] if r["role"] == "game")["runtime_root"])
        for relative in ("diagnostics/request.lock", "profile/diagnostics/multiplayer/request.lock"):
            path = runtime / relative
            launcher.atomic_json(path, {"unresolved": True})
            processes = mock.Mock()
            processes.inspect.return_value = "exited:0"
            with self.assertRaisesRegex(launcher.LaunchError, "unresolved previous Multiplayer lock"):
                launcher.launch(self.args, processes)
            self.assertTrue(path.exists())
            self.assertEqual(launcher.read_json(self.root / "run.json"), previous)
            path.unlink()

    def test_stop_rejects_wrong_owner_and_profile_before_process_access(self):
        _, manifest = self.launch()
        for mutate in (lambda m: m["owner"].update(kind="foreign"),
                       lambda m: next(r for r in m["processes"] if r["role"] == "game").update(framework_profile=str(self.home))):
            bad = copy.deepcopy(manifest)
            mutate(bad)
            launcher.atomic_json(self.root / "run.json", bad)
            processes = mock.Mock()
            with self.assertRaises(launcher.LaunchError):
                launcher.stop_manifest(self.root, processes)
            processes.inspect.assert_not_called()

    def test_stop_preserves_reused_pid(self):
        self.launch()
        processes = mock.Mock()
        processes.inspect.return_value = "identity-mismatch"
        self.assertEqual(launcher.stop_manifest(self.root, processes), 0)
        self.assertTrue(all(r["stop_state"] == "identity-mismatch" for r in launcher.read_json(self.root / "run.json")["processes"]))

    def test_failed_second_game_retains_cause_cleans_only_popen_handles(self):
        def failed(command, **kwargs):
            process = self.fake_popen(command, **kwargs)
            if len(self.started) == 5:
                process.returncode = 42
            return process
        with self.assertRaisesRegex(launcher.LaunchError, "exited code=42"):
            self.launch(fake=failed)
        manifest = launcher.read_json(self.root / "run.json")
        self.assertEqual(manifest["state"], "failed")
        self.assertIn("exited code=42", manifest["failure"])
        self.assertTrue(all(p.returncode is not None for p, _, _ in self.started))
        self.started[-1][0].kill.assert_not_called()

    def test_occupied_port_rejected_without_foreign_process_action(self):
        with socket.socket() as endpoint:
            endpoint.bind(("127.0.0.1", 0))
            endpoint.listen(1)
            with self.assertRaisesRegex(launcher.LaunchError, "exclusive loopback port preflight failed"):
                launcher.reserve_ports([endpoint.getsockname()[1]])

    def test_cli_requires_candidates_and_rejects_overlapping_ports(self):
        self.assertEqual(launcher.main([]), 2)
        args = ["--candidate-binary", str(self.binary), "--multiplayer-package", str(self.package),
            "--framework-runtime", str(self.framework), "--sidecar", str(self.sidecar), "--stock-steam-sha256", "0" * 64]
        self.assertEqual(launcher.main(args + ["--relay-port", "47001"]), 2)

    def test_optional_profiler_exact_package_and_entry(self):
        package = self.home / "profiler"
        package.mkdir()
        launcher.atomic_json(package / "mod.json", {"id": "doodesch.profiler", "type": "native", "entry": "MT2Profiler.dll"})
        (package / "MT2Profiler.dll").write_bytes(b"profiler")
        self.args.profiler_package = package
        _, manifest = self.launch()
        for record in manifest["processes"]:
            if record["role"] == "game":
                self.assertEqual((Path(record["framework_profile"]) / "mods/doodesch.profiler/MT2Profiler.dll").read_bytes(), b"profiler")
                self.assertEqual(set(record["installed_binary_sha256"]), set(launcher.MODULES))

    def test_second_run_is_fresh_retains_previous_profile_and_manifest(self):
        _, previous = self.launch()
        old_runtime = Path(next(r for r in previous["processes"] if r["role"] == "game")["runtime_root"])
        old_save = old_runtime / "profile/game-data/private.vrz"
        old_save.write_bytes(b"old owned test save")
        old_profiler = old_runtime / "profile/mods/doodesch.profiler/MT2Profiler.dll"
        old_profiler.parent.mkdir()
        old_profiler.write_bytes(b"previous profiler")
        processes = mock.Mock()
        processes.inspect.return_value = "exited:0"
        processes.identity.side_effect = lambda p: {"pid": p.pid, "process_start_time": str(p.pid + 5000),
            "executable_path": next(command[0] for process, command, _ in self.started if process is p)}
        with mock.patch.object(launcher.subprocess, "Popen", side_effect=self.fake_popen), \
                mock.patch.object(launcher, "reserve_ports", return_value=[mock.Mock(), mock.Mock(), mock.Mock()]):
            launcher.launch(self.args, processes)
        current = launcher.read_json(self.root / "run.json")
        self.assertNotEqual(current["run_id"], previous["run_id"])
        self.assertEqual(launcher.read_json(self.root / "runs" / previous["run_id"] / "run.json"), previous)
        self.assertEqual(old_save.read_bytes(), b"old owned test save")
        self.assertTrue(old_profiler.exists())
        for record in current["processes"]:
            if record["role"] == "game":
                self.assertFalse((Path(record["framework_profile"]) / "mods/doodesch.profiler").exists())
                self.assertFalse((Path(record["framework_profile"]) / "game-data/private.vrz").exists())

    def test_profile_reparse_alias_blocks_readiness(self):
        record, status, data = self.status_fixture()
        original = Path.is_junction
        with mock.patch.object(Path, "is_junction", lambda p: p == data.parent or original(p)):
            with self.assertRaisesRegex(launcher.LaunchError, "redirects"):
                launcher.game_readiness(status, record, data)

    def test_missing_multiplayer_status_cannot_use_framework_status(self):
        record, status, _ = self.status_fixture()
        runtime = Path(record["runtime_root"])
        (runtime / "profile/diagnostics/multiplayer/status.json").unlink()
        launcher.atomic_json(runtime / "profile/status.json", status)
        launcher.atomic_json(runtime / "profile/diagnostics/status.json", status)
        with self.assertRaisesRegex(launcher.LaunchError, "game readiness timed out.*status acknowledgement unavailable"), \
                mock.patch.object(launcher.time, "sleep"):
            launcher.wait_games([record], [], 0.001)

    def test_status_read_transient_failure_reports_recovery(self):
        record, status, _ = self.status_fixture()
        with mock.patch.object(launcher, "read_status_json", side_effect=[PermissionError("fixture sharing violation"), status]), \
                mock.patch.object(launcher, "log") as logger, mock.patch.object(launcher.time, "sleep"):
            launcher.wait_games([record], [], 1)
        messages = [str(call) for call in logger.call_args_list]
        self.assertTrue(any("fixture sharing violation" in message and "warn" in message for message in messages))
        self.assertTrue(any("recovered" in message and "failures=1" in message for message in messages))

    def test_installed_candidate_tamper_blocks_game_before_start(self):
        def tampered(command, **kwargs):
            process = self.fake_popen(command, **kwargs)
            if "--mode" in command and "client" in command:
                runtime = Path(kwargs["cwd"]).parent
                (runtime / launcher.MODULES["MT2MultiplayerPlugin.dll"]).write_bytes(b"tampered after preparation")
            return process
        with self.assertRaisesRegex(launcher.LaunchError, "installed binary hash differs file=MT2MultiplayerPlugin.dll"):
            self.launch(fake=tampered)
        self.assertFalse(any(Path(command[0]).name == "MT2.exe" for _, command, _ in self.started))
        self.assertTrue(all(p.returncode is not None for p, _, _ in self.started))

    def test_non_mptest_root_rejected_before_process_or_copy(self):
        self.args.test_root = self.home / "other"
        with self.assertRaisesRegex(launcher.LaunchError, "inside _MPTest"):
            self.launch()
        self.assertFalse(self.started)
        self.assertFalse(self.args.test_root.exists())


    def fixture_receipt(self):
        account = "76561198013473990"
        source = self.home / "_MPTest/retained/host/data" / account / "metagame.vrt"
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_bytes(b"owned prior-test metagame fixture")
        receipt = {"source": str(source.resolve()), "sha256": launcher.sha256(source),
                   "bytes": source.stat().st_size, "account_id": account}
        path = self.home / "metagame.json"
        launcher.atomic_json(path, receipt)
        assets = self.home / "mt2mp_ui"
        assets.mkdir(exist_ok=True)
        for name in launcher.legacy.MOD_UI_ASSETS:
            (assets / name).parent.mkdir(parents=True, exist_ok=True)
            (assets / name).write_bytes(b"fixture UI asset")
        return source, receipt, path

    def test_optional_metagame_copies_before_game_and_records_exact_account_readback(self):
        source, receipt, path = self.fixture_receipt()
        self.args.metagame_fixture = path
        def ready(command, **kwargs):
            runtime = Path(kwargs["cwd"]).parent
            if Path(command[0]).name == "MT2.exe":
                target = runtime / "profile/game-data" / receipt["account_id"] / "metagame.vrt"
                self.assertEqual(target.read_bytes(), source.read_bytes())
                copied = launcher.read_json(runtime / "copy-manifest.json")[launcher.metagame_target(receipt)]
                self.assertEqual(copied, receipt)
            process = self.fake_popen(command, **kwargs)
            if Path(command[0]).name == "MT2.exe":
                status_path = runtime / "profile/diagnostics/multiplayer/status.json"
                status = launcher.read_json(status_path)
                status["write_dir"] = str(runtime / "profile/game-data" / receipt["account_id"])
                launcher.atomic_json(status_path, status)
            return process
        with mock.patch.object(launcher, "ROOT", self.home):
            result, manifest = self.launch(fake=ready)
        self.assertEqual(result, 0)
        self.assertTrue(manifest["metagame_fixture"]["write_dir_verified"])
        self.assertEqual(manifest["metagame_fixture"]["sha256"], receipt["sha256"])
        self.assertEqual(source.read_bytes(), b"owned prior-test metagame fixture")

    def test_optional_metagame_wrong_actual_account_fails_visible_startup(self):
        _, _, path = self.fixture_receipt(); self.args.metagame_fixture = path
        with mock.patch.object(launcher, "ROOT", self.home), self.assertRaisesRegex(launcher.LaunchError, "metagame fixture PHYSFS write_dir"):
            self.launch()
        manifest = launcher.read_json(self.root / "run.json")
        self.assertEqual(manifest["state"], "failed")
        self.assertFalse(manifest["metagame_fixture"]["write_dir_verified"])
        self.assertTrue(all(process.returncode is not None for process, _, _ in self.started))

    def test_default_has_no_fixture_copy_or_manifest_requirement(self):
        self.assertIsNone(self.args.metagame_fixture)
        self.assertIsNone(launcher.load_metagame_fixture(None))
        _, manifest = self.launch()
        self.assertNotIn("metagame_fixture", manifest)
        for record in manifest["processes"]:
            if record["role"] == "game":
                copies = launcher.read_json(Path(record["runtime_root"]) / "copy-manifest.json")
                self.assertFalse(any(name.startswith("profile/game-data/") for name in copies))

    def test_fixture_hash_size_account_and_fields_reject_before_processes(self):
        source, original, path = self.fixture_receipt(); self.args.metagame_fixture = path
        edits = [("sha256", "0" * 64), ("bytes", original["bytes"] + 1), ("bytes", True),
                 ("bytes", 0), ("bytes", 16385), ("account_id", "other"),
                 ("account_id", "9" * 21), ("account_id", "1")]
        for field, value in edits:
            altered = {**original, field: value}; launcher.atomic_json(path, altered)
            with self.subTest(field=field, value=value), mock.patch.object(launcher, "ROOT", self.home), self.assertRaises(launcher.LaunchError):
                self.launch()
            self.assertFalse(self.started)
        launcher.atomic_json(path, {**original, "settings": "not permitted"})
        with mock.patch.object(launcher, "ROOT", self.home), self.assertRaisesRegex(launcher.LaunchError, "exact source"):
            launcher.load_metagame_fixture(path)
        source.write_bytes(b"x" * 16385); altered = {**original, "bytes": 16384}
        launcher.atomic_json(path, altered)
        with mock.patch.object(launcher, "ROOT", self.home), self.assertRaisesRegex(launcher.LaunchError, "bytes/hash differs"):
            launcher.load_metagame_fixture(path)

    def test_fixture_outside_owned_tree_and_wrong_filename_reject(self):
        source, receipt, path = self.fixture_receipt()
        outside = self.home / "private" / receipt["account_id"] / "metagame.vrt"
        outside.parent.mkdir(parents=True); outside.write_bytes(source.read_bytes())
        wrong = source.with_name("settings.vrt"); wrong.write_bytes(source.read_bytes())
        for selected in (outside, wrong):
            launcher.atomic_json(path, {**receipt, "source": str(selected)})
            with self.subTest(path=selected), mock.patch.object(launcher, "ROOT", self.home), self.assertRaises(launcher.LaunchError):
                launcher.load_metagame_fixture(path)

    def test_fixture_source_alias_rejects(self):
        source, receipt, path = self.fixture_receipt()
        alias = source.parent / ".." / receipt["account_id"] / "metagame.vrt"
        launcher.atomic_json(path, {**receipt, "source": str(alias)})
        with mock.patch.object(launcher, "ROOT", self.home), self.assertRaisesRegex(launcher.LaunchError, "unaliased"):
            launcher.load_metagame_fixture(path)
        launcher.atomic_json(path, receipt)
        original = Path.is_symlink
        def linked(selected):
            return selected == source.parent or original(selected)
        with mock.patch.object(launcher, "ROOT", self.home), mock.patch.object(Path, "is_symlink", linked), self.assertRaisesRegex(launcher.LaunchError, "redirect"):
            launcher.load_metagame_fixture(path)

    def test_fixture_copy_rechecks_source_and_never_overwrites_existing_runtime(self):
        source, _, path = self.fixture_receipt()
        with mock.patch.object(launcher, "ROOT", self.home):
            receipt = launcher.load_metagame_fixture(path)
            runtime = self.root / "runs/new/host"
            files = {launcher.metagame_target(receipt): source}
            source.write_bytes(b"changed after receipt")
            with self.assertRaisesRegex(launcher.LaunchError, "bytes/hash differs"):
                launcher.prepare_instance(runtime, self.root, files, receipt)
            target = runtime / launcher.metagame_target(receipt)
            self.assertFalse(target.exists())
            target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(b"keep existing")
            with self.assertRaises(FileExistsError):
                launcher.prepare_instance(runtime, self.root, files, receipt)
            self.assertEqual(target.read_bytes(), b"keep existing")

    def test_fixture_readiness_requires_exact_account_not_another_numeric_child(self):
        record, status, data = self.status_fixture()
        for child in ("76561198013473990", "76561198013473991"):
            (data / child).mkdir()
        status["write_dir"] = str(data / "76561198013473990")
        self.assertIsNone(launcher.game_readiness(status, record, data, "76561198013473990"))
        for actual in (data, data / "76561198013473991"):
            status["write_dir"] = str(actual)
            self.assertIn("metagame fixture PHYSFS", launcher.game_readiness(status, record, data, "76561198013473990"))



if __name__ == "__main__":
    unittest.main()

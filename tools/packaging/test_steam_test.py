"""Exercise actual Steam launch configuration and clean-install copying."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import steam_test as steam
import local_test as local


class SteamTests(unittest.TestCase):
    def test_invalid_native_root_fails_before_copy(self):
        import argparse
        args = argparse.Namespace(test_root=Path(tempfile.gettempdir()) / "wrong-name",
                                  source=Path(tempfile.gettempdir()) / "game-source")
        with patch.object(local, "source_files") as copy:
            with self.assertRaisesRegex(local.LaunchError, "basename _MPTest"):
                steam.launch(args, None)
            copy.assert_not_called()

    def test_menu_bootstrap_does_not_require_remote_lobby(self):
        runtime = Path(tempfile.gettempdir()) / "mt2-steam-test"
        record = {"run_id": "run", "pid": 42}
        status = {"schema_version": 1, "run_id": "run", "pid": 42,
                  "game_thread_ready": True, "game_thread_id": 4,
                  "session_connected": False, "write_dir": str(runtime / "data"),
                  "capabilities": {"read_native_lobby": True}}
        self.assertIsNone(steam.bootstrap_reason(status, record, runtime / "data"))
        self.assertFalse(status["session_connected"])
        status["capabilities"]["read_native_lobby"] = False
        self.assertIn("native lobby", steam.bootstrap_reason(status, record, runtime / "data"))
        status["pid"] = 43
        self.assertIn("run_id/pid", steam.bootstrap_reason(status, record, runtime / "data"))

    def test_environment_selects_owned_steam_profile(self):
        with patch.dict("os.environ", {"MT2MP_TRANSPORT": "loopback"}):
            env = steam.steam_environment(Path("runtime"), "run")
        self.assertEqual(env["MT2MP_TRANSPORT"], "steam")
        self.assertEqual(env["MT2MP_RUN_ID"], "run")
        self.assertEqual(env["MT2MP_PEER_ID"], "1")
        self.assertGreater(int(env["MT2MP_SESSION_ID"]), 0)
        self.assertEqual(env["SteamAppId"], "486860")

    def test_clean_game_dependency_uses_isolated_original_name(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, build, ui = root / "source", root / "build", root / "ui"
            source.mkdir(); build.mkdir(); ui.mkdir()
            (source / "MT2.exe").write_bytes(b"game")
            (source / "steam_api64.dll").write_bytes(b"original Steam")
            (source / "Data").mkdir()
            (source / "Data/asset.dat").write_bytes(b"asset")
            for name in local.OVERLAYS:
                (build / name).write_bytes(name.encode())
            for name in local.MOD_UI_ASSETS:
                path = ui / "mt2mp_ui" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"UI")
            expected = local.sha256(source / "MT2.exe")
            with patch.object(local, "EXPECTED_EXE_SHA256", expected), patch.object(local, "ROOT", ui):
                files = steam.source_files(source, build)
                self.assertEqual(files["steam_api64_original.dll"], source / "steam_api64.dll")
                self.assertEqual(files["steam_api64.dll"], build / "steam_api64.dll")
                self.assertFalse((source / "steam_api64_original.dll").exists())
                self.assertEqual((source / "steam_api64.dll").read_bytes(), b"original Steam")
                (source / "steam_api64.dll").write_bytes((build / "steam_api64.dll").read_bytes())
                with self.assertRaisesRegex(local.LaunchError, "original Steam library unavailable"):
                    steam.source_files(source, build)


if __name__ == "__main__":
    unittest.main()

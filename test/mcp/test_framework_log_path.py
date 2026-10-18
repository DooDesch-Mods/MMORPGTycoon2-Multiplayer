"""Bound log selection for the Framework plugin and unchanged standalone mode."""
from pathlib import Path
import importlib.util
import tempfile
import threading
import unittest
from unittest import mock

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("framework_log_runtime", HERE.parents[1] / "tools/mcp/runtime.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class LogPathTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=HERE)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "_MPTest/host"
        self.plugin_log = self.root / "profile/data/doodesch.multiplayer/mt2mp.log"
        self.legacy_log = self.root / "game/mt2mp.log"
        self.mailbox = self.root / "profile/diagnostics/multiplayer"
        for path, text in ((self.plugin_log, "actual-plugin-data"), (self.legacy_log, "stale-game-log"),
                           (self.mailbox / "runtime.log", "actual-runtime-diagnostic")):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        self.runtime = module.Runtime(self.root.parent)
        self.runtime.bound = {"schema_version": 2, "runtime_kind": "framework-plugin", "runtime_root": str(self.root)}
        # Admission and process ownership already have independent sealed coverage.
        self.runtime._current = mock.Mock(return_value=self.runtime.bound)
        self.runtime._diagnostics = mock.Mock(return_value=self.mailbox)

    def test_plugin_reads_only_exact_private_data_log(self):
        result = self.runtime.read_logs()
        self.assertEqual(result["path"], str(self.plugin_log))
        self.assertEqual(result["text"], "actual-plugin-data")
        self.runtime._current.assert_called_once()

    def test_plugin_missing_log_is_failure_without_legacy_fallback(self):
        self.plugin_log.unlink()
        with self.assertRaises(module.RuntimeErrorDetail) as caught:
            self.runtime.read_logs()
        self.assertEqual(caught.exception.code, "log_read_failed")
        self.assertIsInstance(caught.exception.__cause__, OSError)

    def test_legacy_game_log_path_unchanged(self):
        self.runtime.bound = {"schema_version": 1, "runtime_root": str(self.root)}
        self.assertEqual(self.runtime.read_logs()["text"], "stale-game-log")

    def test_diagnostic_log_path_and_tail_bounds_unchanged(self):
        result = self.runtime.read_logs("runtime.log", 10)
        self.assertEqual(result["path"], str(self.mailbox / "runtime.log"))
        self.assertEqual(result["text"], "diagnostic")
        self.assertTrue(result["truncated"])
        for size in (0, True, 65537):
            with self.assertRaises(module.RuntimeErrorDetail):
                self.runtime.read_logs("mt2mp.log", size)

    def test_arbitrary_paths_and_in_root_reparse_alias_rejected(self):
        with self.assertRaises(module.RuntimeErrorDetail) as caught:
            self.runtime.read_logs("../../secret")
        self.assertEqual(caught.exception.code, "invalid_log")
        with mock.patch.object(Path, "is_junction", lambda p: p == self.plugin_log.parent):
            with self.assertRaises(module.RuntimeErrorDetail) as caught:
                self.runtime.read_logs()
        self.assertEqual(caught.exception.code, "path_escape")

    def test_unknown_mode_cannot_choose_plugin_log(self):
        self.runtime.bound["runtime_kind"] = "unknown"
        with self.assertRaises(module.RuntimeErrorDetail) as caught:
            self.runtime.read_logs()
        self.assertEqual(caught.exception.code, "invalid_manifest")


if __name__ == "__main__":
    unittest.main()

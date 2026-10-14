"""Keep the installed loader out of local proxy runtimes."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("local_test", ROOT / "tools/local_test.py")
launcher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(launcher)


class LoaderSeparationTests(unittest.TestCase):
    def test_local_proxy_keeps_original_steam_and_excludes_dropin_loader(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source, build = base / "source", base / "build"
            source.mkdir()
            build.mkdir()
            for name in ("MT2.exe", "steam_api64_original.dll", "steam_api64.dll", "WSOCK32.dll", "SDL3.dll"):
                (source / name).write_bytes(name.encode())
            (source / "Data").mkdir()
            for name in launcher.OVERLAYS:
                (build / name).write_bytes(b"built:" + name.encode())
            with patch.object(launcher, "EXPECTED_EXE_SHA256", launcher.sha256(source / "MT2.exe")):
                files = launcher.source_files(source, build)
            self.assertFalse(any(Path(name).name.casefold() == "wsock32.dll" for name in files))
            self.assertEqual(files["steam_api64.dll"], build / "steam_api64.dll")
            self.assertEqual(files["steam_api64_original.dll"], source / "steam_api64_original.dll")
            self.assertEqual(files["SDL3.dll"], source / "SDL3.dll")


if __name__ == "__main__":
    unittest.main()

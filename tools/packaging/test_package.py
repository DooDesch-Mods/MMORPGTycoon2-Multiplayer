"""Payload contents, provenance, missing assets and delivery readback."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

MODULE = Path(__file__).resolve().parents[1] / "package.py"
spec = importlib.util.spec_from_file_location("mt2_package", MODULE)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / "build"
        self.build.mkdir()
        (self.root / "CMakeLists.txt").write_text("project(MT2_Multiplayer VERSION 0.1.0)")
        for name in (*package.BINARY_NAMES, "mt2mp_sidecar.exe"):
            (self.build / name).write_bytes(name.encode())
        for name in package.UI_NAMES:
            path = self.root / "mt2mp_ui" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode())
        docs = self.root / "tools/packaging"
        docs.mkdir(parents=True)
        (docs / "PRIVATE_TEST.txt").write_text("private candidate")
        (docs / "FRIEND_TEST.txt").write_text("manual beta instructions")
        for name in ("Python.txt", "PyInstaller.txt"):
            file = docs / "licenses" / name
            file.parent.mkdir(exist_ok=True)
            file.write_text("license " + name)
        for relative in ("LICENSE.md", "THIRD_PARTY_NOTICES.md", "include/vendor/minhook/LICENSE.txt",
                         "include/vendor/imgui/LICENSE.txt", "include/vendor/nlohmann/LICENSE.MIT",
                         "include/vendor/SDL3/LICENSE.txt"):
            file = self.root / relative
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("notice " + relative)
        self.freeze = self.root / "freeze.json"
        self.freeze.write_text(json.dumps({"candidate": "test", "dll_sha256":
            package.digest((self.build / "MT2_Multiplayer.dll").read_bytes())}))

    def test_complete_archive_and_exact_readback(self):
        entries, manifest = package.collect(self.root, self.build, self.freeze, True)
        output = self.root / "candidate.zip"
        package.write_package(entries, output)
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(set(archive.namelist()), set(entries))
            for name, item in manifest["files"].items():
                self.assertEqual(package.digest(archive.read(name)), item["sha256"])
                self.assertEqual(len(archive.read(name)), item["size"])
        self.assertFalse(manifest["ready_for_manual_testing"])
        self.assertNotIn("install.bat", entries)
        self.assertNotIn("steam_api64_original.dll", entries)
        self.assertEqual(len(entries), 16)

    def test_beta_launcher_runtime_and_notices_are_complete(self):
        launcher = self.root / "starter" / "MT2MP-Start.exe"
        launcher.parent.mkdir()
        launcher.write_bytes(b"MZtest executable")
        bundled = launcher.parent / "_internal" / "python313.dll"
        bundled.parent.mkdir()
        bundled.write_bytes(b"test runtime")
        entries, manifest = package.collect(self.root, self.build, self.freeze, True,
                                            launcher, "0.1.0-beta.1")
        self.assertTrue(manifest["ready_for_manual_testing"])
        self.assertFalse(manifest["online_acceptance_passed"])
        self.assertEqual(manifest["version"], "0.1.0-beta.1")
        self.assertEqual(entries["_internal/python313.dll"], b"test runtime")
        for name in ("LICENSE.md", "THIRD_PARTY_NOTICES.md", "licenses/MinHook.txt",
                     "licenses/DearImGui.txt", "licenses/nlohmann-json.txt", "licenses/SDL3.txt",
                     "licenses/Python.txt", "licenses/PyInstaller.txt"):
            self.assertIn(name, entries)
        self.assertNotIn("steam_api64_original.dll", entries)

    def test_invalid_beta_version_fails(self):
        with self.assertRaisesRegex(ValueError, "package version"):
            package.collect(self.root, self.build, self.freeze, release_version="v0.1-beta")

    def test_missing_native_icon_fails(self):
        (self.root / "mt2mp_ui/multiplayer.png").unlink()
        with self.assertRaisesRegex(ValueError, "required regular payload file missing"):
            package.collect(self.root, self.build, self.freeze)

    def test_stale_candidate_fails(self):
        (self.build / "MT2_Multiplayer.dll").write_bytes(b"different build")
        with self.assertRaisesRegex(ValueError, "candidate DLL differs"):
            package.collect(self.root, self.build, self.freeze)

    def test_existing_delivery_is_preserved(self):
        output = self.root / "candidate.zip"
        output.write_bytes(b"previous candidate")
        with self.assertRaises(FileExistsError):
            package.write_package({"file": b"new"}, output)
        self.assertEqual(output.read_bytes(), b"previous candidate")


if __name__ == "__main__":
    unittest.main()

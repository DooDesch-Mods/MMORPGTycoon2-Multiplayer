"""Payload contents, provenance, missing assets and delivery readback."""
import importlib.util
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
import sys
from unittest.mock import patch

MODULE = Path(__file__).resolve().parents[1] / "package.py"
spec = importlib.util.spec_from_file_location("mt2_package", MODULE)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
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


class VortexPackageTests(unittest.TestCase):
    def setUp(self):
        PackageTests.setUp(self)
        self.plugin = self.root / "selected-mod"
        self.plugin.mkdir()
        self.binary = self.build / "MT2MultiplayerPlugin.dll"
        self.binary.write_bytes(b"MZ selected fake native plugin")
        self.mod = {"id": "doodesch.multiplayer", "name": "Multiplayer", "version": "0.1.0-beta.13",
                    "api_version": "1.0.0", "type": "native", "entry": "MT2MultiplayerPlugin.dll",
                    "capabilities": ["ui", "input"], "multiplayer": "shared"}
        self.write_mod()
        self.frozen = {"dll_sha256": package.digest(self.binary.read_bytes()),
                       "files": {"MT2MultiplayerPlugin.dll": {"bytes": self.binary.stat().st_size,
                                 "sha256": package.digest(self.binary.read_bytes())}}}
        self.write_freeze()
        (self.root / "tools/packaging/VORTEX_TEST.md").write_bytes(
            (MODULE.parent / "packaging/VORTEX_TEST.md").read_bytes())
        self.icon = self.root / "assets/ui/desktop_multiplayer/multiplayer.png"
        self.icon.parent.mkdir(parents=True)
        self.icon_bytes = (MODULE.parent.parent / "assets/ui/desktop_multiplayer/multiplayer.png").read_bytes()
        self.icon.write_bytes(self.icon_bytes)

    def write_mod(self):
        (self.plugin / "mod.json").write_text(json.dumps(self.mod), encoding="utf-8")

    def write_freeze(self):
        self.freeze.write_text(json.dumps(self.frozen), encoding="utf-8")

    def collect(self, **changes):
        options = {"mode": "vortex", "multiplayer_package": self.plugin}
        options.update(changes)
        return package.collect(self.root, self.build, self.freeze, **options)

    def test_build_receipt_version_must_match_manifest(self):
        self.frozen['version'] = self.mod['version']
        self.write_freeze()
        self.collect()
        self.mod['version'] = '0.1.0-beta.14'
        self.write_mod()
        with self.assertRaisesRegex(ValueError, 'built plugin version differs'):
            self.collect()

    def test_root_layout_and_exact_archive_readback(self):
        entries, metadata = self.collect()
        expected = {"mod.json", "MT2MultiplayerPlugin.dll", "README.md", "LICENSE.md",
                    "THIRD_PARTY_NOTICES.md", "licenses/MinHook.txt", "licenses/DearImGui.txt",
                    "licenses/nlohmann-json.txt", "licenses/SDL3.txt", "assets/multiplayer.png", "package.json"}
        self.assertEqual(set(entries), expected)
        self.assertEqual(set(metadata["files"]), expected - {"package.json"})
        self.assertEqual(entries["mod.json"], (self.plugin / "mod.json").read_bytes())
        self.assertEqual(entries["assets/multiplayer.png"], self.icon_bytes)
        output = self.root / "multiplayer.zip"
        package.write_package(entries, output)
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(set(archive.namelist()), expected)
            self.assertIsNone(archive.testzip())
            self.assertEqual(archive.read("assets/multiplayer.png"), self.icon_bytes)
            for name, record in metadata["files"].items():
                data = archive.read(name)
                self.assertEqual(len(data), record["size"])
                self.assertEqual(package.digest(data), record["sha256"])

    def test_extras_cannot_add_runtime_or_forbidden_payload(self):
        names = ("MT2.exe", "steam_api64.dll", "WSOCK32.dll", "MT2Framework.dll", "mt2fw.exe",
                 "MT2Profiler.dll", "mt2mp_sidecar.exe", "python313.dll", "MT2_Multiplayer.dll", "mt2fw.json")
        for folder in (self.build, self.plugin):
            for name in names:
                (folder / name).write_bytes(b"excluded fake file")
        entries, _ = self.collect()
        self.assertTrue(set(entries).isdisjoint(names))
        self.assertFalse(any(name.startswith(("mods/", "mt2mp_ui/", "_internal/", "mt2fw/")) for name in entries))

    def test_readiness_dependencies_and_support_text_are_honest(self):
        entries, metadata = self.collect()
        self.assertEqual(metadata["schema_version"], 2)
        self.assertEqual(metadata["stage"], "early-beta")
        self.assertEqual(metadata["validation_scope"], "package-integrity")
        for key in ("ready_for_manual_testing", "delivery_integration_accepted", "manual_test_package"):
            self.assertNotIn(key, metadata)
        self.assertFalse(metadata["online_acceptance_passed"])
        self.assertEqual(metadata["installation"], {"manager": "vortex", "game_id": "mmorpgtycoon2",
            "mod_id": "doodesch.multiplayer", "framework_runtime_required": True, "framework_runtime_included": False,
            "framework_min_version": "0.2.6", "manual_target": "<profile>/mods/doodesch.multiplayer",
            "manual_requires_configured_framework": True, "game_root_extraction_supported": False})
        self.assertNotIn("framework_version", metadata)
        text = entries["README.md"].decode("utf-8")
        support = "> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer)."
        self.assertEqual(text.count(support), 1)
        self.assertIn("Enable Multiplayer and deploy", text)
        self.assertIn("Manual installation", text)
        self.assertIn("Extracting it beside MT2.exe does not install", text)
        self.assertIn("mt2fw.json", text)
        self.assertIn("absolute profile path", text)
        self.assertIn("<profile>/mods/doodesch.multiplayer/mod.json", text)
        self.assertIn("<profile>/game-data", text)
        self.assertIn("originals remain untouched", text)
        self.assertNotIn('game_data', text)

    def test_shipped_readme_uses_selected_version_and_minimum_runtime(self):
        for version in ("0.1.0-beta.13", "0.1.0-beta.19", "0.2.0"):
            with self.subTest(version=version):
                self.mod["version"] = version
                self.write_mod()
                entries, metadata = self.collect()
                text = entries["README.md"].decode("utf-8")
                self.assertEqual(text.splitlines()[0], "# MMORPG Tycoon 2 Multiplayer " + version)
                self.assertIn("Both players need Multiplayer " + version, text)
                self.assertIn("MT2 Mod Framework Runtime " + metadata["installation"]["framework_min_version"] + " or newer", text)
                self.assertNotIn("{{", text)
                self.assertNotIn("Early Beta 17", text)
                self.assertNotIn("0.2.5", text)

    def test_unversioned_readme_template_is_rejected(self):
        template = self.root / "tools/packaging/VORTEX_TEST.md"
        template.write_text("stale hardcoded beta instructions", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "README template lacks required token"):
            self.collect()

    def test_requires_explicit_manifest_directory(self):
        with self.assertRaisesRegex(ValueError, "explicit --multiplayer-package"):
            self.collect(multiplayer_package=None)

    def test_missing_and_empty_required_inputs_rejected(self):
        for file in (self.binary, self.plugin / "mod.json", self.freeze,
                     self.root / "include/vendor/nlohmann/LICENSE.MIT", self.icon):
            content = file.read_bytes()
            for empty in (False, True):
                with self.subTest(file=file.name, empty=empty):
                    if empty:
                        file.write_bytes(b"")
                    else:
                        file.unlink()
                    with self.assertRaisesRegex(ValueError, "missing|empty"):
                        self.collect()
                    file.write_bytes(content)

    def test_wrong_frozen_plugin_hash_size_and_top_level_pin_rejected(self):
        original = self.binary.read_bytes()
        self.binary.write_bytes(original + b"changed")
        with self.assertRaisesRegex(ValueError, "exact frozen hash or size"):
            self.collect()
        self.binary.write_bytes(original)
        self.frozen["dll_sha256"] = "0" * 64
        self.write_freeze()
        with self.assertRaisesRegex(ValueError, "exact frozen hash or size"):
            self.collect()

    def test_malformed_or_absent_plugin_pin_rejected(self):
        for record in (None, {}, {"bytes": True, "sha256": "0" * 64},
                       {"bytes": 1, "sha256": "not-a-hash"}, {"bytes": 0, "sha256": "0" * 64}):
            with self.subTest(record=record):
                self.frozen["files"]["MT2MultiplayerPlugin.dll"] = record
                self.write_freeze()
                with self.assertRaisesRegex(ValueError, "exact plugin file pin"):
                    self.collect()

    def test_exact_native_manifest_and_confined_entry_rejected(self):
        for key, bad in (("id", "other.plugin"), ("api_version", "2.0.0"), ("type", "lua"),
                         ("entry", "../MT2MultiplayerPlugin.dll"), ("entry", "C:/foreign.dll"),
                         ("multiplayer", "local"), ("capabilities", ["ui"]), ("name", "")):
            original = self.mod[key]
            with self.subTest(key=key, value=bad):
                self.mod[key] = bad
                self.write_mod()
                with self.assertRaisesRegex(ValueError, "manifest differs"):
                    self.collect()
                self.mod[key] = original

    def test_version_comes_from_manifest_and_override_must_match(self):
        self.assertEqual(self.collect()[1]["version"], self.mod["version"])
        self.assertEqual(self.collect(release_version=self.mod["version"])[1]["version"], self.mod["version"])
        with self.assertRaisesRegex(ValueError, "version differs"):
            self.collect(release_version="0.1.0-beta.14")
        self.mod["version"] = "v0.1.0"
        self.write_mod()
        with self.assertRaisesRegex(ValueError, "manifest version"):
            self.collect()

    def test_input_aliases_and_parent_traversal_rejected(self):
        with patch.object(Path, "is_symlink", return_value=True):
            with self.assertRaisesRegex(ValueError, "linked package input"):
                self.collect()
        with self.assertRaisesRegex(ValueError, "unsafe package input path"):
            self.collect(multiplayer_package=self.plugin / ".." / "selected-mod")

    def test_launcher_sidecar_and_wrong_mode_inputs_rejected(self):
        for kwargs in ({"with_sidecar": True}, {"launcher": self.root / "starter.exe"}):
            with self.subTest(kwargs=kwargs), self.assertRaisesRegex(ValueError, "launcher or sidecar"):
                self.collect(**kwargs)
        with self.assertRaisesRegex(ValueError, "requires vortex mode"):
            package.collect(self.root, self.build, self.freeze, multiplayer_package=self.plugin)

    def test_unsafe_archive_paths_and_case_collisions_rejected(self):
        output = self.root / "unsafe.zip"
        for name in ("../escape", "/absolute", "C:/absolute", "a\\b", "a/../b", "a//b", "a/./b",
                     "a.", "a ", "con", "dir/NUL.txt", "what?.txt", "line\nfeed", 7):
            with self.subTest(name=name), self.assertRaises(ValueError):
                package.write_package({name: b"test"}, output)
        with self.assertRaisesRegex(ValueError, "duplicate archive path"):
            package.write_package({"File": b"a", "file": b"b"}, output)
        self.assertFalse(output.exists())

    def test_cli_requires_explicit_binary_and_emits_selected_vortex_package(self):
        args = ["package.py", "--mode", "vortex", "--freeze", str(self.freeze),
                "--multiplayer-package", str(self.plugin), "--output", str(self.root / "artifacts/tester.zip")]
        errors, output = io.StringIO(), io.StringIO()
        with patch.object(package, "ROOT", self.root), patch.object(sys, "argv", args), contextlib.redirect_stderr(errors):
            self.assertEqual(package.main(), 1)
        self.assertIn("explicit --build", errors.getvalue())
        self.assertFalse((self.root / "artifacts/tester.zip").exists())
        with patch.object(package, "ROOT", self.root), patch.object(sys, "argv", args + ["--build", str(self.build)]), contextlib.redirect_stdout(output):
            self.assertEqual(package.main(), 0)
        result = json.loads(output.getvalue())
        self.assertEqual(result["status"], "payload-verified")
        self.assertEqual(result["package_mode"], "vortex")
        self.assertNotIn("ready_for_manual_testing", result)
        with zipfile.ZipFile(self.root / "artifacts/tester.zip") as archive:
            self.assertIn("mod.json", archive.namelist())

    def test_dropin_mode_regression(self):
        (self.build / "WSOCK32.dll").write_bytes(b"old loader")
        self.freeze.write_text(json.dumps({"candidate": "old-dropin", "dll_sha256":
            package.digest((self.build / "MT2_Multiplayer.dll").read_bytes()),
            "loader_sha256": package.digest(b"old loader")}))
        (self.root / "tools/packaging/DROPIN_TEST.txt").write_text("drop-in instructions")
        entries, metadata = package.collect(self.root, self.build, self.freeze, mode="dropin")
        self.assertIn("WSOCK32.dll", entries)
        self.assertIn("MT2_Multiplayer.dll", entries)
        self.assertIn("mt2mp_ui/lobby.win", entries)
        self.assertNotIn("steam_api64.dll", entries)
        self.assertNotIn("MT2MultiplayerPlugin.dll", entries)
        self.assertTrue(metadata["ready_for_manual_testing"])

    def test_no_private_input_paths_in_output_metadata(self):
        self.frozen.update(candidate="F:/private/developer/build", source="F:/private/source")
        self.write_freeze()
        text = self.collect()[0]["package.json"].decode("utf-8")
        self.assertNotIn("F:/private", text)
        self.assertEqual(json.loads(text)["inputs"]["candidate_receipt_sha256"], package.digest(self.freeze.read_bytes()))

    def test_json_roots_must_be_objects(self):
        original = self.freeze.read_bytes()
        self.freeze.write_text("[]", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "receipt must be a JSON object"):
            self.collect()
        self.freeze.write_bytes(original)
        (self.plugin / "mod.json").write_text("[]", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "manifest differs"):
            self.collect()


if __name__ == "__main__":
    unittest.main()

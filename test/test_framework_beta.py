"""Exercise the actual Vortex packager and the public release validation boundary."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / ".github/scripts"))
import framework_beta
import build_beta

COMMIT = "a" * 40
TAG = "v0.1.0-beta.13"


class FrameworkReleaseTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        paths = ["tools/package.py", ".github/framework-sdk.json", "mod.json",
                 "assets/ui/desktop_multiplayer/multiplayer.png",
                 "LICENSE.md", "THIRD_PARTY_NOTICES.md", "tools/packaging/VORTEX_TEST.md",
                 "include/vendor/minhook/LICENSE.txt", "include/vendor/imgui/LICENSE.txt",
                 "include/vendor/nlohmann/LICENSE.MIT", "include/vendor/SDL3/LICENSE.txt"]
        for name in paths:
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        self.mod = json.loads((self.root / "mod.json").read_text(encoding="utf-8"))
        self.mod["version"] = TAG[1:]
        (self.root / "mod.json").write_text(json.dumps(self.mod), encoding="utf-8")
        self.pin = json.loads((self.root / ".github/framework-sdk.json").read_text(encoding="utf-8"))
        self.build = self.root / "build"
        self.build.mkdir()
        self.binary = self.build / "MT2MultiplayerPlugin.dll"
        self.binary.write_bytes(b"authored synthetic native plugin")
        self.receipt = {"version": TAG[1:], "source_commit": COMMIT, "sdk_commit": self.pin["revision"],
                        "dll_sha256": hashlib.sha256(self.binary.read_bytes()).hexdigest(),
                        "files": {self.binary.name: {"bytes": self.binary.stat().st_size,
                                  "sha256": hashlib.sha256(self.binary.read_bytes()).hexdigest()}}}
        self.save_receipt()
        self.output = self.root / "output"
        self.archive = self.output / ("MMORPGTycoon2-Multiplayer-" + TAG + "-windows-x64.zip")

    def save_receipt(self):
        (self.build / "framework-build.json").write_text(json.dumps(self.receipt), encoding="utf-8")

    def package(self, tag=TAG):
        with patch.object(framework_beta, "source_commit", return_value=COMMIT):
            framework_beta.package(self.root, self.build, tag, COMMIT, self.output)

    def rewrite(self, edit):
        with zipfile.ZipFile(self.archive) as zipped:
            entries = {name: zipped.read(name) for name in zipped.namelist()}
        edit(entries)
        with zipfile.ZipFile(self.archive, "w") as zipped:
            for name, data in entries.items():
                zipped.writestr(name, data)

    def test_exact_framework_package_and_existing_publisher_validator(self):
        self.package()
        manifest = build_beta.validate(self.archive, TAG + "@" + COMMIT)
        self.assertEqual(manifest["package_mode"], "vortex")
        self.assertEqual(manifest["sdk_commit"], self.pin["revision"])
        with zipfile.ZipFile(self.archive) as zipped:
            self.assertEqual(set(zipped.namelist()), framework_beta.PAYLOAD | {"package.json"})
        checksum = self.archive.with_suffix(".zip.sha256").read_text(encoding="ascii")
        self.assertEqual(checksum, hashlib.sha256(self.archive.read_bytes()).hexdigest() + "  " + self.archive.name + "\n")

    def test_changed_binary_is_rejected_before_output(self):
        self.binary.write_bytes(b"different binary")
        with self.assertRaisesRegex(ValueError, "frozen hash or size"):
            self.package()
        self.assertFalse(self.archive.exists())

    def test_stale_source_sdk_or_version_receipt_is_rejected(self):
        for key, value in [("source_commit", "b" * 40), ("sdk_commit", "c" * 40), ("version", "0.1.0-beta.2")]:
            with self.subTest(key=key):
                original = self.receipt[key]
                self.receipt[key] = value
                self.save_receipt()
                with self.assertRaisesRegex(ValueError, "build identity differs"):
                    self.package()
                self.assertFalse(self.archive.exists())
                self.receipt[key] = original

    def test_tag_must_match_compiled_mod_version(self):
        with self.assertRaisesRegex(ValueError, "tag differs"):
            self.package("v0.1.0-beta.14")
        self.assertFalse(self.archive.exists())

    def test_private_game_and_legacy_payloads_are_rejected(self):
        self.package()
        original = self.archive.read_bytes()
        for name in ["MT2.exe", "steam_api64.dll", "WSOCK32.dll", "save.vrz", "../private.txt"]:
            with self.subTest(name=name):
                self.archive.write_bytes(original)
                self.rewrite(lambda entries: entries.update({name: b"must not ship"}))
                with self.assertRaisesRegex(ValueError, "payload"):
                    framework_beta.validate(self.archive, TAG + "@" + COMMIT)

    def test_modified_archive_content_is_rejected(self):
        self.package()
        self.rewrite(lambda entries: entries.update({"MT2MultiplayerPlugin.dll": b"changed"}))
        with self.assertRaisesRegex(ValueError, "content differs"):
            framework_beta.validate(self.archive, TAG + "@" + COMMIT)

    def test_other_package_mode_cannot_accept_framework_payload(self):
        self.package()
        with self.assertRaisesRegex(ValueError, "mode differs"):
            build_beta.validate(self.archive, TAG + "@" + COMMIT, "dropin")

    def test_modified_sdk_is_rejected_before_cmake(self):
        sdk = self.root / "sdk"
        header = sdk / next(iter(self.pin["headers"]))
        header.parent.mkdir(parents=True)
        header.write_bytes(b"changed SDK")
        with patch.object(framework_beta, "source_commit", return_value=COMMIT), patch.object(framework_beta.subprocess, "run") as command:
            with self.assertRaisesRegex(ValueError, "SDK header differs"):
                framework_beta.build(self.root, sdk, COMMIT)
        command.assert_not_called()


if __name__ == "__main__":
    unittest.main()

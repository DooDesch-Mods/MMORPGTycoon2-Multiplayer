#!/usr/bin/env python3
"""Build a private payload of the current candidate, without legacy installers."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BINARY_NAMES = ("MT2_Multiplayer.dll", "steam_api64.dll")
UI_NAMES = ("entry.win", "lobby.win", "browser.win", "multiplayer.png",
            "materials/mt2mp_desktop_multiplayer.mat")
GAME_SHA256 = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def collect(root, build, freeze, with_sidecar=False, launcher=None, release_version=None):
    version = re.search(r"project\(MT2_Multiplayer VERSION (\d+\.\d+\.\d+)",
                        (root / "CMakeLists.txt").read_text(encoding="utf-8"))
    if not version:
        raise ValueError("CMake project version missing")
    full_version = release_version or version.group(1)
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-beta\.[1-9]\d*)?", full_version):
        raise ValueError("package version must be X.Y.Z or X.Y.Z-beta.N")
    pin = json.loads(freeze.read_text(encoding="utf-8"))
    entries = {}
    binaries = (*BINARY_NAMES, "mt2mp_sidecar.exe") if with_sidecar else BINARY_NAMES
    sources = {name: build / name for name in binaries}
    sources.update({"mt2mp_ui/" + name: root / "mt2mp_ui" / name for name in UI_NAMES})
    for name, source in sources.items():
        if source.is_symlink() or not source.is_file():
            raise ValueError(f"required regular payload file missing: {source}")
        data = source.read_bytes()
        if not data:
            raise ValueError(f"empty payload file: {source}")
        entries[name] = data
    actual = digest(entries["MT2_Multiplayer.dll"])
    if actual != pin["dll_sha256"]:
        raise ValueError(f"candidate DLL differs: actual={actual} required={pin['dll_sha256']}")
    if launcher is not None:
        entries["MT2MP-Start.exe"] = launcher.read_bytes()
        if not entries["MT2MP-Start.exe"].startswith(b"MZ"):
            raise ValueError("portable launcher is not a Windows executable")
        bundled = launcher.parent / "_internal"
        if bundled.is_dir():
            for source in sorted(bundled.rglob("*")):
                if source.is_symlink():
                    raise ValueError(f"linked runtime file rejected: {source}")
                if source.is_file():
                    entries[source.relative_to(launcher.parent).as_posix()] = source.read_bytes()
        for name in ("Python.txt", "PyInstaller.txt"):
            entries["licenses/" + name] = (root / "tools/packaging/licenses" / name).read_bytes()
    notices = {
        "LICENSE.md": root / "LICENSE.md",
        "THIRD_PARTY_NOTICES.md": root / "THIRD_PARTY_NOTICES.md",
        "licenses/MinHook.txt": root / "include/vendor/minhook/LICENSE.txt",
        "licenses/DearImGui.txt": root / "include/vendor/imgui/LICENSE.txt",
        "licenses/nlohmann-json.txt": root / "include/vendor/nlohmann/LICENSE.MIT",
        "licenses/SDL3.txt": root / "include/vendor/SDL3/LICENSE.txt",
    }
    for name, source in notices.items():
        entries[name] = source.read_bytes()
    readme = "FRIEND_TEST.txt" if launcher is not None else "PRIVATE_TEST.txt"
    entries["README.txt"] = (root / "tools/packaging" / readme).read_bytes()
    manifest = {
        "schema_version": 1, "version": full_version,
        "source_commit": pin.get("source_commit", ""),
        "candidate": pin["candidate"], "stage": "manual-online-test" if launcher else "payload-only",
        "manual_test_package": launcher is not None, "online_acceptance_passed": False,
        "ready_for_manual_testing": launcher is not None,
        "required_game_exe_sha256": GAME_SHA256,
        "files": {name: {"size": len(data), "sha256": digest(data)}
                  for name, data in entries.items()},
    }
    entries["package.json"] = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
    return entries, manifest


def write_package(entries, output):
    output.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation preserves previous delivered candidates.
    with output.open("xb") as stream:
        with zipfile.ZipFile(stream, "w", zipfile.ZIP_DEFLATED) as archive:
            for name, data in entries.items():
                archive.writestr(name, data)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise ValueError(f"ZIP integrity check failed: {output}")
        for name, expected in entries.items():
            if archive.read(name) != expected:
                raise ValueError(f"ZIP readback differs: {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freeze", required=True, type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "build/bin/Release")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--with-sidecar", action="store_true")
    parser.add_argument("--launcher", type=Path)
    parser.add_argument("--version")
    args = parser.parse_args()
    try:
        output = args.output.resolve()
        if not output.is_relative_to(ROOT / "artifacts"):
            raise ValueError("private packages must be written under project artifacts/")
        entries, manifest = collect(ROOT, args.build, args.freeze, args.with_sidecar, args.launcher, args.version)
        write_package(entries, output)
        print(json.dumps({"status": "payload-verified", "output": str(output),
                          "files": len(entries), "ready_for_manual_testing": manifest["ready_for_manual_testing"]}))
        return 0
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print(f"error: package failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

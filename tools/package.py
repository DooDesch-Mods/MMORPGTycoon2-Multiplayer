#!/usr/bin/env python3
"""Build a private payload of the current candidate, without legacy installers."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BINARY_NAMES = ("MT2_Multiplayer.dll", "steam_api64.dll")
UI_NAMES = ("entry.win", "lobby.win", "browser.win", "multiplayer.png",
            "materials/mt2mp_desktop_multiplayer.mat")
GAME_SHA256 = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"
FRAMEWORK_MIN_VERSION = "0.2.6"


def vortex_readme(root, version):
    text = regular_bytes(root / "tools/packaging/VORTEX_TEST.md").decode("utf-8")
    for token, value in (("{{MULTIPLAYER_VERSION}}", version),
                         ("{{FRAMEWORK_MIN_VERSION}}", FRAMEWORK_MIN_VERSION)):
        if token not in text:
            raise ValueError("Vortex README template lacks required token: " + token)
        text = text.replace(token, value)
    return text.encode("utf-8")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def regular_bytes(path):
    if ".." in Path(path).parts:
        raise ValueError(f"unsafe package input path: {path}")
    path = Path(path).absolute()
    for part in (path, *path.parents):
        if part.is_symlink() or (hasattr(part, "is_junction") and part.is_junction()):
            raise ValueError(f"linked package input rejected: {path}")
    if not path.is_file():
        raise ValueError(f"required regular payload file missing: {path}")
    data = path.read_bytes()
    if not data:
        raise ValueError(f"empty payload file: {path}")
    return data


def collect_vortex(root, build, freeze, multiplayer_package, release_version):
    if multiplayer_package is None:
        raise ValueError("vortex mode requires explicit --multiplayer-package")
    frozen_bytes = regular_bytes(freeze)
    frozen = json.loads(frozen_bytes)
    if not isinstance(frozen, dict):
        raise ValueError("candidate receipt must be a JSON object")
    files = frozen.get("files")
    record = files.get("MT2MultiplayerPlugin.dll") if isinstance(files, dict) else None
    if (not isinstance(record, dict) or type(record.get("bytes")) is not int or record["bytes"] <= 0
            or not isinstance(record.get("sha256"), str) or not re.fullmatch(r"[0-9a-f]{64}", record["sha256"])):
        raise ValueError("candidate receipt lacks exact plugin file pin")
    binary = regular_bytes(build / "MT2MultiplayerPlugin.dll")
    if (len(binary) != record["bytes"] or digest(binary) != record["sha256"]
            or frozen.get("dll_sha256") != digest(binary)):
        raise ValueError("candidate plugin differs from exact frozen hash or size")
    manifest_bytes = regular_bytes(Path(multiplayer_package) / "mod.json")
    mod = json.loads(manifest_bytes)
    required = {"id": "doodesch.multiplayer", "api_version": "1.0.0", "type": "native",
                "entry": "MT2MultiplayerPlugin.dll", "multiplayer": "shared"}
    if (not isinstance(mod, dict) or any(mod.get(key) != value for key, value in required.items())
            or not isinstance(mod.get("name"), str) or not mod["name"].strip()
            or mod.get("capabilities") != ["ui", "input"]
            or set(mod) != {*required, "name", "version", "capabilities"}):
        raise ValueError("Multiplayer manifest differs from supported native package contract")
    version = mod.get("version")
    if not isinstance(version, str) or not re.fullmatch(r"\d+\.\d+\.\d+(?:-beta\.[1-9]\d*)?", version):
        raise ValueError("Multiplayer manifest version must be X.Y.Z or X.Y.Z-beta.N")
    if release_version is not None and release_version != version:
        raise ValueError("package version differs from Multiplayer manifest")
    if frozen.get("version") is not None and frozen["version"] != version:
        raise ValueError("built plugin version differs from Multiplayer manifest")
    entries = {"MT2MultiplayerPlugin.dll": binary, "mod.json": manifest_bytes,
               "assets/multiplayer.png": regular_bytes(root / "assets/ui/desktop_multiplayer/multiplayer.png")}
    notices = {
        "LICENSE.md": root / "LICENSE.md",
        "THIRD_PARTY_NOTICES.md": root / "THIRD_PARTY_NOTICES.md",
        "licenses/MinHook.txt": root / "include/vendor/minhook/LICENSE.txt",
        "licenses/DearImGui.txt": root / "include/vendor/imgui/LICENSE.txt",
        "licenses/nlohmann-json.txt": root / "include/vendor/nlohmann/LICENSE.MIT",
        "licenses/SDL3.txt": root / "include/vendor/SDL3/LICENSE.txt",
    }
    entries.update({name: regular_bytes(source) for name, source in notices.items()})
    entries["README.md"] = vortex_readme(root, version)
    metadata = {
        "schema_version": 2, "version": version, "candidate": "framework-native-plugin",
        "package_mode": "vortex", "stage": "early-beta",
        "validation_scope": "package-integrity", "online_acceptance_passed": False,
        "required_game_exe_sha256": GAME_SHA256,
        "installation": {"manager": "vortex", "game_id": "mmorpgtycoon2", "mod_id": mod["id"],
                         "framework_runtime_required": True, "framework_runtime_included": False,
                         "framework_min_version": FRAMEWORK_MIN_VERSION,
                         "manual_target": "<profile>/mods/doodesch.multiplayer",
                         "manual_requires_configured_framework": True,
                         "game_root_extraction_supported": False},
        "inputs": {"candidate_receipt_sha256": digest(frozen_bytes), "mod_manifest_sha256": digest(manifest_bytes)},
        "files": {name: {"size": len(data), "sha256": digest(data)} for name, data in entries.items()},
    }
    entries["package.json"] = (json.dumps(metadata, indent=2) + "\n").encode("utf-8")
    return entries, metadata


def collect(root, build, freeze, with_sidecar=False, launcher=None, release_version=None, mode="legacy",
            multiplayer_package=None):
    if mode == "vortex":
        if with_sidecar or launcher is not None:
            raise ValueError("vortex plugin package cannot include launcher or sidecar")
        return collect_vortex(root, build, freeze, multiplayer_package, release_version)
    if multiplayer_package is not None:
        raise ValueError("Multiplayer package input requires vortex mode")
    version = re.search(r"project\(MT2_Multiplayer VERSION (\d+\.\d+\.\d+)",
                        (root / "CMakeLists.txt").read_text(encoding="utf-8"))
    if not version:
        raise ValueError("CMake project version missing")
    full_version = release_version or version.group(1)
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-beta\.[1-9]\d*)?", full_version):
        raise ValueError("package version must be X.Y.Z or X.Y.Z-beta.N")
    pin = json.loads(freeze.read_text(encoding="utf-8"))
    if mode not in {"legacy", "dropin"}:
        raise ValueError("unknown package mode")
    if mode == "dropin" and (with_sidecar or launcher is not None):
        raise ValueError("drop-in package cannot include launcher or sidecar")
    entries = {}
    binaries = ("MT2_Multiplayer.dll", "WSOCK32.dll") if mode == "dropin" else ((*BINARY_NAMES, "mt2mp_sidecar.exe") if with_sidecar else BINARY_NAMES)
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
    if mode == "dropin" and digest(entries["WSOCK32.dll"]) != pin.get("loader_sha256"):
        raise ValueError("candidate WSOCK loader differs from freeze")
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
    readme = "DROPIN_TEST.txt" if mode == "dropin" else ("FRIEND_TEST.txt" if launcher is not None else "PRIVATE_TEST.txt")
    entries["README.txt"] = (root / "tools/packaging" / readme).read_bytes()
    manifest = {
        "schema_version": 1, "version": full_version,
        "source_commit": pin.get("source_commit", ""),
        "candidate": pin["candidate"], "package_mode": mode, "stage": "manual-online-test" if launcher or mode == "dropin" else "payload-only",
        "manual_test_package": launcher is not None or mode == "dropin", "online_acceptance_passed": False,
        "ready_for_manual_testing": launcher is not None or mode == "dropin",
        "required_game_exe_sha256": GAME_SHA256,
        "files": {name: {"size": len(data), "sha256": digest(data)}
                  for name, data in entries.items()},
    }
    entries["package.json"] = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
    return entries, manifest


def write_package(entries, output):
    folded = set()
    devices = {"con", "prn", "aux", "nul", *(f"{prefix}{index}" for prefix in ("com", "lpt") for index in range(1, 10))}
    for name in entries:
        if not isinstance(name, str):
            raise ValueError(f"unsafe archive path type: {type(name).__name__}")
        if (not name or "\\" in name or ":" in name or PurePosixPath(name).is_absolute()
                or PureWindowsPath(name).drive
                or any(part in {"", ".", ".."} or part.endswith((".", " ")) for part in name.split("/"))
                or any(part.split(".")[0].casefold() in devices for part in name.split("/"))
                or any(ord(char) < 32 or char in '<>"?*|' for char in name)
                or name.casefold() in folded):
            raise ValueError(f"unsafe or duplicate archive path: {name}")
        folded.add(name.casefold())
    output.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation preserves previous delivered candidates.
    with output.open("xb") as stream:
        with zipfile.ZipFile(stream, "w", zipfile.ZIP_DEFLATED) as archive:
            for name, data in entries.items():
                archive.writestr(name, data)
    with zipfile.ZipFile(output) as archive:
        if len(archive.namelist()) != len(entries) or set(archive.namelist()) != set(entries):
            raise ValueError(f"ZIP file list differs: {output}")
        if archive.testzip() is not None:
            raise ValueError(f"ZIP integrity check failed: {output}")
        for name, expected in entries.items():
            if archive.read(name) != expected:
                raise ValueError(f"ZIP readback differs: {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freeze", required=True, type=Path)
    parser.add_argument("--build", type=Path, help="Explicit artifact directory; legacy modes default to build/bin/Release")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--with-sidecar", action="store_true")
    parser.add_argument("--launcher", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--mode", choices=("legacy", "dropin", "vortex"), default="legacy")
    parser.add_argument("--multiplayer-package", type=Path, help="Exact selected Multiplayer mod.json directory")
    args = parser.parse_args()
    try:
        output = args.output.resolve()
        if not output.is_relative_to(ROOT / "artifacts"):
            raise ValueError("private packages must be written under project artifacts/")
        if args.mode == "vortex" and args.build is None:
            raise ValueError("vortex mode requires an explicit --build artifact directory")
        entries, manifest = collect(ROOT, args.build or ROOT / "build/bin/Release", args.freeze,
                                    args.with_sidecar, args.launcher, args.version, args.mode, args.multiplayer_package)
        write_package(entries, output)
        result = {"status": "payload-verified", "output": str(output), "files": len(entries),
                  "package_mode": manifest["package_mode"]}
        if "ready_for_manual_testing" in manifest:
            result["ready_for_manual_testing"] = manifest["ready_for_manual_testing"]
        print(json.dumps(result))
        return 0
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print(f"error: package failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

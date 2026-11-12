#!/usr/bin/env python3
"""Build the Vortex package of the checked Framework plugin build."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
GAME_SHA256 = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"


def framework_min_version(root):
    """The pinned Framework tag without its "v" is the minimum runtime the plugin accepts."""
    tag = json.loads(regular_bytes(root / ".github/framework-sdk.json"))["tag"]
    if not re.fullmatch(r"v\d+\.\d+\.\d+", tag):
        raise ValueError("Framework pin must be a tag vMAJOR.MINOR.PATCH: " + tag)
    return tag[1:]


def vortex_readme(root, version, framework_version):
    text = regular_bytes(root / "tools/packaging/README.template.md").decode("utf-8")
    for token, value in (("{{MULTIPLAYER_VERSION}}", version),
                         ("{{FRAMEWORK_MIN_VERSION}}", framework_version)):
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


def collect(root, build, freeze, multiplayer_package=None, release_version=None, framework_sdk=None):
    if multiplayer_package is None:
        raise ValueError("vortex mode requires explicit --multiplayer-package")
    if framework_sdk is None:
        raise ValueError("license texts require explicit --framework-sdk")
    framework_version = framework_min_version(root)
    vendor = Path(framework_sdk) / "third_party/vendor"
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
        "licenses/MinHook.txt": vendor / "minhook/LICENSE.txt",
        "licenses/nlohmann-json.txt": vendor / "nlohmann/LICENSE.MIT",
        "licenses/SDL3.txt": vendor / "SDL3/LICENSE.txt",
    }
    entries.update({name: regular_bytes(source) for name, source in notices.items()})
    entries["README.md"] = vortex_readme(root, version, framework_version)
    metadata = {
        "schema_version": 2, "version": version, "candidate": "framework-native-plugin",
        "package_mode": "vortex", "stage": "early-beta",
        "validation_scope": "package-integrity", "online_acceptance_passed": False,
        "required_game_exe_sha256": GAME_SHA256,
        "installation": {"manager": "vortex", "game_id": "mmorpgtycoon2", "mod_id": mod["id"],
                         "framework_runtime_required": True, "framework_runtime_included": False,
                         "framework_min_version": framework_version,
                         "manual_target": "<profile>/mods/doodesch.multiplayer",
                         "manual_requires_configured_framework": True,
                         "game_root_extraction_supported": False},
        "inputs": {"candidate_receipt_sha256": digest(frozen_bytes), "mod_manifest_sha256": digest(manifest_bytes)},
        "files": {name: {"size": len(data), "sha256": digest(data)} for name, data in entries.items()},
    }
    entries["package.json"] = (json.dumps(metadata, indent=2) + "\n").encode("utf-8")
    return entries, metadata


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
    parser.add_argument("--build", required=True, type=Path, help="Directory with the built MT2MultiplayerPlugin.dll")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version")
    parser.add_argument("--multiplayer-package", required=True, type=Path, help="Exact selected Multiplayer mod.json directory")
    parser.add_argument("--framework-sdk", required=True, type=Path, help="Framework checkout at the pinned tag (license texts)")
    args = parser.parse_args()
    try:
        output = args.output.resolve()
        if not output.is_relative_to(ROOT / "artifacts"):
            raise ValueError("private packages must be written under project artifacts/")
        entries, manifest = collect(ROOT, args.build, args.freeze, args.multiplayer_package, args.version, args.framework_sdk)
        write_package(entries, output)
        print(json.dumps({"status": "payload-verified", "output": str(output), "files": len(entries),
                          "package_mode": manifest["package_mode"]}))
        return 0
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print(f"error: package failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

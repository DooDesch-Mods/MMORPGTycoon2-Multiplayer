"""Build and package the native Framework plugin with an exact source and SDK receipt."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PAYLOAD = {"MT2MultiplayerPlugin.dll", "mod.json", "assets/multiplayer.png", "README.md", "LICENSE.md",
           "THIRD_PARTY_NOTICES.md", "licenses/MinHook.txt", "licenses/DearImGui.txt",
           "licenses/nlohmann-json.txt", "licenses/SDL3.txt"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def source_commit(root):
    return subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], check=True,
                          capture_output=True, text=True).stdout.strip()


def identity(root, commit):
    if not re.fullmatch(r"[0-9a-f]{40}", commit) or source_commit(root) != commit:
        raise ValueError("build source commit differs from checked-out source")
    mod = json.loads((root / "mod.json").read_text(encoding="utf-8"))
    version = mod["version"]
    if not re.fullmatch(r"\d+\.\d+\.\d+-beta\.[1-9]\d*", version):
        raise ValueError("expected an explicit Early Beta mod version")
    return version


def build(root, sdk, commit):
    version = identity(root, commit)
    pin = json.loads((root / ".github/framework-sdk.json").read_text(encoding="utf-8"))
    for name, expected in pin["headers"].items():
        if digest((sdk / name).read_bytes()) != expected:
            raise ValueError("Framework SDK header differs from pin: " + name)
    sys.path.insert(0, str(root / "tools"))
    from build_input_guard import inputs, require_unchanged
    before = inputs(root, sdk)
    output = root / ".ci-build/mod"
    subprocess.run(["cmake", "-S", str(root), "-B", str(output), "-G", "Visual Studio 17 2022", "-A", "x64",
                    "-DMT2MP_BUILD_TESTS=OFF", "-DMT2MP_FRAMEWORK_PLUGIN=ON",
                    "-DMT2_FRAMEWORK_SDK=" + str(sdk.resolve()), "-DMT2MP_BUILD_VERSION=" + version], check=True)
    subprocess.run(["cmake", "--build", str(output), "--config", "Release", "--target",
                    "MT2MultiplayerPlugin", "--parallel", "2", "--", "/nodeReuse:false"], check=True)
    require_unchanged(root, before, sdk)
    binary = output / "bin/Release/MT2MultiplayerPlugin.dll"
    data = binary.read_bytes()
    if not data:
        raise ValueError("Framework build produced an empty plugin")
    receipt = {"version": version, "source_commit": commit, "sdk_commit": pin["revision"],
               "dll_sha256": digest(data),
               "files": {binary.name: {"sha256": digest(data), "bytes": len(data)}}}
    (binary.parent / "framework-build.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print("info: Framework plugin built: " + commit)


def package(root, build_dir, tag, commit, output):
    version = identity(root, commit)
    if tag != "v" + version:
        raise ValueError("release tag differs from native mod version")
    receipt_path = build_dir / "framework-build.json"
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    pin = json.loads((root / ".github/framework-sdk.json").read_text(encoding="utf-8"))
    if any(receipt.get(key) != expected for key, expected in
           (("version", version), ("source_commit", commit), ("sdk_commit", pin["revision"]))):
        raise ValueError("downloaded Framework build identity differs from release source")
    spec = importlib.util.spec_from_file_location("framework_package", root / "tools/package.py")
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    entries, manifest = builder.collect(root, build_dir, receipt_path, mode="vortex",
                                        multiplayer_package=root, release_version=version)
    manifest.update(source_commit=commit, release_tag=tag, sdk_commit=pin["revision"])
    entries["package.json"] = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
    archive = output / ("MMORPGTycoon2-Multiplayer-" + tag + "-windows-x64.zip")
    builder.write_package(entries, archive)
    validate(archive, tag + "@" + commit)
    archive.with_suffix(".zip.sha256").write_text(digest(archive.read_bytes()) + "  " + archive.name + "\n", encoding="ascii")
    print("info: Framework release package checked: " + archive.name)


def validate(archive, candidate):
    tag, separator, commit = candidate.partition("@")
    if not separator or not re.fullmatch(r"v\d+\.\d+\.\d+-beta\.[1-9]\d*", tag) or not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("invalid Framework release identity")
    with zipfile.ZipFile(archive) as zipped:
        names = zipped.namelist()
        if len(names) != len(set(names)) or set(names) != PAYLOAD | {"package.json"}:
            raise ValueError("unexpected or missing Framework release payload")
        if zipped.testzip() is not None:
            raise ValueError("Framework release ZIP integrity failed")
        manifest = json.loads(zipped.read("package.json"))
        mod = json.loads(zipped.read("mod.json"))
        if (manifest.get("package_mode") != "vortex" or manifest.get("source_commit") != commit
                or manifest.get("release_tag") != tag or manifest.get("version") != tag[1:]
                or not re.fullmatch(r"[0-9a-f]{40}", str(manifest.get("sdk_commit", "")))
                or manifest.get("candidate") != "framework-native-plugin"):
            raise ValueError("Framework release source, tag or SDK identity differs")
        if (mod.get("id") != "doodesch.multiplayer" or mod.get("type") != "native"
                or mod.get("entry") != "MT2MultiplayerPlugin.dll" or mod.get("version") != tag[1:]
                or mod.get("api_version") != "1.0.0" or mod.get("multiplayer") != "shared"):
            raise ValueError("Framework release mod contract differs")
        if manifest.get("online_acceptance_passed") is not False:
            raise ValueError("CI cannot claim native online acceptance")
        if set(manifest.get("files", {})) != PAYLOAD:
            raise ValueError("Framework release file manifest differs")
        for name in PAYLOAD:
            data = zipped.read(name)
            record = manifest["files"][name]
            if not data or record.get("size") != len(data) or record.get("sha256") != digest(data):
                raise ValueError("Framework release payload content differs: " + name)
        return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    compile_command = commands.add_parser("build")
    compile_command.add_argument("--sdk", type=Path, required=True)
    compile_command.add_argument("--commit", required=True)
    pack = commands.add_parser("package")
    pack.add_argument("--tag", required=True)
    pack.add_argument("--commit", required=True)
    pack.add_argument("--build", type=Path, required=True)
    pack.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "build":
        build(ROOT, args.sdk, args.commit)
    else:
        package(ROOT, args.build, args.tag, args.commit, args.output)


if __name__ == "__main__":
    main()

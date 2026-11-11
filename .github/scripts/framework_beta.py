"""Build the native Framework plugin in CI and write its build receipt."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def build(root, sdk, commit, ci_ninja=False):
    checked_out = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], check=True,
                                 capture_output=True, text=True).stdout.strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit) or checked_out != commit:
        raise ValueError("build source commit differs from checked-out source")
    # CMake reads the version from mod.json and checks the SDK against the pinned tag.
    version = json.loads((root / "mod.json").read_text(encoding="utf-8"))["version"]
    sdk_tag = json.loads((root / ".github/framework-sdk.json").read_text(encoding="utf-8"))["tag"]
    sys.path.insert(0, str(root / "tools"))
    from build_input_guard import inputs, require_unchanged
    before = inputs(root, sdk)
    output = root / (".ci-build/mod-forced" if ci_ninja else ".ci-build/mod")
    generator = (["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER_LAUNCHER=sccache", "-DMT2MP_CI_FORCE_HEADER=ON",
                  "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=" + str(output / "bin/Release")]
                 if ci_ninja else ["-G", "Visual Studio 17 2022", "-A", "x64"])
    subprocess.run(["cmake", "-S", str(root), "-B", str(output), *generator,
                    "-DMT2MP_BUILD_TESTS=OFF", "-DMT2_FRAMEWORK_SDK=" + str(sdk.resolve())], check=True)
    command = ["cmake", "--build", str(output), "--config", "Release", "--target",
               "MT2MultiplayerPlugin", "--parallel"]
    # Ninja without a job count uses every core of the runner.
    if not ci_ninja:
        command += ["2", "--", "/nodeReuse:false"]
    subprocess.run(command, check=True)
    require_unchanged(root, before, sdk)
    binary = output / "bin/Release/MT2MultiplayerPlugin.dll"
    data = binary.read_bytes()
    if not data:
        raise ValueError("Framework build produced an empty plugin")
    receipt = {"version": version, "source_commit": commit, "sdk_tag": sdk_tag,
               "dll_sha256": digest(data),
               "files": {binary.name: {"sha256": digest(data), "bytes": len(data)}}}
    (binary.parent / "framework-build.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"info: Framework plugin {version} built from {commit} against Framework {sdk_tag}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    compile_command = commands.add_parser("build")
    compile_command.add_argument("--sdk", type=Path, required=True)
    compile_command.add_argument("--commit", required=True)
    compile_command.add_argument("--ci-ninja", action="store_true")
    args = parser.parse_args()
    build(ROOT, args.sdk, args.commit, args.ci_ninja)


if __name__ == "__main__":
    main()

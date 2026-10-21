from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from build_input_guard import inputs, require_unchanged

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description='Build the mod and selected affected checks, or all targets by default.')
parser.add_argument('--target', action='append', help='CMake target to build; repeat for several targets')
parser.add_argument('--version', help='Full version to embed in a local tester build')
parser.add_argument('--mode', choices=('standalone', 'framework'), default='standalone')
parser.add_argument('--framework-sdk', type=Path, help='Framework SDK source tree for the native plugin')
parser.add_argument('--reconfigure', action='store_true', help='Run configure even when the selected CMake cache already matches')
parser.add_argument('--jobs', type=int, default=4, choices=range(1, 65), metavar='N',
                    help='Maximum build jobs, 1..64 (default: 4)')
parser.add_argument('--no-node-reuse', action='store_true',
                    help='Make MSBuild workers exit after this build')
args = parser.parse_args()
if args.version and not re.fullmatch(r'\d+\.\d+\.\d+(?:-beta\.[1-9]\d*)?', args.version):
    parser.error('version must be X.Y.Z or X.Y.Z-beta.N')
if args.mode == 'framework' and args.framework_sdk is None:
    parser.error('framework mode requires --framework-sdk')
if args.mode != 'framework' and args.framework_sdk is not None:
    parser.error('--framework-sdk requires --mode framework')
sdk = args.framework_sdk.resolve() if args.framework_sdk else None
version = args.version
if sdk is not None:
    for header in ('include/mt2fw/plugin.h', 'adapters/mt2/sdk/native_services.h',
                   'adapters/mt2/sdk/native_routes.h', 'adapters/mt2/sdk/native_ownership.h'):
        if not (sdk / header).is_file():
            parser.error('required Framework SDK header missing: ' + str(sdk / header))
    manifest = json.loads((ROOT / 'mod.json').read_text(encoding='utf-8'))
    version = version or manifest['version']
    if version != manifest['version']:
        parser.error('Framework build version must match mod.json; update the manifest before building a new version')
build = ROOT / ('build/framework' if args.mode == 'framework' else 'build')


def matching_cache():
    path = build / 'CMakeCache.txt'
    # CMake can write its cache before configuration fails. Only generated build
    # files establish that this cached configuration is usable.
    if (args.reconfigure or not path.is_file() or
            not (build / 'CMakeFiles/generate.stamp').is_file() or
            not (build / 'MT2_Multiplayer.sln').is_file()):
        return False
    cache = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.startswith(('#', '//')) or '=' not in line or ':' not in line.split('=', 1)[0]:
            continue
        key, value = line.split('=', 1)
        cache[key.split(':', 1)[0]] = value
    expected = {'CMAKE_GENERATOR': 'Visual Studio 17 2022', 'CMAKE_GENERATOR_PLATFORM': 'x64',
                'MT2MP_FRAMEWORK_PLUGIN': 'ON' if sdk else 'OFF'}
    if version:
        expected['MT2MP_BUILD_VERSION'] = version
    if any(cache.get(key) != value for key, value in expected.items()):
        return False
    if Path(cache.get('CMAKE_HOME_DIRECTORY', '')).resolve() != ROOT:
        return False
    return sdk is None or Path(cache.get('MT2_FRAMEWORK_SDK', '')).resolve() == sdk


configure_required = not matching_cache()
cmake = r"C:\Program Files\CMake\bin\cmake.exe"
stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
history = ROOT / "artifacts" / "build_history" / stamp
history.mkdir(parents=True, exist_ok=False)
before = inputs(ROOT, sdk)
commands = [
    [cmake, "-S", str(ROOT), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64",
     "-DMT2MP_FRAMEWORK_PLUGIN=" + ('ON' if sdk else 'OFF'),
     *(["-DMT2_FRAMEWORK_SDK=" + str(sdk)] if sdk else []),
     *(["-DMT2MP_BUILD_VERSION=" + version] if version else [])],
    [cmake, "--build", str(build), "--config", "Release", "--parallel", str(args.jobs),
     *(["--target", *args.target] if args.target else []),
     *(["--", "/nr:false"] if args.no_node_reuse else [])],
]
for index, command in enumerate(commands):
    log = history / f"build-{index}.log"
    if index == 0 and not configure_required:
        log.write_text('info: selected CMake cache matches; build-system regeneration remains enabled\n', encoding='utf-8')
        shutil.copy2(log, ROOT / 'artifacts' / f'build-{index}.log')
        print(f'info: configure skipped: matching mode, SDK and version; log={log}', flush=True)
        continue
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    shutil.copy2(log, ROOT / "artifacts" / f"build-{index}.log")
    print(f"{'info' if result.returncode == 0 else 'error'}: build step {index}: exit={result.returncode} log={log}", flush=True)
    if result.returncode:
        print("\n".join(log.read_text(encoding="utf-8", errors="replace").splitlines()[-45:]))
        sys.exit(result.returncode)
try:
    require_unchanged(ROOT, before, sdk)
except RuntimeError as error:
    print('error: ' + str(error), flush=True)
    (history / 'invalid-inputs.log').write_text(str(error) + '\n', encoding='utf-8')
    sys.exit(1)
if sdk is not None and (not args.target or 'MT2MultiplayerPlugin' in args.target):
    binary = build / 'bin/Release/MT2MultiplayerPlugin.dll'
    if not binary.is_file():
        raise RuntimeError('Successful Framework target did not produce its plugin DLL: ' + str(binary))
    source_record = history / 'source-inputs.json'
    source_record.write_text(json.dumps(before, indent=2) + '\n', encoding='utf-8')
    data = binary.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    receipt = {'candidate': 'framework-native-plugin', 'state': 'linked-source-unchanged-native-pending',
               'version': version, 'dll_sha256': digest, 'native_accepted': False,
               'files': {'MT2MultiplayerPlugin.dll': {'sha256': digest, 'bytes': len(data)}},
               'build_history': str(history.relative_to(ROOT)),
               'source_inputs_sha256': hashlib.sha256(source_record.read_bytes()).hexdigest()}
    output = history / 'framework-build.json'
    output.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    (build / 'build-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(f'info: Framework build receipt: {output}', flush=True)

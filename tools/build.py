from pathlib import Path
from datetime import datetime, timezone
import argparse
import shutil
import subprocess
import sys
from build_input_guard import inputs, require_unchanged

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description='Build the mod and selected affected checks, or all targets by default.')
parser.add_argument('--target', action='append', help='CMake target to build; repeat for several targets')
args = parser.parse_args()
cmake = r"C:\Program Files\CMake\bin\cmake.exe"
stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
history = ROOT / "artifacts" / "build_history" / stamp
history.mkdir(parents=True, exist_ok=False)
before = inputs(ROOT)
commands = [
    [cmake, "-S", str(ROOT), "-B", str(ROOT / "build"), "-G", "Visual Studio 17 2022", "-A", "x64"],
    [cmake, "--build", str(ROOT / "build"), "--config", "Release", "--parallel", "4",
     *(["--target", *args.target] if args.target else [])],
]
for index, command in enumerate(commands):
    log = history / f"build-{index}.log"
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    shutil.copy2(log, ROOT / "artifacts" / f"build-{index}.log")
    print(f"{'info' if result.returncode == 0 else 'error'}: build step {index}: exit={result.returncode} log={log}", flush=True)
    if result.returncode:
        print("\n".join(log.read_text(encoding="utf-8", errors="replace").splitlines()[-45:]))
        sys.exit(result.returncode)
try:
    require_unchanged(ROOT, before)
except RuntimeError as error:
    print('error: ' + str(error), flush=True)
    (history / 'invalid-inputs.log').write_text(str(error) + '\n', encoding='utf-8')
    sys.exit(1)

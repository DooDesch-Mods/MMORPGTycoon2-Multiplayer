import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--native-inc", type=Path, required=True)
parser.add_argument("--reader-fixture", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
native = args.native_inc.read_text(encoding="utf-8-sig")
def part(begin, end):
    if native.count(begin) != 1 or native.count(end) != 1:
        raise SystemExit(f"changed native creation boundary: {begin}")
    return native[native.index(begin):native.index(end)]
args.output.mkdir(parents=True, exist_ok=True)
(args.output / "creation_calls.inc").write_text(
    part("bool same_world(", '#include "research_release.inc"') +
    part("bool startingpoint_clock(", "DWORD startingpoint_deploy(") +
    part("struct StartingpointCalls {", "void commit_current("), encoding="utf-8")
fixture = args.reader_fixture.read_text(encoding="utf-8-sig")
if fixture.count("int tests=0;") != 1:
    raise SystemExit("changed canonical Reader fixture boundary")
(args.output / "reader_fixture.h").write_text(fixture[:fixture.index("int tests=0;")], encoding="utf-8")
(args.output / "inputs.json").write_text(json.dumps({str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in (args.native_inc, args.reader_fixture)}, indent=2) + "\n", encoding="utf-8")

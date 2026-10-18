import argparse
import hashlib
import json
from pathlib import Path


def part(text, begin, end):
    if text.count(begin) != 1 or text.count(end) != 1:
        raise SystemExit(f"source extraction boundary changed: {begin}")
    return text[text.index(begin):text.index(end)]


parser = argparse.ArgumentParser()
parser.add_argument("--native", type=Path, required=True)
parser.add_argument("--reader-fixture", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
native = args.native.read_text(encoding="utf-8-sig")
fixture = args.reader_fixture.read_text(encoding="utf-8-sig")
args.output.mkdir(parents=True, exist_ok=True)
extracted = [
    part(native, "bool step_member(", "struct Scope {"),
    part(native, "struct Scope {", "void command_start("),
    part(native, "void tech_start(", "void sequence_update("),
    part(native, "bool interval(", "struct BorrowedString {"),
]
(args.output / "native_scope.inc").write_text("\n".join(extracted), encoding="utf-8")
native_calls = args.native.with_name("startingpoint_native.inc").read_text(encoding="utf-8-sig")
commit = part(native_calls, "bool same_world(", '#include "research_release.inc"')
commit += native_calls[native_calls.index("void commit_current(void* lounge){"):]
(args.output / "native_commit.inc").write_text(commit, encoding="utf-8")
(args.output / "reader_fixture.h").write_text(
    part(fixture, '#include "tutorial_development.h"', "int tests=0;"), encoding="utf-8")
receipt = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
           for path in (args.native, args.native.with_name("startingpoint_native.inc"), args.reader_fixture)}
(args.output / "extraction.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")

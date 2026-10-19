#!/usr/bin/env python3
"""Parse MMORPG Tycoon 2 log.txt to extract function references and string anchors
for reverse engineering hook points."""

import re
import sys
from pathlib import Path
from collections import defaultdict

def parse_log(log_path: str):
    """Extract source file references and log strings from game log."""
    source_refs = defaultdict(set)  # filename -> set of line numbers
    log_strings = []  # (source_file, line_num, message)

    # Pattern: [filename.cpp:123] or filename.cpp:123:
    ref_pattern = re.compile(r'(\w+\.cpp):(\d+)')
    # Pattern: strings that could be hook anchors
    anchor_pattern = re.compile(r'(==>|>>|!!|LOAD|SHARD|set mode|Starting|Destroying)')

    with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            matches = ref_pattern.findall(line)
            for filename, linenum in matches:
                source_refs[filename].add(int(linenum))

            if anchor_pattern.search(line):
                log_strings.append(line)

    print("=" * 70)
    print("SOURCE FILE REFERENCES")
    print("=" * 70)
    for filename in sorted(source_refs.keys()):
        lines = sorted(source_refs[filename])
        print(f"\n{filename}:")
        for ln in lines:
            print(f"  :{ln}")

    print("\n" + "=" * 70)
    print("POTENTIAL HOOK ANCHOR STRINGS")
    print("=" * 70)
    for s in log_strings:
        print(f"  {s}")

    print(f"\nTotal source files: {len(source_refs)}")
    print(f"Total anchor strings: {len(log_strings)}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        # Default path
        log_path = Path.home() / "AppData/Roaming/VectorStorm/MMORPG Tycoon 2"
        # Find most recent steam ID folder
        candidates = list(log_path.glob("*/log.txt"))
        if candidates:
            parse_log(str(candidates[0]))
        else:
            print(f"Usage: {sys.argv[0]} <path_to_log.txt>")
    else:
        parse_log(sys.argv[1])

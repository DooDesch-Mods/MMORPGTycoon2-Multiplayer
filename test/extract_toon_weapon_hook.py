"""Extract the actual fixed Equip boundary for the portable call-order test."""
from pathlib import Path
import argparse
parser = argparse.ArgumentParser()
parser.add_argument('--source-root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = (args.source_root / 'src/game/toon_weapon_visual/native.cpp').read_text(encoding='utf-8')
start = source.index('void equip(uintptr_t toon,bool rez){')
brace = source.index('{', start)
depth = 1
end = brace + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(source[start:end] + '\n', encoding='utf-8')

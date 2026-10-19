"""Publish the frozen additive global-root reader without changing legacy readers."""
import hashlib
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
STAGING = ROOT / 'tools/engine_analysis/calendar_authority/global_root/staging'
HASHES = {
    'global_root.inc': '0175811a666e2025810c49a0f3f8e6f95120eb4f1b2e532fd0e49fb370f47b51',
    'global_root.h': 'ce8e094a9eb15109cf289fd46865c29b5be6234af0d747da3a09ce37102230a8',
    'global_building_catalog.cpp': 'bc5bc4122d4e2a8f3273dca729caf85ae4b452ce7ba0fb4f6157d6c7891d7497',
    'global_building_catalog.h': 'f2aaa3b7cf9219e5957367281d2a9a5b08727e12150ba4209125c2c0e6355292',
}
for name, expected in HASHES.items():
    actual = hashlib.sha256((STAGING / name).read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError(f'Frozen source changed file={name} actual={actual} required={expected}')
native = ROOT / 'src/game/native_engine.cpp'
text = native.read_text(encoding='utf-8')
include = '#include "global_root.inc"'
if include not in text:
    expected = '1b3dff8e48810eb7c0fd2ba8bdf358a52d97436d665fcf052c10f18eea78cd79'
    actual = hashlib.sha256(native.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError(f'Native engine base changed actual={actual} required={expected}')
    text = text.rstrip() + '\n\n' + include + '\n'
for name in HASHES:
    shutil.copy2(STAGING / name, ROOT / 'src/game' / name)
native.write_text(text, encoding='utf-8')
print('info: published four frozen global-root files and additive native TU include')

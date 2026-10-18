from pathlib import Path
import argparse

p = argparse.ArgumentParser()
p.add_argument('--source-root', type=Path, required=True)
p.add_argument('--dependency-root', type=Path)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
dependency = a.dependency_root or a.source_root
a.output.mkdir(parents=True, exist_ok=True)
source = (a.source_root / 'src/game/live_host_state/day_rollover/economy/native_quest_state.inc').read_text(encoding='utf-8')
begin = source.index('    const auto refresh_weapon=')
end = source.index('    if(!actual||!target)', begin)
(a.output / 'weapon_refresh_branch.inc').write_text(source[begin:end], encoding='utf-8')
line = '    if(!store_toon_weapon(p,toon,*actual,*target,weapon_fence,e)||!refresh_weapon())return false;'
if source.count(line) != 1:
    raise SystemExit('Native tuple refresh boundary changed')
(a.output / 'weapon_refresh_store.inc').write_text(line + '\n', encoding='utf-8')

source_weapon = (dependency / 'src/game/live_host_state/day_rollover/economy/native_weapon_state.inc').read_text(encoding='utf-8')
begin_weapon = source_weapon.index('bool same_weapon(')
end_weapon = source_weapon.index('\n}', begin_weapon) + 2
(a.output / 'weapon_same_tuple.inc').write_text(source_weapon[begin_weapon:end_weapon] + '\n', encoding='utf-8')

native = (dependency / 'src/game/toon_weapon_visual/native.cpp').read_text(encoding='utf-8')
start = native.index('bool matrix(const Plan& p,')
stop = native.index('{', start) + 1
depth = 1
while depth:
    depth += (native[stop] == '{') - (native[stop] == '}')
    stop += 1
(a.output / 'weapon_matrix_body.inc').write_text(native[start:stop] + '\n', encoding='utf-8')
source = (dependency / 'src/game/live_host_state/bridge.cpp').read_text(encoding='utf-8')
begin = source.index('struct WeaponEconomyScope {')
end = source.index('} weapon_scope(', begin)
(a.output / 'weapon_economy_scope.inc').write_text(source[begin:end] + '};\n', encoding='utf-8')

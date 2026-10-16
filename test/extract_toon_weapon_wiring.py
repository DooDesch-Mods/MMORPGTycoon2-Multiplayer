from pathlib import Path
import argparse

p = argparse.ArgumentParser()
p.add_argument('--source-root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
source = (a.source_root / 'src/game/live_host_state/day_rollover/economy/native_quest_state.inc').read_text(encoding='utf-8')
begin = source.index('    const auto refresh_weapon=')
end = source.index('    if(!actual||!target)', begin)
(a.output / 'weapon_refresh_branch.inc').write_text(source[begin:end], encoding='utf-8')
line = '    if(!store_toon_weapon(p,toon,*actual,*target,weapon_fence,e)||!refresh_weapon())return false;'
if source.count(line) != 1:
    raise SystemExit('Native tuple refresh boundary changed')
(a.output / 'weapon_refresh_store.inc').write_text(line + '\n', encoding='utf-8')
source = (a.source_root / 'src/game/live_host_state/bridge.cpp').read_text(encoding='utf-8')
begin = source.index('struct WeaponEconomyScope {')
end = source.index('} weapon_scope(', begin)
(a.output / 'weapon_economy_scope.inc').write_text(source[begin:end] + '};\n', encoding='utf-8')

import argparse
import hashlib
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--source-root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
source = (a.source_root / 'src/game/toon_time_fence/native.cpp').read_text(encoding='utf-8')
original_timer = '        live_host_state::apply_timing::Scope timing(live_host_state::apply_timing::Part::ToonOriginal);\n'
normalize_timer = '    {live_host_state::apply_timing::Scope timing(live_host_state::apply_timing::Part::ToonNormalize);\n        normalize(calls,toon,dt,receipt);}'
assert source.count(original_timer) == 1
assert source.count(normalize_timer) == 1
original_boundary = '        uint32_t exception=0;\n        {live_host_state::apply_timing::PageReadScope boundary(live_host_state::apply_timing::PageBoundary::OriginalCall);\n            exception=update_call(toon,dt);}'
assert source.count(original_boundary) == 1
restored = source.replace(original_boundary, '        const auto exception=update_call(toon,dt);').replace(original_timer, '').replace(normalize_timer, '    normalize(calls,toon,dt,receipt);')
backend_include = '#include "mt2mp/hooks/minhook_backend.h"'
assert restored.count(backend_include) == 1, 'Toon hook backend include changed'
restored = restored.replace(backend_include, '#include <MinHook.h>', 1)
assert hashlib.sha256(restored.encode()).hexdigest() == 'e58ee34b277bb321d95c7917de499f80f31d700fe1025e7f33cad2ba351602e7', 'Toon native behavior changed beyond two timer scopes and the original-call metadata boundary'

def body(marker):
    start = source.index(marker)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

a.output.mkdir(parents=True, exist_ok=True)
(a.output / 'toon_time_update.inc').write_text(body('void update(uintptr_t toon,float dt)'), encoding='utf-8')
(a.output / 'toon_time_original.inc').write_text(body('bool original(uintptr_t toon,float dt,std::string& error)'), encoding='utf-8')
print('Exact timers and original-call metadata boundary; production bodies extracted')

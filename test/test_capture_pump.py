from pathlib import Path
import argparse
p = argparse.ArgumentParser()
p.add_argument("--source", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

source = (a.source / 'src/game/ui_move_runtime.cpp').read_text(encoding='utf-8')
legacy = source[source.index('void tick(){\n    boss_placement::tick();'):source.index('void tick_core_capture(){')]
core = source[source.index('void tick_core_capture(){'):source.index('    const auto intent=*r->pending;', source.index('void tick_core_capture(){'))]
if legacy.count('selection_move::tick()') != 0 or legacy.count('tick_core_capture()') != 1:
    raise RuntimeError('Legacy path does not delegate exactly once to core capture')
if core.count('selection_move::tick()') != 1 or core.index('selection_move::tick()') > core.index('if(!r||!enabled()||!r->pending)return;'):
    raise RuntimeError('Ordered core does not flush selection before single-Move early return')
if legacy.count('group_edit::tick()') != 0 or core.count('group_edit::tick()') != 1:
    raise RuntimeError('Group capture must flush exactly once through shared core')
if legacy.count('duplicate_edit::tick()') != 0 or core.count('duplicate_edit::tick()') != 1:
    raise RuntimeError('Duplicate capture must flush exactly once through shared core')
names = ('boss_placement', 'group_object', 'modular', 'district_edit',
         'district_stroke', 'district_title', 'district_text', 'prop_text', 'boss_phase', 'service_membership')
prefix = '#include <atomic>\n#include <iostream>\nstruct Runtime { bool pending=false; };\nstd::atomic<Runtime*> published{nullptr};\nbool enabled(){return true;}\n'
prefix += 'int selection_calls=0, scenery_calls=0, duplicate_calls=0, group_calls=0, legacy_calls=0;\n'
prefix += 'namespace group_edit { void tick(){++group_calls;} }\n'
prefix += 'namespace duplicate_edit { void tick(){++duplicate_calls;} }\n'
prefix += 'namespace selection_move { void tick(){++selection_calls;} }\nnamespace scenery_stroke { void tick(){++scenery_calls;} }\n'
prefix += ''.join('namespace ' + name + ' { void tick(){++legacy_calls;} }\n' for name in names)
prefix += 'void tick_core_capture();\n'
main = '''\nint main(){
    tick_core_capture();
    if(selection_calls!=1||scenery_calls!=1||duplicate_calls!=1||group_calls!=1||legacy_calls!=0)return 1;
    tick();
    if(selection_calls!=2||scenery_calls!=2||duplicate_calls!=2||group_calls!=2||legacy_calls!=10)return 2;
    std::cout<<"2 actual ordered and legacy capture-pump call-count cases passed\\n";
}\n'''
(a.output / 'test_capture_pump.cpp').write_text(prefix + legacy + core + '}\n' + main, encoding='utf-8')
print('actual capture-pump bodies extracted')

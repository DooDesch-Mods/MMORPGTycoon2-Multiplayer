from pathlib import Path
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def body(source, anchor):
    start = source.index(anchor)
    pos = source.index('{', start)
    depth = 1
    end = pos + 1
    while depth:
        if source[end] == '{':
            depth += 1
        elif source[end] == '}':
            depth -= 1
        end += 1
    return source[start:end] + '\n'


for filename, source_path, anchor in (
    ('move_history_method.inc', 'src/game/typed_action_runtime.cpp', '    action::Outcome perform_move_history('),
    ('dungeon_original_scope.inc', 'src/game/typed_action_runtime.cpp', '    bool dungeon_scope('),
    ('history_effect.inc', 'src/game/typed_action/action_session.cpp', 'bool object_effect('),
    ('session_wire_generation.inc', 'src/net/session_wire.cpp', 'uint64_t session_id()'),
    ('local_session_context.inc', 'src/game/typed_action/sidecar_adapter.cpp', 'bool LocalSession::current('),
):
    source = (args.root / source_path).read_text(encoding='utf-8')
    (args.output / filename).write_text(body(source, anchor), encoding='utf-8')

# Preserve complete history dispatch with its current predicates and real effect validators.
dependencies = ['#include <regex>\n']
for source_path, anchors in (
    ('src/game/typed_action/action_session.cpp', ['bool uuid(']),
    ('src/game/typed_action/modular_codec.inc', ['bool modular_history(', 'bool modular_action(']),
    ('src/game/typed_action/group_object_codec.inc', ['bool group_object_history(']),
    ('src/game/typed_action/scenery_stroke_history_codec.inc', ['bool scenery_stroke_history(']),
    ('src/game/typed_action/duplicate_history_codec.inc', ['bool duplicate_history(']),
    ('src/game/typed_action/action_session.cpp', ['bool group_history(']),
    ('src/game/typed_action/monster_edit_codec.inc', ['bool monster_history_effect(']),
):
    source = (args.root / source_path).read_text(encoding='utf-8')
    dependencies.extend(body(source, anchor) for anchor in anchors)
for local, public in (
    ('modular_effect', 'valid_modular_effect'),
    ('group_object_effect', 'valid_group_object_effect'),
    ('scenery_stroke_history_effect', 'valid_scenery_stroke_effect'),
    ('duplicate_history_effect', 'valid_duplicate_history_effect'),
    ('group_history_effect', 'valid_group_history_effect'),
):
    dependencies.append(f'bool {local}(const Request& r,const Evidence& e){{return action::{public}(r,e);}}\n')
path = args.output / 'history_effect.inc'
path.write_text(''.join(dependencies) + path.read_text(encoding='utf-8'), encoding='utf-8')

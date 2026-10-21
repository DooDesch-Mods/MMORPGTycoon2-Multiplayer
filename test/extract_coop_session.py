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

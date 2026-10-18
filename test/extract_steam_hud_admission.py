"""Extract current HUD admission and transport wiring, with no native hooks."""
import argparse
import hashlib
import json
from pathlib import Path


def body(source, signature):
    start = source.index(signature)
    opened = source.index('{', start)
    depth = 0
    for end in range(opened, len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if depth == 0:
            return source[start:end + 1]
    raise ValueError(f'Unclosed production body: {signature}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sources = {name: (args.root / 'src/game' / name).read_text(encoding='utf-8')
               for name in ('hud_time_runtime.cpp', 'hud_calendar_runtime.cpp')}
    for name, source in sources.items():
        assert 'SidecarClient' not in source and 'local_test' not in source, name
        assert 'LocalSession' not in source and 'local_session()' not in source, name
        assert 'selected_session().current' in source, name
    time = body(sources['hud_time_runtime.cpp'], 'bool authority(')
    calendar_source = sources['hud_calendar_runtime.cpp']
    calendar = '\n'.join(body(calendar_source, signature) for signature in
                         ('bool read_context(', 'bool context(', 'bool transport_context('))
    assert 'session!=expected' in calendar
    send = body(calendar_source, 'services.send=[]') + ';'
    claim = body(calendar_source, 'services.register_handler=[]') + ';'
    assert calendar_source.count('session_wire::claim_handler(') == 1
    assert 'session_wire::send(' in send and 'failure_reason()' in send
    assert 'throw std::runtime_error(cause)' in claim
    (args.output / 'steam_hud_time.inc').write_text(time + '\n', encoding='utf-8')
    (args.output / 'steam_hud_calendar.inc').write_text(calendar + '\n', encoding='utf-8')
    (args.output / 'steam_hud_wire.inc').write_text(send + '\n' + claim + '\n', encoding='utf-8')
    (args.output / 'source_hashes.json').write_text(json.dumps({name: hashlib.sha256(
        (args.root / 'src/game' / name).read_bytes()).hexdigest() for name in sources}, indent=2) + '\n', encoding='utf-8')
    print('Extracted current HUD authority, Calendar admission, and exclusive session-wire callbacks.')


if __name__ == '__main__':
    main()

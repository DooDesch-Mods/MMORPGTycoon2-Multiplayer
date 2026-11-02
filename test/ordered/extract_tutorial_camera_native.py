import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = (args.source / 'src/game/tutorial_movement/camera_native.inc').read_text(encoding='utf-8')
prefix = source[:source.index('bool camera_pin(')]
position = source[source.index('DWORD camera_position_original('):source.index('CameraFixture camera_fixture(')]
event = source[source.index('std::string camera_event_bytes('):]
if position.count('camera_position_original(window,timestamp') != 1 or event.count('camera_fixture.polled(kind,frame,error)') != 1:
    raise SystemExit('Actual SDL camera position or poll boundary differs')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(prefix + position + event, encoding='utf-8')

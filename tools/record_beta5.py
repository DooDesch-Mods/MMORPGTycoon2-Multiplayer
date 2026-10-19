"""Bind Beta 5 to the actual named-world, HUD and shutdown receipts."""
import argparse
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--shutdown', required=True)
args = parser.parse_args()
paths = {
    'named_world_start_and_rejoin': 'artifacts/acceptance/b97004c6-1072-4df8-9cd0-ecac40a7fddf/summary.json',
    'native_hud_buttons': 'artifacts/acceptance/06568e3f-0493-4f0a-9975-de97549c69a0/summary.json',
    'native_shutdown': args.shutdown,
}
records = {}
for name, path in paths.items():
    data = (root / path).read_bytes()
    value = json.loads(data)
    if value.get('passed') is not True:
        raise SystemExit('Native acceptance not complete: ' + path)
    records[name] = {'path': path, 'sha256': hashlib.sha256(data).hexdigest()}
named = json.loads((root / paths['named_world_start_and_rejoin']).read_text(encoding='utf-8'))
hud = json.loads((root / paths['native_hud_buttons']).read_text(encoding='utf-8'))
if named['runtime_run_id'] != hud['runtime_run_id'] or len(hud['attempts']) != 4:
    raise SystemExit('Named-world and HUD runtime or accepted button count differs')
binary = hashlib.sha256((root / 'build/bin/Release/MT2_Multiplayer.dll').read_bytes()).hexdigest()
if named['dll_sha256'] != binary:
    raise SystemExit('Accepted local binary differs from the retained build')
data = {
    'version': '0.1.0-beta.5',
    'native_checks': [
        'The small retained fixture loads in an ordinary Worlds of Titans folder; both peers reach Playing, native software ACK, world authority and enabled action captures.',
        'Automatic Host-world preparation starts after the external native load acknowledgement without replaying that diagnostic request.',
        'Both peers leave to Title, rejoin in the same processes, select the retained named world through the native picker and reach Playing using the sole load/start button.',
        'The native lobby frame shows the single load/start control with readable layout.',
        'Actual native cash, ledger and calendar agree after both shared loads.',
        'Native Client HUD speed and Pause plus Host Normal and Pause each receive one Host effect and matching peer presentation. Client simulation clock remains paused.',
        'Both test games exit normally with code 0 after final Leave.'
    ],
    **records,
    'mod_sha256': binary,
    'build': 'artifacts/build_history/20261014T225654087739Z',
    'regressions': ['named_world', 'client_leave', 'auto_host_world', 'prepare_current_world', 'lobby_row_join', 'diagnostic_mailbox'],
    'unit_scope': 'Actual production decision bodies, native primitive doubles and the real action Controller; no game launch in unit CI.',
    'limits': [
        'The local pair uses loopback transport. Separate-account Steam discovery, admission and gameplay require the next manual friend test.',
        'Direct public row Join is covered through actual command and callback bodies, not a new two-account discovery run.',
        'The named-world acceptance proves preparation, shared live state, HUD controls and rejoin. Prior object-action evidence is carried; every action family was not replayed.',
        'Tutorial progression is not shared yet. Complete action coverage, Undo/Redo, four players and large worlds remain open.',
        'The tested local binary and the CI release are distinct builds of the same intended source. Downloaded release startup is recorded separately after publication.'
    ]
}
(root / 'docs/evidence/beta5-native.json').write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
milestone = {'schema': 1, 'version': '0.1.0-beta.5',
             'summary': 'Allow ordinary world names, retire previous Client bindings, unify world loading and start, and join public lobby rows directly.',
             'native_evidence': 'docs/evidence/beta5-native.json'}
(root / '.github/beta-milestone.json').write_text(json.dumps(milestone, indent=2) + '\n', encoding='utf-8')
print('Beta 5 milestone recorded from accepted native receipts')

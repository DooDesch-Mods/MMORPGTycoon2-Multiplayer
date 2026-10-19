"""Record one actual native acceptance without replaying any game operation."""
import argparse
import hashlib
import json
from pathlib import Path
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--acceptance', required=True)
args = parser.parse_args()
identifier = str(uuid.UUID(args.acceptance))
directory = ROOT / 'artifacts/acceptance' / identifier
summary_path = directory / 'summary.json'
summary = json.loads(summary_path.read_text(encoding='utf-8'))
assert summary['acceptance_id'] == identifier and summary['state'] != 'running'
checks = {entry['check']: entry['result'] for entry in summary['checks']}
capture = checks['host-native-capture']
instances = checks['instance-manifest-identity']
receipts = {}
for job in summary['jobs']:
    if job['step'] not in ('host-final-readback', 'client-final-readback'):
        continue
    source = Path(job['evidence_path']).resolve()
    source.relative_to(ROOT.resolve())
    assert hashlib.sha256(source.read_bytes()).hexdigest() == job['evidence_sha256']
    evidence = json.loads(source.read_text(encoding='utf-8'))
    response = next(c['result']['structuredContent'] for c in evidence['calls'] if c['tool'] == 'game_inspect_engine')
    assert response['ok'] and response['run_id'] == instances['host']['run_id']
    net = response['result']['network_identity']
    assert net['uuid'] == capture['snapshot_id'] and net['snapshot_sha256'] == capture['sha256']
    receipts[job['peer']] = {'evidence_path': str(source), 'network_identity': net}
assert set(receipts) == {'host', 'client-1'}
record = {'schema_version': 1, 'acceptance_id': identifier, 'passed': summary['passed'],
          'full_coop_proven': False, 'runtime_run_id': instances['host']['run_id'],
          'deployed_dll_sha256': instances['host']['installed_binary_sha256']['MT2_Multiplayer.dll'],
          'native_file': capture, 'proof': summary['proof'], 'peers': receipts,
          'building_capture': checks['host-building-capture-receipt'],
          'original_error': summary.get('error'), 'game_operations': 0}
target = directory / 'native_identity_checkpoint.json'
assert not target.exists(), 'Existing checkpoint must stay immutable'
target.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
host, client = (receipts[p]['network_identity'] for p in ('host', 'client-1'))
net_confirmed = host['ready'] and client['ready'] and host['keys_sha256'] == client['keys_sha256']
assert not net_confirmed or host['staged_nodes'] == client['staged_nodes'] == 240
state_path = ROOT / '_drafts/implementation_state.md'
state = state_path.read_text(encoding='utf-8')
heading = '## Aktueller nativer Netzwerkversuch\n\n'
if heading in state:
    start = state.index(heading)
    end = state.index('\n## ', start + len(heading))
    retained = ''.join(line + '\n\n' for line in state[start:end].splitlines()
                       if 'bda05d23-78d4-4fa2-afaa-70762a2724ce' in line)
    state = state[:start] + state[end+1:]
else:
    retained = ''
insert = state.index('## Vorherige native Abnahmen') if '## Vorherige native Abnahmen' in state else state.index('## Aktueller Kandidat und Lauf')
cause = client['first_failure'] or 'keine'
next_action = ('Nativen Dateiursprung vor dem ersten Lesen belegen und anschließend den Host-Zustandsadapter prüfen.'
               if net_confirmed else 'Fehlgeschlagenen nativen Guard anhand seiner Call-/Thread-/Lifetime-Kette klären.')
block = f'''{heading}Eigener Lauf `{record['runtime_run_id']}`, deployed DLL `{record['deployed_dll_sha256']}`. [Abnahme](../artifacts/acceptance/{identifier}/summary.json), [unveränderlicher Identitätsbeleg](../artifacts/acceptance/{identifier}/native_identity_checkpoint.json). Datei `{capture['snapshot_id']}`, {capture['bytes']} Bytes, SHA256 `{capture['sha256']}`. Dateiübertragung und tatsächlicher Client-Load: {summary['proof']['transfer_and_client_load']}.

Host-Netzwerkkarte ready={host['ready']}, Nodes={host['staged_nodes']}; Client ready={client['ready']}, Nodes={client['staged_nodes']}. Client-Ursache: `{cause}`. Portable Netzwerkidentität bestätigt={net_confirmed}. Gebäudeursache: `{record['building_capture']['error']}`. Vollständiger Koop bleibt unbewiesen.

Nächster Schritt: {next_action} Keine Wiederholung einer ausstehenden Mutation. NETSTATE- und NETSESSION-Schreibadapter bleiben bis zur vollständigen realen Karte und dem Beleg der tatsächlich geöffneten Datei im Staging. Die Prüfsumme der übertragenen Datei allein beweist diesen nativen Dateiursprung nicht.

'''
state_path.write_text(state[:insert] + block + retained + state[insert:], encoding='utf-8')
print(f'info: checkpoint={target} host_nodes={host["staged_nodes"]} client_nodes={client["staged_nodes"]} network_confirmed={net_confirmed} cause={cause}')

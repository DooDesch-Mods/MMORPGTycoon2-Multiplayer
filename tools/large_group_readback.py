"""Read every owned peer after a completed large-save acceptance, without replay."""
import argparse
from pathlib import Path
import re
import uuid

from large_save_acceptance import (LargeSaveRunner, INPUT_ID, SUBSCRIBER_IDS, parser as large_parser,
                                  software_state, confirm_time_ui)
from local_acceptance import (AcceptanceError, AcceptanceRunner, ROOT, compare_ledgers,
                              log, read_json, utc, validate_snapshot)

def confirm_group_maps(engines, capture, peers):
    if set(engines) != set(peers) or 'host' not in engines:
        raise AcceptanceError('native group readback is missing an expected peer')
    result = {}
    for peer in peers:
        identity = engines[peer].get('network_identity', {})
        if (identity.get('ready') is not True or identity.get('staged_nodes') != 240 or
            identity.get('uuid') != capture['snapshot_id'] or
            identity.get('snapshot_sha256') != capture['sha256'] or
            not re.fullmatch('[0-9a-f]{64}', identity.get('keys_sha256', ''))):
            raise AcceptanceError(f'complete native map differs peer={peer}')
        if peer != 'host' and identity['keys_sha256'] != result['host']['keys_sha256']:
            raise AcceptanceError(f'native map digest differs peer={peer}')
        result[peer] = identity
    return result

def confirm_source(source, resume_time_inventory):
    proofs = source.get('proof', {})
    complete = all(proofs.get(key) is True for key in
                   ('host_native_load', 'host_native_capture', 'transfer_and_client_load', 'network_identity_confirmed'))
    expected_failure = (source.get('passed') is False and
                        source.get('error', {}).get('step') == 'client-native-time-items' and
                        'time reader unexpectedly captures or applies state:' in source.get('error', {}).get('cause', ''))
    if not complete or (source.get('passed') is not True and not (resume_time_inventory and expected_failure)):
        raise AcceptanceError('source acceptance lacks confirmed native load/map or expected legacy time-reader failure')

class GroupReadback(LargeSaveRunner):
    def single(self, name, peer, tool, arguments=None):
        if tool == 'game_capture_frame':
            return AcceptanceRunner.single(self, name, peer, tool, arguments)
        return super().single(name, peer, tool, arguments)

    def run(self):
        try:
            source_id = str(uuid.UUID(self.args.source_acceptance))
            source = read_json(ROOT / 'artifacts/acceptance' / source_id / 'summary.json')
            count = source['parameters']['players']
            confirm_source(source, self.args.resume_time_inventory)
            if count not in (2, 3, 4):
                raise AcceptanceError('source acceptance player count is invalid')
            self.args.players = count
            self.summary['parameters']['players'] = count
            self.summary['scope'] = 'all owned peers: paused large-world readbacks and native frames; no load or action'
            self.summary['source_acceptance'] = source_id
            self.summary['source_acceptance_failed'] = not source['passed']
            self.load_manifests()
            if source['runtime_run_id'] != self.summary['runtime_run_id']:
                raise AcceptanceError('source acceptance run differs from current owned run; no replay')
            peers = ['host'] + [f'client-{index}' for index in range(1, count)]
            for peer in peers[2:]:
                path = self.root / peer / 'instance.json'
                manifest = read_json(path)
                index = int(peer.split('-')[1])
                if (manifest['run_id'] != source['runtime_run_id'] or
                    manifest.get('schema_version') != 1 or
                    manifest.get('owner') != {'kind': 'MPTest', 'run_id': source['runtime_run_id']} or
                    manifest.get('peer_id') != index + 1 or
                    type(manifest.get('pid')) is not int or
                    not str(manifest.get('process_start_time', '')).isdigit() or
                    manifest.get('installed_binary_sha256') != read_json(self.manifests['host'])['installed_binary_sha256'] or
                    Path(manifest['runtime_root']).resolve() != (self.root / peer).resolve()):
                    raise AcceptanceError(f'owned peer manifest differs peer={peer}')
                self.manifests[peer] = path
            self.checked('all-peer-manifest-identity', {p: read_json(self.manifests[p]) for p in peers})
            capture = next(c['result'] for c in source['checks'] if c['check'] == 'host-native-capture')
            engines, samples, frames, files = {}, {}, {}, {}
            for peer in peers:
                pending = AcceptanceRunner.single(self, f'{peer}-pending', peer, 'observe_pending')
                if pending.get('pending') is not False:
                    raise AcceptanceError(f'owned peer still has a pending MCP request peer={peer}; no replay')
                engines[peer] = self.paused_engine(peer, f'{peer}-group-engine')
                identity = engines[peer].get('network_identity', {})
                if identity.get('ready') is not True or identity.get('staged_nodes') != 240 or identity.get('uuid') != capture['snapshot_id'] or identity.get('snapshot_sha256') != capture['sha256']:
                    raise AcceptanceError(f'live complete network identity differs peer={peer}: {identity}')
                if peer != 'host' and identity['keys_sha256'] != engines['host']['network_identity']['keys_sha256']:
                    raise AcceptanceError(f'live key digest differs peer={peer}')
                record = read_json(self.root / peer / 'diagnostics' / f"snapshot-{capture['snapshot_id']}.json")
                if record.get('sha256') != capture['sha256'] or record.get('bytes') != capture['bytes']:
                    raise AcceptanceError(f'transferred file receipt differs peer={peer}')
                files[peer] = validate_snapshot(record, capture['snapshot_id'], self.root / peer)
                samples[peer] = self.samples(peer)
                if set(samples[peer]) != {str(i) for i in SUBSCRIBER_IDS} or samples[peer] != samples['host']:
                    raise AcceptanceError(f'complete selected subscriber readback differs peer={peer}')
                compare_ledgers(engines['host'], engines[peer])
                if software_state(engines['host']) != software_state(engines[peer], client=peer != 'host'):
                    raise AcceptanceError(f'paused software readback differs peer={peer}')
                self.checked(f'{peer}-native-building-catalog', self.single(f'{peer}-group-catalog', peer, 'game_read_building_catalog'))
                if peer != 'host' or 'shared_time' in engines[peer]:
                    presentation = self.single(f'{peer}-group-time-items', peer, 'game_read_time_ui')
                    confirm_time_ui(presentation, engines[peer])
                    self.checked(f'{peer}-native-time-items', presentation)
                frames[peer] = self.single(f'{peer}-group-frame', peer, 'game_capture_frame')
                self.memory(peer)
            self.checked('all-peer-engine-readbacks', engines)
            self.checked('all-peer-native-network-identity', confirm_group_maps(engines, capture, peers))
            self.checked('all-peer-transferred-files', files)
            self.checked('all-peer-selected-subscribers', samples)
            self.checked('all-peer-native-frames', frames)
            self.summary['proof'] = {'all_peer_network_identity': True, 'all_peer_native_load': True,
                                     'all_peer_file_verified': True, 'all_peer_selected_needs': True,
                                     'all_peer_software': True, 'all_peer_ledger': True,
                                     'native_frames_captured': True}
            if all('shared_time' in engines[peer] for peer in peers):
                self.summary['proof']['authoritative_time_receipts_and_client_items'] = True
            self.summary['state'], self.summary['passed'] = 'confirmed-all-peer-large-readback', True
        except Exception as error:
            self.summary['state'], self.summary['passed'] = 'failed', False
            self.summary['error'] = {'cause': f'{type(error).__name__}: {error}', 'mutation_replayed': False}
            log('error', f'group readback failed acceptance={self.id} cause={error}')
        finally:
            self.summary['finished_utc'] = utc()
            self.summary['full_coop_proven'] = False
            self.persist()
        log('info' if self.summary['passed'] else 'error', f"group readback passed={self.summary['passed']} summary={self.summary_path}")
        return 0 if self.summary['passed'] else 1

if __name__ == '__main__':
    options = argparse.ArgumentParser(description=__doc__)
    options.add_argument('--source-acceptance', required=True)
    options.add_argument('--resume-time-inventory', action='store_true')
    given = options.parse_args()
    args = large_parser().parse_args([])
    args.source_acceptance = given.source_acceptance
    args.resume_time_inventory = given.resume_time_inventory
    args.cleanup = False
    args.snapshot_id = INPUT_ID
    raise SystemExit(GroupReadback(args).run())

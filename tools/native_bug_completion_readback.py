"""Read-only finalization after the original observer has paused both peers."""
import argparse
import json
from pathlib import Path
import copy
import sys
ROOT = next(p for p in Path(__file__).resolve().parents if (p / 'tools/local_acceptance.py').is_file())
sys.path.insert(0, str(ROOT / 'tools'))
from local_acceptance import AcceptanceError, AcceptanceRunner, hash_file, read_json, utc
PEERS = ('host', 'client-1')

def require(value, cause):
    if not value:
        raise AcceptanceError(cause)

def sample(runner, peer, label, paused=False, require_bug_work=True, extended_frames=False):
    engine = runner.inspect(label + '-' + peer, peer)
    controller = engine['typed_actions']
    live = controller['live_state']
    require(controller['initialized'] and controller['started'] and not controller['locked']
            and not controller['pending'] and controller['failed'] == 0 and not controller.get('live_error')
            and live['active'] and not live['locked'] and not live.get('cause'),
            peer + ' Controller/live state failed: ' + str(controller.get('cause')) + ' ' + str(live))
    if paused or peer == 'client-1':
        require(engine['clock']['scale'] == 0 and engine['clock']['paused'], peer + ' actual Clock is not paused')
    if peer == 'client-1':
        require(live['confirmed'], 'Client last authoritative frame is not confirmed')
    frame = controller['native_economy']['day']['frame']
    vip_armed = controller.get('vip_frame', {}).get('armed', False)
    expected_schemas = ((13, 14, 15, 16) if extended_frames else (13,)) if vip_armed else (12,)
    require(frame['schema'] in expected_schemas,
            peer + ' expected frame schemas' + str(expected_schemas) + ' for current VIP arming')
    require(isinstance(frame.get('vips'), dict) if vip_armed else 'vips' not in frame,
            peer + ' VIP frame component differs from current arming')
    if frame['schema'] >= 15:
        require(isinstance(frame.get('marketing'), dict)
                and controller.get('marketing_frame', {}).get('unknown') is False,
                peer + ' Marketing frame component or native acknowledgement absent')
    if require_bug_work:
        work = frame['developers'].get('construction', {})
        require(all(k in work for k in ('bug_jobs', 'bug_parents', 'job_created', 'job_fixed')), peer + ' complete Work state absent')
    native = runner.single(label + '-native-' + peer, peer, 'game_read_native_developers')
    require(native['observed'] and native['developer_movement']['observed']
            and native['jobs']['queue_count'] == len(native['jobs']['jobs']), peer + ' native graph observation incomplete')
    return {'engine': engine, 'frame': frame, 'native': native}

def physical_jobs(native):
    rows = copy.deepcopy(native['jobs']['jobs'])
    for row in rows:
        row.pop('raw_id_untrusted', None)
        row['bug'].pop('runtime_id_local', None)
    return rows

def work(sampled):
    return sampled['frame']['developers']['construction']


def validate(final, uis, selected):
    require(final['host']['frame'] == final['client-1']['frame'], 'Paused complete frames differ')
    for peer in PEERS:
        value = final[peer]
        frame, native = value['frame'], value['native']
        state = work(value)
        require(not any(j[6:10] == selected['job'] for j in state['bug_jobs']), peer + ' selected Job still published')
        require(not any(b[:6] == selected['bug'] for b in frame['bugs']), peer + ' selected Bug still published')
        parents = [p for p in state['bug_parents'] if p[:2] == selected['bug'][:2]]
        require(len(parents) == 1 and parents[0][2] == 2, peer + ' selected parent is not online')
        require(native['jobs']['queue_count'] == 0 and native['jobs']['jobs'] == [], peer + ' physical Job queue not empty')
        require(all(not d['job_present'] and not d['reciprocal'] for d in native['jobs']['developers']), peer + ' physical Developer Job edge remains')
        dev = [d for d in native['developers'] if d['id'] == 0]
        require(len(dev) == 1 and not dev[0]['job_link'] and dev[0]['state'] == 0, peer + ' Developer0 has not returned to idle')
        ui = uis[peer]
        require(ui['observed'] and not ui['locked'] and not any(b['bug'] == selected['bug'] for b in ui['known_bugs']), peer + ' selected Bug remains in actual Tracker')
    host, client = (final[p]['native'] for p in PEERS)
    require(host['ledger'] == client['ledger'], 'Full physical peer ledger differs')
    require(len(host['developers']) == 50 and host['developers'] == client['developers'], 'Full50 physical staff/pose/map differs')
    require(physical_jobs(host) == physical_jobs(client), 'Physical ordered Job semantics differ')
    require(host['jobs']['developers'] == client['jobs']['developers'], 'Physical reciprocal Developer graph differs')
    return {'job_fixed_before': selected['fixed'], 'job_fixed_after': work(final['host'])['job_fixed'],
            'counter_is_completion_requirement': False, 'selected_retired': True, 'parent_online': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-run-id', required=True)
    parser.add_argument('--prior-summary', type=Path, required=True)
    args = parser.parse_args()
    args.test_root = ROOT / '_MPTest/tutorial-two/_MPTest'
    args.players, args.cleanup, args.step_timeout, args.ready_timeout = 2, False, 45, 60
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'Read-only natural repair finalization; original observer and false counter oracle retained'
    try:
        runner.load_manifests()
        prior = read_json(args.prior_summary)
        require(runner.summary['runtime_run_id'] == args.expected_run_id == prior['runtime_run_id'], 'Owned prior runtime differs')
        digest = hash_file(ROOT / 'build/bin/Release/MT2_Multiplayer.dll')
        require(prior['dll_sha256'] == digest and all(read_json(p)['installed_binary_sha256']['MT2_Multiplayer.dll'] == digest for p in runner.manifests.values()), 'Prior/installed DLL differs')
        require(prior.get('error', {}).get('cause') == 'Natural repair observation bound reached without selected completion', 'Original observer has not ended at the expected counter-oracle bound')
        intents = prior['clock_intents']
        require(len(intents) == 2 and [x['fields']['scale'] for x in intents] == [9, 0] and all(x['state'] == 'confirmed' for x in intents), 'Original one unpause/pause not confirmed')
        require(all(v['assigned'] and v['debugging'] for v in prior['observations'].values()), 'Original actual assignment/debugging evidence absent')
        require(prior['final']['host']['frame'] == prior['final']['client-1']['frame'] and all(v['engine']['clock']['paused'] and v['engine']['clock']['scale'] == 0 for v in prior['final'].values()), 'Original final frames not synchronized and paused')
        final = {p: sample(runner, p, 'readonly-final', True) for p in PEERS}
        uis = {p: runner.single('readonly-tracker-' + p, p, 'game_read_native_bug_fix_ui') for p in PEERS}
        result = validate(final, uis, prior['selected'])
        runner.summary.update(passed=True, state='natural-selected-repair-readonly-confirmed', final=final, final_ui=uis, result=result, dll_sha256=digest,
                              prior={'path': str(args.prior_summary), 'sha256': hash_file(args.prior_summary), 'error': prior['error'], 'acceptance_id': prior['acceptance_id']})
    except Exception as error:
        runner.summary.update(passed=False, state='readonly-finalization-failed', error={'step': runner.summary['step'], 'cause': str(error)})
        print('error: ' + str(error))
    runner.summary['finished_utc'] = utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

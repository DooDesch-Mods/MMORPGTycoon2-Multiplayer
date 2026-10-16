"""Observe natural repair of one already assigned Bug in the current loaded world."""
import argparse
from pathlib import Path
import sys
import time

ROOT = next(p for p in Path(__file__).resolve().parents if (p / 'tools/local_acceptance.py').is_file())
sys.path.insert(0, str(ROOT / 'tools'))
from local_acceptance import AcceptanceError, AcceptanceRunner, hash_file, read_json, utc
from native_bug_completion_readback import sample, validate, work, PEERS, require
from e2e.subscriber_additions import set_clock_once

def baseline(samples, uis):
    require(samples['host']['frame'] == samples['client-1']['frame'], 'Paused continuation full frames differ')
    selected = {}
    for peer in PEERS:
        ui = uis[peer]
        require(ui['observed'] and not ui['locked'] and len(ui['known_bugs']) == 1, peer + ' expected one genuine known Bug')
        selected[peer] = ui['known_bugs'][0]
        require(not selected[peer]['schedule'], peer + ' known Bug is not already scheduled')
        jobs = samples[peer]['native']['jobs']['jobs']
        require(len(jobs) == 1, peer + ' continuation requires exactly one surviving Job and no aborted predecessor')
        job = jobs[0]
        require(job['type'] == 7 and job['valid'] and job['developer_present'] and job['developer_id'] == 0
                and job['developer_reciprocal'] and job['bug']['present'] and job['bug']['anchor_present']
                and job['bug']['known'] == 1 and job['bug']['manual'] == 1 and job['bug']['debugged'] == 1
                and job['bug']['job_reciprocal'] and job['bug']['runtime_id_local'] == selected[peer]['display_id'],
                peer + ' exact assigned debugging Bug/Job graph differs')
        actors = [d for d in samples[peer]['native']['developers'] if d['id'] == 0]
        require(len(actors) == 1 and actors[0]['state'] in (1, 2) and actors[0]['job_link'] and actors[0]['on_map'],
                peer + ' actual Developer0 is not assigned to the repair')
    identity = selected['host']['bug']
    require(identity == selected['client-1']['bug'], 'Shared selected Bug identity differs')
    require(len(samples['host']['frame']['bugs']) == 1 and samples['host']['frame']['bugs'][0][:6] == identity,
            'Continuation requires one exact live witnessed Bug')
    jobs = work(samples['host'])['bug_jobs']
    require(len(jobs) == 1 and jobs[0][:6] == identity and jobs[0][10:13] == [7, True, 0],
            'Continuation requires the exact assigned witnessed Job')
    return {'bug': identity, 'job': jobs[0][6:10], 'local_ids': {p: selected[p]['display_id'] for p in PEERS},
            'fixed': work(samples['host'])['job_fixed']}

def observe(sampled, selected, peer):
    frame = sampled['frame']
    state = work(sampled)
    jobs = [j for j in state['bug_jobs'] if j[6:10] == selected['job']]
    require(len(jobs) <= 1, 'Selected repair occurrence duplicated')
    parents = [p for p in state['bug_parents'] if p[:2] == selected['bug'][:2]]
    retired = not jobs and not any(b[:6] == selected['bug'] for b in frame['bugs'])
    complete = retired and len(parents) == 1 and parents[0][2] == 2
    physical = [j for j in sampled['native']['jobs']['jobs'] if j['type'] == 7 and j['valid'] and j['bug']['present'] and j['bug']['runtime_id_local'] == selected['local_ids'][peer]]
    assigned = bool(jobs and jobs[0][12] >= 0 and any(j['developer_id'] == jobs[0][12] and j['developer_present'] and j['developer_reciprocal'] and j['bug']['job_reciprocal'] for j in physical))
    return {'complete': complete, 'assigned': assigned, 'debugging': assigned and any(j['bug']['debugged'] == 1 for j in physical), 'selected_jobs': jobs, 'fixed': state['job_fixed']}


def execute(runner, args, clock=set_clock_once, now=time.monotonic):
    started = False
    try:
        samples = {p: sample(runner, p, 'baseline', True) for p in PEERS}
        uis = {p: runner.single('baseline-ui-' + p, p, 'game_read_native_bug_fix_ui') for p in PEERS}
        selected = baseline(samples, uis)
        runner.summary.update(baseline=samples, selected=selected, baseline_ui=uis)
        runner.persist()
        clock(runner, args.scale, 45)
        started = True
        seen = {p: {'assigned': True, 'debugging': True} for p in PEERS}
        deadline, index, previous = now() + args.wait_seconds, 0, None
        completed = False
        while now() < deadline:
            index += 1
            current = {p: sample(runner, p, 'natural-' + str(index)) for p in PEERS}
            states = {p: observe(current[p], selected, p) for p in PEERS}
            for peer in PEERS:
                for field in ('assigned', 'debugging'):
                    seen[peer][field] |= states[peer][field]
            signature = {p: {k: states[p][k] for k in ('assigned', 'debugging', 'complete', 'selected_jobs', 'fixed')} for p in PEERS}
            runner.summary.update(latest=current, observations=seen, polls=index)
            if signature != previous:
                runner.summary.setdefault('transitions', []).append({'utc': utc(), 'states': states})
                print('info: natural repair changed ' + str({p: {k: states[p][k] for k in ('assigned', 'debugging', 'complete')} for p in PEERS}), flush=True)
                previous = signature
            runner.persist()
            if all(state['complete'] for state in states.values()):
                completed = True
                break
        clock(runner, 0, 45)
        started = False
        # The shared clock receipt can precede the final state publication.
        # Only read while the paused Client finishes adopting that publication.
        deadline = now() + 45
        while True:
            final = {p: sample(runner, p, 'paused-final', True) for p in PEERS}
            runner.summary['final'] = final
            runner.persist()
            if final['host']['frame'] == final['client-1']['frame']:
                break
            require(now() < deadline, 'Paused complete Host/Client frame did not converge')
        require(completed, 'Natural repair observation bound reached without selected completion')
        uis = {p: runner.single('final-ui-' + p, p, 'game_read_native_bug_fix_ui') for p in PEERS}
        result = validate(final, uis, selected)
        runner.summary.update(final_ui=uis, result=result)
        for peer in PEERS:
            runner.capture(peer)
        runner.summary.update(passed=True, state='natural-selected-repair-confirmed-on-both-peers')
    except Exception as error:
        runner.summary.update(passed=False, state='failed-no-replay', error={'step': runner.summary['step'], 'cause': str(error)})
        print('error: ' + str(error), flush=True)
        intents = runner.summary.get('clock_intents', [])
        starts = [r for r in intents if r['fields']['scale'] == args.scale]
        stops = [r for r in intents if r['fields']['scale'] == 0]
        if not stops and (started or (starts and starts[-1]['state'] in ('receipt-confirmed', 'confirmed'))):
            try:
                clock(runner, 0, 45)
                runner.summary['clock_compensation'] = 'confirmed-paused-once'
            except Exception as pause_error:
                runner.summary['clock_compensation'] = {'state': 'failed-no-replay', 'cause': str(pause_error)}
                print('error: compensation pause failed: ' + str(pause_error), flush=True)
    runner.summary['finished_utc'] = utc()
    runner.persist()
    return runner.summary['passed']

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-run-id', required=True)
    parser.add_argument('--wait-seconds', type=int, choices=range(10, 601), default=180)
    args = parser.parse_args()
    args.scale = 9
    args.test_root = ROOT / '_MPTest/tutorial-two/_MPTest'
    args.players, args.cleanup, args.step_timeout, args.ready_timeout = 2, False, 45, 60
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'Current assigned Bug repair: one original scale9, bounded observation, one original pause and full peer readback'
    try:
        runner.load_manifests()
        require(runner.summary['runtime_run_id'] == args.expected_run_id, 'Owned run differs')
        digest = hash_file(ROOT / 'build/bin/Release/MT2_Multiplayer.dll')
        require(all(read_json(p)['installed_binary_sha256']['MT2_Multiplayer.dll'] == digest for p in runner.manifests.values()), 'Installed candidate differs')
        runner.summary['dll_sha256'] = digest
        passed = execute(runner, args)
    except Exception as error:
        runner.summary.update(passed=False, state='preflight-failed-no-mutation', error={'step': runner.summary['step'], 'cause': str(error)})
        runner.persist()
        passed = False
    print(runner.summary_path, flush=True)
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())

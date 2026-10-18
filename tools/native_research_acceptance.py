"""Prove ordinary Client Research through the existing Host-ordered UI actions."""
import argparse
import copy
from functools import partial
from pathlib import Path
import sys
import time
import uuid

ROOT = next(p for p in Path(__file__).resolve().parents if (p / 'tools/local_acceptance.py').is_file())
sys.path.insert(0, str(ROOT / 'tools'))
from local_acceptance import AcceptanceError, AcceptanceRunner, compare_ledgers, hash_file, read_json, utc
from lobby_ui.native_local_acceptance import ready
from native_bug_completion_readback import sample, physical_jobs, require, PEERS
from native_time_once import set_clock_once

# An indoor Research project need not have outdoor construction or Bug Jobs.
sample = partial(sample, require_bug_work=False, extended_frames=True)

KEY = 'bridges'
# Original implementation completion is work / (Tech.devHours * 3600) >= 1.
WORK_REQUIRED = 21600
SHARED_FIELDS = ('project', 'startingpoint', 'startingpoint_release', 'research')


def without_scan_stamp(value, field):
    # Release scans advance this local stamp even when no feature is added.
    return {key: item for key, item in value.items() if key != field}


def research_state(value):
    return without_scan_stamp(value, 'release_counter')


def development_state(value):
    return {field: without_scan_stamp(value[field], 'mutation_counter')
            if field == 'startingpoint_release' else value[field] for field in SHARED_FIELDS}


def indexed_bugs(rows):
    require(isinstance(rows, list) and all(isinstance(row, list) and len(row) == 15 for row in rows),
            'Commit requires complete15-field Bug rows')
    result = {tuple(row[:6]): row for row in rows}
    require(len(result) == len(rows), 'Commit Bug identities duplicated')
    return result


def bug_changes(before, after, action):
    old, new = indexed_bugs(before), indexed_bugs(after)
    added = new.keys() - old.keys()
    require(all(identity[2] == 1 and identity[3] == action for identity in added),
            'Commit new Bug birth is not bound to its action')
    return {'added': [new[key] for key in added],
            'changed': [new[key] for key in old.keys() & new.keys() if old[key] != new[key]],
            'removed': [old[key] for key in old.keys() - new.keys()]}


def bug_delta(before, after, action):
    delta = bug_changes(before, after, action)
    return {'before': len(before), 'after': len(after), 'added': len(delta['added']),
            'changed': len(delta['changed']), 'retired': len(delta['removed'])}


class Research:
    def __init__(self, runner, args, clock=set_clock_once, now=time.monotonic):
        self.runner, self.args, self.clock, self.now = runner, args, clock, now

    def current(self, peer, label):
        development = self.runner.single(label + '-development-' + peer, peer, 'game_read_tutorial_development')
        progress = self.runner.single(label + '-progress-' + peer, peer, 'game_read_tutorial_progress')
        require(development.get('schema') == 1 and development.get('mainthread_confirmed') is True
                and development.get('lifetime_confirmed') is True, peer + ' incomplete native development read')
        row = progress.get('tutorial', {}).get('research')
        require(isinstance(row, dict) and row.get('key') == KEY, peer + ' actual Bridges row absent')
        return {'development': development, 'row': row, 'progress': progress}

    def context(self, label):
        values = {peer: self.runner.single(label + '-' + peer, peer, 'game_read_native_lobby') for peer in PEERS}
        require(all(ready(value) and value['controller'].get('pending') is False for value in values.values()),
                label + ' requires two Playing peers and idle Controllers')
        require(values['host']['prepare']['snapshot_id'] == values['client-1']['prepare']['snapshot_id'],
                label + ' shared snapshot differs')
        return {peer: value['controller'] for peer, value in values.items()}

    def paused(self, label):
        deadline = self.now() + 60
        while True:
            values = {peer: self.current(peer, label) for peer in PEERS}
            native = {peer: sample(self.runner, peer, label, True) for peer in PEERS}
            left, right = (values[peer] for peer in PEERS)
            shared = (research_state(left['row']) == research_state(right['row'])
                      and development_state(left['development']) == development_state(right['development'])
                      and native['host']['frame'] == native['client-1']['frame']
                      and all(research_state(native[peer]['frame']['tutorial']['research'])
                              == research_state(values[peer]['row']) for peer in PEERS))
            if shared:
                ledger = compare_ledgers(native['host']['engine'], native['client-1']['engine'])
                self.context(label + '-context')
                value = {'peers': values, 'native': native, 'ledger': ledger}
                self.runner.checked(label, value)
                return value
            require(self.now() < deadline, label + ' paused authoritative state did not converge within60s')

    def operation(self, name, development, read_only=False):
        request = {'token': str(uuid.uuid4()), 'native_origin_uuid': development['snapshot_uuid'],
                   'world_epoch': development['world_epoch'], 'world_generation': development['world_generation'],
                   'operation': name, 'expected_development': development}
        intent = {'operation': name, 'request': request, 'state': 'attempted'}
        self.runner.summary.setdefault('ui_intents', []).append(intent)
        self.runner.persist()
        result = self.runner.single(name + '-once', 'client-1', 'game_tutorial_startingpoint', request)
        intent['result'] = result
        require(result.get('token') == request['token'] and result.get('unknown') is False,
                name + ' native outcome/token differs; no replay')
        require(result.get('returned') is True and result.get('confirmed') is True
                and result.get('attempted') is (not read_only), name + ' native acknowledgement incomplete; no replay')
        if read_only:
            require(result.get('read_only') is True, name + ' lacks read-only acknowledgement')
        intent['state'] = 'confirmed-native-return'
        self.runner.persist()
        return result

    def view(self, name, state_name):
        current = self.current('client-1', name)
        state = self.operation(state_name, current['development'], True)
        if state.get('ready') is True:
            return state
        require(state.get('ready') is False, state_name + ' missing original page state')
        shown = self.operation(name, current['development'])
        require(shown.get('page_requested') is True, name + ' original page request not confirmed')
        deadline = self.now() + 45
        while True:
            state = self.operation(state_name, current['development'], True)
            if state.get('ready') is True:
                return state
            require(state.get('ready') is False and self.now() < deadline,
                    state_name + ' original page/layout not ready within45s: ' + str(state.get('cause')))

    def confirm(self, name, before, result):
        action = result.get('action_uuid')
        try:
            require(result.get('submitted') is True and str(uuid.UUID(action)) == action,
                    name + ' submitted action UUID absent; no replay')
        except (ValueError, TypeError, AttributeError) as error:
            raise AcceptanceError(name + ' invalid action UUID; no replay: ' + str(error)) from error
        deadline = self.now() + 60
        while True:
            contexts = {peer: self.runner.single(name + '-result-' + peer, peer, 'game_read_native_lobby') for peer in PEERS}
            for peer, context in contexts.items():
                state = context['controller']
                require(not state.get('locked') and state.get('failed') == before[peer]['failed'],
                        name + ' failed on ' + peer + ': ' + str(state.get('cause')))
                if state.get('pending') is True and state.get('outcome') == 0:
                    require(context['prepare'].get('playing') is True and not context['prepare'].get('error')
                            and context['snapshot'].get('world_authority_valid') is True,
                            name + ' lost pending world/session on ' + peer)
                else:
                    require(ready(context), name + ' lost Playing on ' + peer)
            if all(value['controller'].get('action_uuid') == action and value['controller'].get('pending') is False
                   and value['controller'].get('outcome') == 1 for value in contexts.values()):
                break
            require(self.now() < deadline, name + ' result timed out within60s; no replay')
        for peer, context in contexts.items():
            for field in ('confirmed', 'attempted', 'host_revision'):
                require(context['controller'].get(field) == before[peer][field] + 1,
                        name + ' expected exactly one ' + field + ' on ' + peer)
        host = contexts['host']['controller']
        require(host['required_mask'] > 0 and host['ack_mask'] == host['required_mask'], name + ' Host ACK incomplete')
        self.runner.checked(name + '-confirmed-on-both', {'action_uuid': action, 'controllers': contexts})
        return action

    def unchanged_local_ledger(self, name, result, before):
        for field in ('ledger_before', 'ledger_after'):
            require(all(result.get(field, {}).get(key) == value for key, value in before.items()),
                    name + ' altered Client ledger before Host result: ' + field)

    def commit_proofs(self, baseline, after, action):
        proofs = {}
        snapshot = after['native']['host']['frame']['snapshot_uuid']
        for peer in PEERS:
            local = after['peers'][peer]
            value = local['progress'].get('research_commit_proof')
            require(isinstance(value, dict) and value.get('confirmed') is True and value.get('key') == KEY
                    and value.get('action_uuid') == action and value.get('snapshot_uuid') == snapshot
                    and value.get('native_origin_uuid') == local['development']['snapshot_uuid'],
                    peer + ' native Commit proof identity/confirmation differs')
            require(not local['progress'].get('research_commit_receipt_cause'),
                    peer + ' native Commit receipt failed: ' + str(local['progress'].get('research_commit_receipt_cause')))
            release = value.get('release', {})
            require(release.get('preserved') is True, peer + ' native prior Release members not preserved')
            require(type(release.get('scan_stamp_before')) is int and release['scan_stamp_before'] >= 0
                    and type(release.get('scan_stamp_after')) is int
                    and release['scan_stamp_after'] == release['scan_stamp_before'] + 1,
                    peer + ' native Commit must advance its own scan stamp exactly once')
            for field in ('features_before', 'features_after', 'minor_before', 'minor_after', 'members_before', 'members_after'):
                require(type(release.get(field)) is int and release[field] >= 0, peer + ' invalid native Release count: ' + field)
            require(release['features_before'] == baseline['peers'][peer]['row']['release_change_count']
                    and release['features_after'] == after['peers'][peer]['row']['release_change_count']
                    and release['minor_after'] == release['minor_before']
                    and release['members_after'] == release['members_before'] + 1,
                    peer + ' native Release preservation counts differ')
            proofs[peer] = value
        semantic_release = lambda value: {key: item for key, item in value.items()
                                          if key not in ('scan_stamp_before', 'scan_stamp_after')}
        require(semantic_release(proofs['host']['release']) == semantic_release(proofs['client-1']['release']),
                'Peer Release preservation receipts differ')
        before_bugs = baseline['native']['host']['frame']['bugs']
        after_bugs = after['native']['host']['frame']['bugs']
        expected = bug_changes(before_bugs, after_bugs, action)
        actual = proofs['host'].get('map_bugs', {})
        require(actual.get('observed') is True and actual.get('before_count') == len(before_bugs)
                and actual.get('after_count') == len(after_bugs), 'Host native Commit map Bug counts differ')
        for field in ('added', 'changed', 'removed'):
            require(indexed_bugs(actual.get(field)) == indexed_bugs(expected[field]),
                    'Host native Commit map Bug delta differs: ' + field)
        require('map_bugs' not in proofs['client-1'], 'Client Commit includes a second native map Bug operation')
        self.runner.summary['native_commit_proofs'] = proofs

    def start(self):
        baseline = self.paused('start-baseline')
        row = baseline['peers']['host']['row']
        require(not any(row[field] for field in ('project_present', 'purchased', 'deployed', 'custom')),
                'Start requires unpurchased Bridges and no project; no action replay')
        self.view('client_bridges_show', 'client_bridges_state')
        before = self.context('before-start')
        current = self.current('client-1', 'start-current')
        result = self.operation('client_bridges_start', current['development'])
        require(result.get('selection_attempted') is True and result.get('selection_confirmed') is True
                and result.get('activation_attempted') is True and result.get('price') == 10000,
                'Start lacks original selection/activation/price; no replay')
        self.unchanged_local_ledger('Start', result, baseline['ledger'])
        self.confirm('start', before, result)
        after = self.paused('start-after')
        actual = after['peers']['host']['row']
        require(actual['project_present'] and actual['purchased'] and not actual['deployed']
                and not actual['custom'] and actual['inventory_count'] == row['inventory_count'],
                'Start actual purchased project/inventory differs')
        require(all(actual[field] == row[field] for field in ('release_change_count', 'release_feature_count')),
                'Start changed existing Release')
        expected = copy.deepcopy(baseline['ledger'])
        expected['cash'] -= 10000
        expected['record_cash'] -= 10000
        expected['record_expense'] += 10000
        expected['line_counts'][3] += 1
        expected['line_income'][3] -= 10000
        require(after['ledger'] == expected, 'Start full ledger differs from one category3 purchase')
        self.runner.summary['start_confirmed'] = True

    def workers(self):
        baseline = self.paused('workers-baseline')
        row = baseline['peers']['host']['row']
        require(row['project_present'] and row['purchased'] and not row['deployed'] and row['desired_workers'] != 1,
                'Workers requires current project and a different allocation; no pip replay')
        self.view('client_bridges_staff_show', 'client_bridges_staff_state')
        before = self.context('before-workers')
        current = self.current('client-1', 'workers-current')
        result = self.operation('client_bridges_staff_one', current['development'])
        require(result.get('activation_attempted') is True and result.get('desired_workers') == 1,
                'Workers lacks original pip activation')
        self.unchanged_local_ledger('Workers', result, baseline['ledger'])
        self.confirm('workers', before, result)
        after = self.paused('workers-after')
        require(after['peers']['host']['row'] == row | {'desired_workers': 1}, 'Workers changed other Research fields')
        require(after['ledger'] == baseline['ledger'], 'Workers changed full ledger')
        self.runner.summary['workers_confirmed'] = True

    def complete(self):
        baseline = self.paused('work-baseline')
        row = baseline['peers']['host']['row']
        require(row['project_present'] and row['purchased'] and not row['deployed'] and row['desired_workers'] == 1,
                'Natural work requires purchased Bridges and one assigned worker')
        if row['work'] < WORK_REQUIRED:
            self.clock(self.runner, 9, 45)
            deadline = self.now() + self.args.wait_seconds
            while True:
                current = self.current('host', 'natural-work')
                require(current['row']['project_present'] and current['row']['key'] == KEY,
                        'Natural work lost current Bridges project')
                for peer in PEERS:
                    sample(self.runner, peer, 'natural-work')
                self.runner.summary['latest_work'] = current
                self.runner.persist()
                if current['row']['work'] >= WORK_REQUIRED:
                    break
                require(self.now() < deadline,
                        'Natural implementation incomplete within' + str(self.args.wait_seconds)
                        + 's: work=' + str(current['row']['work']) + ' required=' + str(WORK_REQUIRED))
            self.clock(self.runner, 0, 45)
        after = self.paused('work-after')
        require(after['peers']['host']['row']['work'] >= WORK_REQUIRED, 'Paused native implementation incomplete')
        self.runner.summary['natural_work_confirmed'] = True

    def profile(self):
        scale = getattr(self.args, 'profile_scale', 9)
        require(scale in (1, 9), 'Profile scale must be normal1 or fast9')
        baseline = self.paused('profile-baseline')
        row = baseline['peers']['host']['row']
        require(row['project_present'] and row['purchased'] and not row['deployed']
                and row['desired_workers'] == 1, 'Profile requires the retained one-worker Research project')
        self.clock(self.runner, scale, 45)
        for _ in range(3):
            current = self.current('host', 'natural-work')
            require(current['row']['project_present'] and current['row']['key'] == KEY,
                    'Profile lost current Bridges project')
            for peer in PEERS:
                sample(self.runner, peer, 'natural-work')
            self.runner.summary['latest_work'] = current
            self.runner.persist()
        require(current['row']['work'] > row['work'], 'Profile did not observe active Research work')
        self.clock(self.runner, 0, 45)
        self.paused('profile-after')
        self.runner.summary['profile_confirmed'] = True

    def commit(self):
        baseline = self.paused('commit-baseline')
        row = baseline['peers']['host']['row']
        require(row['project_present'] and row['purchased'] and not row['deployed'] and row['work'] >= WORK_REQUIRED,
                'Commit requires the naturally implemented Bridges project; no commit replay')
        current = self.current('client-1', 'commit-window')
        state = self.operation('client_bridges_staff_state', current['development'], True)
        visible = state.get('ui', {}).get('window_visible')
        require(type(visible) is int and visible in (0, 1), 'Commit window visibility acknowledgement absent')
        if visible == 0:
            shown = self.operation('client_bridges_staff_show', current['development'])
            require(shown.get('page_requested') is True, 'Commit window original Show/page request not confirmed')
            current = self.current('client-1', 'commit-window-after-show')
            state = self.operation('client_bridges_staff_state', current['development'], True)
            require(state.get('ui', {}).get('window_visible') == 1,
                    'Commit window is not visible after confirmed original Show; no Commit attempted')
        before = self.context('before-commit')
        current = self.current('client-1', 'commit-current')
        result = self.operation('client_bridges_commit', current['development'])
        self.finish_commit(baseline, before, result)

    def finish_commit(self, baseline, before, result):
        # The Commit fixture checks its local ledger before returning submission.
        # Its response has no ledger echo; compare the complete final ledger below.
        row = baseline['peers']['host']['row']
        action = self.confirm('commit', before, result)
        after = self.paused('commit-after')
        actual = after['peers']['host']['row']
        require(not actual['project_present'] and actual['purchased'] and actual['deployed'] and not actual['custom'],
                'Commit actual deployed/retired Research differs')
        require(all(actual[field] == row[field] + 1 for field in
                    ('inventory_count', 'release_change_count', 'release_feature_count')),
                 'Commit requires exactly one inventory item and Release Change/feature')
        require(actual['commit_bugs'] == {'total': row['bugs_total'], 'known': row['bugs_known']},
                'Commit exact project Bug receipt differs')
        self.runner.summary['commit_bug_delta'] = bug_delta(baseline['native']['host']['frame']['bugs'],
                                                          after['native']['host']['frame']['bugs'], action)
        self.commit_proofs(baseline, after, action)
        require(after['ledger'] == baseline['ledger'], 'Commit changed full native ledger')
        initial = baseline['peers']['host']['development']
        final = after['peers']['host']['development']
        require(initial['startingpoint'] == final['startingpoint'], 'Commit changed unrelated Startingpoint technology/inventory')
        require(initial['startingpoint_release']['feature_count'] == final['startingpoint_release']['feature_count'],
                'Commit lost the prior Startingpoint Release feature')
        host, client = (after['native'][peer]['native'] for peer in PEERS)
        require(host['ledger'] == client['ledger'], 'Final physical ledger differs')
        require(host['developers'] == client['developers']
                and all(after['native'][peer]['native']['developers']
                        == baseline['native'][peer]['native']['developers'] for peer in PEERS),
                'Final physical staff differ from peers or baseline')
        require(physical_jobs(host) == physical_jobs(client)
                and host['jobs']['developers'] == client['jobs']['developers'], 'Final physical Job graph differs')
        self.runner.summary['commit_confirmed'] = True
        self.runner.summary['commit_result'] = result

    def execute(self):
        try:
            phases = ('start', 'workers', 'complete', 'commit') if self.args.phase == 'all' else (self.args.phase,)
            for phase in phases:
                getattr(self, phase)()
                self.runner.persist()
            for peer in PEERS:
                self.runner.capture(peer)
            self.runner.summary.update(passed=True, state='native-research-' + self.args.phase + '-confirmed')
        except Exception as error:
            self.runner.summary.update(passed=False, state='failed-no-replay',
                                       error={'step': self.runner.summary['step'], 'cause': str(error)})
            print('error: ' + str(error), flush=True)
            intents = self.runner.summary.get('clock_intents', [])
            starts = [row for row in intents if row['fields']['scale'] > 0]
            stops = [row for row in intents if row['fields']['scale'] == 0]
            if starts and starts[-1]['state'] in ('confirmed', 'receipt-confirmed') and not stops:
                try:
                    self.clock(self.runner, 0, 45)
                    self.runner.summary['clock_compensation'] = 'confirmed-paused-once'
                except Exception as pause_error:
                    self.runner.summary['clock_compensation'] = {'state': 'failed-no-replay', 'cause': str(pause_error)}
                    print('error: compensation pause failed: ' + str(pause_error), flush=True)
        self.runner.summary['finished_utc'] = utc()
        self.runner.persist()
        return self.runner.summary['passed']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-run-id', required=True)
    parser.add_argument('--phase', choices=('all', 'start', 'workers', 'complete', 'commit', 'profile'), required=True)
    parser.add_argument('--wait-seconds', type=int, choices=range(10, 601), default=300)
    parser.add_argument('--profile-scale', type=int, choices=(1, 9), default=9)
    args = parser.parse_args()
    args.test_root = ROOT / '_MPTest/tutorial-two/_MPTest'
    args.players, args.cleanup, args.step_timeout, args.ready_timeout = 2, False, 45, 60
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'Ordinary Bridges Client UI, once Host actions, natural work and actual two-peer readback'
    try:
        runner.load_manifests()
        require(runner.summary['runtime_run_id'] == args.expected_run_id, 'Owned current runtime differs')
        digest = hash_file(ROOT / 'build/bin/Release/MT2_Multiplayer.dll')
        require(all(read_json(path)['installed_binary_sha256']['MT2_Multiplayer.dll'] == digest
                    for path in runner.manifests.values()), 'Installed candidate differs')
        runner.summary['dll_sha256'] = digest
        passed = Research(runner, args).execute()
    except Exception as error:
        runner.summary.update(passed=False, state='preflight-failed-no-mutation',
                              error={'step': runner.summary['step'], 'cause': str(error)})
        runner.persist()
        passed = False
    print(runner.summary_path, flush=True)
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())

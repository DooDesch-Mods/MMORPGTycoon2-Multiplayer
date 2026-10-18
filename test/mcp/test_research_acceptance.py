"""Exercise the actual acceptance sequence with synthetic MCP acknowledgements."""
import copy
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
import uuid

spec = importlib.util.spec_from_file_location('research_driver', Path(__file__).resolve().parents[2] / 'tools/native_research_acceptance.py')
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class Runner:
    def __init__(self, mode='pass', phase='all'):
        self.mode = mode
        self.summary = {'step': 'synthetic', 'passed': False}
        self.calls, self.captures, self.scales = [], [], []
        self.inspections = []
        self.pending = None
        self.clock_scale = 0
        self.work_seen = False
        self.development_visible = mode not in ('closed-pages', 'closed-commit-window', 'commit-show-unknown')
        self.last_action = ''
        self.staff_count = 3 if mode in ('small-staff', 'both-staff-drift') else 50
        self.prior_features = 1 if mode in ('nonempty-release', 'lost-old-feature') else 0
        self.controllers = {p: {'pending': False, 'locked': False, 'failed': 0, 'confirmed': 0,
                               'attempted': 0, 'host_revision': 0, 'outcome': 1, 'action_uuid': '',
                               'required_mask': 3, 'ack_mask': 3} for p in driver.PEERS}
        self.row = {'key': 'bridges', 'project_present': False, 'purchased': False, 'deployed': False,
                    'custom': False, 'work': 0, 'lines': 0, 'desired_workers': 0, 'bugs_total': 0,
                    'bugs_known': 0, 'inventory_count': 0, 'release_change_count': self.prior_features,
                    'release_feature_count': 0, 'release_counter': self.prior_features, 'commit_bugs': None}
        self.ledger = {'cash': 88160, 'record_present': True, 'record_day': 1, 'record_cash': 88160,
                       'record_income': 0, 'record_expense': 0, 'line_counts': [0] * 20, 'line_income': [0] * 20}
        if phase in ('workers', 'complete', 'commit', 'profile'):
            self.apply('client_bridges_start')
        if phase in ('complete', 'commit', 'profile'):
            self.apply('client_bridges_staff_one')
        if phase == 'commit' or mode == 'already-complete':
            self.finish_work()

    def persist(self):
        pass

    def checked(self, name, result):
        self.summary.setdefault('checks', []).append({'check': name, 'result': copy.deepcopy(result)})

    def capture(self, peer):
        self.captures.append(peer)

    def finish_work(self):
        self.row.update(work=21600, lines=120, bugs_total=2, bugs_known=1)

    def development(self, peer='host'):
        row = self.row
        return {'schema': 1, 'mainthread_confirmed': True, 'lifetime_confirmed': True,
                'snapshot_uuid': '00000000-0000-0000-0000-000000000001', 'world_epoch': 1, 'world_generation': 1,
                'project': {'present': row['project_present'], 'kind': 0 if row['project_present'] else None,
                            'tech_key': 'bridges' if row['project_present'] else None,
                            **{k: row[k] for k in ('work', 'lines', 'desired_workers', 'bugs_total', 'bugs_known')}},
                'startingpoint': {'key': 'startingpoint', 'purchased': True, 'deployed': True, 'inventory_count': 1},
                'startingpoint_release': {'change_count': row['release_change_count'],
                                         'mutation_counter': row['release_counter'] + (120 if peer == 'client-1' and self.mode == 'moving-stamps' else 0),
                                         'feature_count': 0 if self.mode == 'lost-old-feature' and row['deployed'] else self.prior_features},
                'research': {'kind': 0, 'project_present': row['project_present'], 'rows': []}}

    def apply(self, operation):
        if operation == 'client_bridges_start':
            self.row.update(project_present=True, purchased=True)
            amount = 10001 if self.mode == 'wrong-price' else 10000
            self.ledger['cash'] -= amount
            self.ledger['record_cash'] -= amount
            self.ledger['record_expense'] += amount
            self.ledger['line_counts'][3] += 1
            self.ledger['line_income'][3] -= amount
        elif operation == 'client_bridges_staff_one':
            self.row['desired_workers'] = 1
        elif operation == 'client_bridges_commit':
            bugs = {k: self.row['bugs_' + k] for k in ('total', 'known')}
            self.row.update(project_present=False, deployed=True, work=0, lines=0, desired_workers=0,
                            bugs_total=0, bugs_known=0, commit_bugs=bugs)
            for field in ('inventory_count', 'release_change_count', 'release_feature_count', 'release_counter'):
                self.row[field] += 1
            if self.mode == 'commit-ledger-drift':
                self.ledger['cash'] -= 1
                self.ledger['record_cash'] -= 1

    def inspect(self, label, peer):
        self.inspections.append((label, peer))
        frame = {'schema': 12, 'snapshot_uuid': '00000000-0000-0000-0000-000000000003',
                 'tutorial': {'research': copy.deepcopy(self.row)}, 'bugs': [],
                 'developers': {'count': 1, 'indoor': list(range(self.staff_count)), 'outdoor': [], 'rows': []}}
        extended = self.mode in ('current-frame', 'future-frame', 'missing-marketing', 'unknown-marketing')
        if extended:
            frame.update(schema=17 if self.mode == 'future-frame' else 16, vips={}, marketing={})
            if self.mode == 'missing-marketing':
                del frame['marketing']
        scale = self.clock_scale if peer == 'host' else 0
        if self.mode == 'client-simulates':
            scale = 9 if peer == 'client-1' else scale
        cause = 'original Source capture failure' if self.mode == 'source-failure' and self.clock_scale else ''
        return {'typed_actions': {'initialized': True, 'started': True, 'locked': False, 'pending': False,
                                  'vip_frame': {'armed': extended},
                                  'marketing_frame': {'unknown': self.mode == 'unknown-marketing'},
                                  'failed': 0, 'live_state': {'active': True, 'locked': False, 'confirmed': True, 'cause': cause},
                                  'native_economy': {'day': {'frame': frame}}},
                'clock': {'scale': scale, 'paused': scale == 0, 'day': 1}, 'simulation': copy.deepcopy(self.ledger)}

    def single(self, label, peer, tool, fields=None):
        if tool == 'game_read_tutorial_development':
            if self.clock_scale and peer == 'host' and not self.work_seen:
                self.work_seen = True
                if self.mode == 'profile-partial':
                    self.row['work'] = 1000
                elif self.mode == 'profile-stalled':
                    self.row['work'] = 0
                else:
                    self.finish_work()
            value = copy.deepcopy(self.development(peer))
            if self.mode == 'different-project' and peer == 'client-1':
                value['project']['lines'] += 1
            if self.mode == 'different-release' and peer == 'client-1':
                value['startingpoint_release']['feature_count'] += 1
            return value
        if tool == 'game_read_tutorial_progress':
            value = {'tutorial': {'research': copy.deepcopy(self.row)}}
            if self.mode == 'moving-stamps':
                value['tutorial']['research']['release_counter'] += (440 if peer == 'host' else 560)
            if self.row['deployed']:
                release = {'preserved': self.mode != 'release-receipt-false',
                           'features_before': self.prior_features, 'features_after': self.prior_features + 1, 'minor_before': 0,
                           'minor_after': 0, 'members_before': self.prior_features, 'members_after': self.prior_features + 1,
                           'scan_stamp_before': 440 if peer == 'host' else 560,
                           'scan_stamp_after': 441 if peer == 'host' else 561}
                if self.mode == 'wrong-local-stamp' and peer == 'client-1':
                    release['scan_stamp_after'] += 1
                proof = {'key': 'bridges', 'confirmed': True, 'action_uuid': self.last_action,
                         'snapshot_uuid': '00000000-0000-0000-0000-000000000003',
                         'native_origin_uuid': '00000000-0000-0000-0000-000000000001', 'release': release}
                if self.mode == 'receipt-wrong-action':
                    proof['action_uuid'] = str(uuid.uuid4())
                if peer == 'host':
                    proof['map_bugs'] = {'observed': True, 'before_count': 1 if self.mode == 'map-receipt-wrong' else 0,
                                         'after_count': 0, 'added': [], 'changed': [], 'removed': []}
                value['research_commit_proof'] = proof
            return value
        if tool == 'game_read_native_developers':
            staff = [{'id': i, 'state': 0, 'job_link': False} for i in range(self.staff_count)]
            if self.mode == 'both-staff-drift' and self.row['deployed']:
                staff[-1]['state'] = 1
            if self.mode == 'physical-staff' and self.row['deployed'] and peer == 'client-1':
                staff[-1]['state'] = 1
            return {'observed': True, 'developer_movement': {'observed': True}, 'developers': staff,
                    'ledger': copy.deepcopy(self.ledger), 'jobs': {'queue_count': 0, 'jobs': [], 'developers': []}}
        if tool == 'game_read_native_lobby':
            if self.pending:
                operation, action = self.pending
                self.last_action = action
                self.pending = None
                self.apply(operation)
                for value in self.controllers.values():
                    value.update(action_uuid=action, outcome=1, pending=False)
                    for field in ('attempted', 'confirmed', 'host_revision'):
                        value[field] += 2 if self.mode == 'duplicate' and operation == 'client_bridges_staff_one' else 1
            context = {'prepare': {'snapshot_id': 'shared', 'playing': True},
                       'snapshot': {'world_authority_valid': True}, 'controller': copy.deepcopy(self.controllers[peer])}
            # The runner uses the production ready predicate; only this leaf is
            # replaced below because the synthetic fixture is not a full lobby.
            return context
        if tool == 'game_tutorial_startingpoint':
            operation = fields['operation']
            self.calls.append(operation)
            if operation.endswith('_show'):
                self.development_visible = True
            readonly = operation.endswith('_state')
            value = {'token': fields['token'], 'unknown': False, 'attempted': not readonly,
                     'returned': True, 'confirmed': True, 'read_only': readonly, 'ready': True,
                     'page_requested': True, 'ui': {'window_visible': int(self.development_visible)}}
            if operation == 'client_bridges_staff_state' and self.row['work'] >= driver.WORK_REQUIRED:
                value['ready'] = False
            if self.mode == 'commit-no-visibility' and operation == 'client_bridges_staff_state':
                value['ui'] = {}
            if self.mode == 'commit-show-unknown' and operation == 'client_bridges_staff_show':
                value['unknown'] = True
            if self.mode == 'closed-pages' and readonly:
                value['ready'] = operation.replace('_state', '_show') in self.calls
            if operation in ('client_bridges_start', 'client_bridges_staff_one', 'client_bridges_commit'):
                action = str(uuid.uuid4())
                self.pending = operation, action
                value.update(action_uuid=action, submitted=True, activation_attempted=True, selection_attempted=True,
                             selection_confirmed=True, price=10000, desired_workers=1)
                if operation != 'client_bridges_commit':
                    value.update(ledger_before={'clock_day': 1, **copy.deepcopy(self.ledger)},
                                 ledger_after={'clock_day': 1, **copy.deepcopy(self.ledger)})
            return value
        raise AssertionError('Unexpected tool: ' + tool)

    def clock(self, runner, scale, timeout):
        self.scales.append(scale)
        intent = {'fields': {'scale': scale}, 'state': 'confirmed'}
        self.summary.setdefault('clock_intents', []).append(intent)
        if self.mode == 'pause-failure' and scale == 0:
            intent['state'] = 'attempted'
            raise driver.AcceptanceError('original pause failure')
        self.clock_scale = scale


class Tests(unittest.TestCase):
    def test_closed_commit_window_is_shown_once_before_commit(self):
        runner, passed = self.run_case('closed-commit-window', 'commit')
        self.assertTrue(passed, runner.summary.get('error'))
        self.assertEqual(runner.calls.count('client_bridges_staff_show'), 1)
        self.assertEqual(runner.calls.count('client_bridges_commit'), 1)
        self.assertLess(runner.calls.index('client_bridges_staff_show'), runner.calls.index('client_bridges_commit'))

    def test_visible_completed_window_does_not_replay_show(self):
        runner, passed = self.run_case(phase='commit')
        self.assertTrue(passed, runner.summary.get('error'))
        self.assertNotIn('client_bridges_staff_show', runner.calls)
        self.assertEqual(runner.calls.count('client_bridges_commit'), 1)
        self.assertNotIn('ledger_before', runner.summary['commit_result'])
        self.assertNotIn('ledger_after', runner.summary['commit_result'])

    def test_commit_without_ledger_echo_still_rejects_actual_ledger_drift(self):
        runner, passed = self.run_case('commit-ledger-drift', 'commit')
        self.assertFalse(passed)
        self.assertEqual(runner.calls.count('client_bridges_commit'), 1)
        self.assertIn('Commit changed full native ledger', runner.summary['error']['cause'])

    def test_unknown_window_show_prevents_commit(self):
        runner, passed = self.run_case('commit-show-unknown', 'commit')
        self.assertFalse(passed)
        self.assertEqual(runner.calls.count('client_bridges_staff_show'), 1)
        self.assertNotIn('client_bridges_commit', runner.calls)

    def test_missing_window_visibility_prevents_commit(self):
        runner, passed = self.run_case('commit-no-visibility', 'commit')
        self.assertFalse(passed)
        self.assertNotIn('client_bridges_staff_show', runner.calls)
        self.assertNotIn('client_bridges_commit', runner.calls)

    def test_closed_original_pages_are_opened_once(self):
        runner, passed = self.run_case('closed-pages')
        self.assertTrue(passed, runner.summary.get('error'))
        self.assertEqual(runner.calls.count('client_bridges_show'), 1)
        self.assertEqual(runner.calls.count('client_bridges_staff_show'), 1)

    def test_bug_completion_still_requires_its_work_graph(self):
        from native_bug_completion_readback import sample as bug_sample
        with self.assertRaisesRegex(driver.AcceptanceError, 'complete Work state absent'):
            bug_sample(Runner(), 'host', 'bug-work-required', True)

    def run_case(self, mode='pass', phase='all', profile_scale=9):
        runner = Runner(mode, phase)
        ticks = iter(range(1000))
        actual_ready = driver.ready
        driver.ready = lambda state: state['prepare']['playing'] and not state['controller']['locked']
        try:
            value = driver.Research(runner, SimpleNamespace(phase=phase, wait_seconds=300, profile_scale=profile_scale),
                                    clock=runner.clock, now=lambda: next(ticks)).execute()
        finally:
            driver.ready = actual_ready
        return runner, value

    def test_all_real_runner_phases_once(self):
        runner, passed = self.run_case()
        self.assertTrue(passed, runner.summary.get('error'))
        for operation in ('client_bridges_start', 'client_bridges_staff_one', 'client_bridges_commit'):
            self.assertEqual(runner.calls.count(operation), 1)
        self.assertEqual(runner.scales, [9, 0])
        self.assertEqual(runner.captures, ['host', 'client-1'])
        self.assertEqual(runner.row['commit_bugs'], {'total': 2, 'known': 1})

    def test_accepted_start_is_not_replayed_for_worker_phase(self):
        runner, passed = self.run_case(phase='workers')
        self.assertTrue(passed, runner.summary.get('error'))
        self.assertNotIn('client_bridges_start', runner.calls)
        self.assertEqual(runner.calls.count('client_bridges_staff_one'), 1)

    def test_existing_release_entries_are_preserved(self):
        runner, passed = self.run_case('nonempty-release')
        self.assertTrue(passed, runner.summary.get('error'))
        release = runner.summary['native_commit_proofs']['host']['release']
        self.assertEqual((release['features_before'], release['features_after']), (1, 2))

    def test_completed_natural_work_has_no_new_clock_action(self):
        runner, passed = self.run_case('already-complete', 'complete')
        self.assertTrue(passed, runner.summary.get('error'))
        self.assertEqual(runner.calls, [])
        self.assertEqual(runner.scales, [])

    def test_profile_pauses_without_requiring_or_claiming_completion(self):
        runner, passed = self.run_case('profile-partial', 'profile')
        self.assertTrue(passed, runner.summary.get('error'))
        self.assertEqual(runner.scales, [9, 0])
        self.assertEqual(runner.calls, [])
        self.assertEqual(runner.row['work'], 1000)
        self.assertTrue(runner.summary['profile_confirmed'])
        self.assertNotIn('natural_work_confirmed', runner.summary)
        self.assertNotIn('commit_confirmed', runner.summary)
        for peer in driver.PEERS:
            self.assertEqual(runner.inspections.count(('natural-work-' + peer, peer)), 3)

    def test_profile_failure_preserves_cause_and_pauses_once(self):
        runner, passed = self.run_case('source-failure', 'profile')
        self.assertFalse(passed)
        self.assertIn('original Source capture failure', runner.summary['error']['cause'])
        self.assertEqual(runner.scales, [9, 0])

    def test_normal_profile_uses_original_normal_button_and_pauses_once(self):
        runner, passed = self.run_case('profile-partial', 'profile', profile_scale=1)
        self.assertTrue(passed, runner.summary.get('error'))
        self.assertEqual(runner.scales, [1, 0])
        self.assertTrue(runner.summary['profile_confirmed'])
        self.assertEqual(runner.calls, [])

    def test_normal_profile_failure_preserves_cause_and_pauses_once(self):
        runner, passed = self.run_case('source-failure', 'profile', profile_scale=1)
        self.assertFalse(passed)
        self.assertIn('original Source capture failure', runner.summary['error']['cause'])
        self.assertEqual(runner.scales, [1, 0])

    def test_normal_profile_does_not_replay_an_attempted_pause(self):
        runner, passed = self.run_case('pause-failure', 'profile', profile_scale=1)
        self.assertFalse(passed)
        self.assertEqual(runner.summary['error']['cause'], 'original pause failure')
        self.assertEqual(runner.scales, [1, 0])

    def test_profile_does_not_replay_an_attempted_pause(self):
        runner, passed = self.run_case('pause-failure', 'profile')
        self.assertFalse(passed)
        self.assertEqual(runner.summary['error']['cause'], 'original pause failure')
        self.assertEqual(runner.scales, [9, 0])

    def test_profile_rejects_an_idle_workload_and_pauses_once(self):
        runner, passed = self.run_case('profile-stalled', 'profile')
        self.assertFalse(passed)
        self.assertEqual(runner.summary['error']['cause'], 'Profile did not observe active Research work')
        self.assertEqual(runner.scales, [9, 0])

    def test_wrong_purchase_delta_stops_before_workers(self):
        runner, passed = self.run_case('wrong-price')
        self.assertFalse(passed)
        self.assertIn('one category3', runner.summary['error']['cause'])
        self.assertNotIn('client_bridges_staff_one', runner.calls)

    def test_duplicate_counter_is_not_success_or_replayed(self):
        runner, passed = self.run_case('duplicate')
        self.assertFalse(passed)
        self.assertIn('exactly one', runner.summary['error']['cause'])
        self.assertEqual(runner.calls.count('client_bridges_staff_one'), 1)
        self.assertEqual(runner.scales, [])

    def test_source_failure_keeps_cause_and_pauses_once(self):
        runner, passed = self.run_case('source-failure', 'complete')
        self.assertFalse(passed)
        self.assertIn('original Source capture failure', runner.summary['error']['cause'])
        self.assertEqual(runner.scales, [9, 0])
        self.assertEqual(runner.captures, [])

    def test_attempted_pause_is_not_replayed(self):
        runner, passed = self.run_case('pause-failure', 'complete')
        self.assertFalse(passed)
        self.assertEqual(runner.summary['error']['cause'], 'original pause failure')
        self.assertEqual(runner.scales, [9, 0])

    def test_old_release_feature_must_survive(self):
        runner, passed = self.run_case('lost-old-feature', 'commit')
        self.assertFalse(passed)
        self.assertIn('prior Startingpoint', runner.summary['error']['cause'])

    def test_final_physical_staff_is_checked(self):
        runner, passed = self.run_case('physical-staff', 'commit')
        self.assertFalse(passed)
        self.assertIn('physical staff', runner.summary['error']['cause'])

    def test_scan_activity_may_differ_without_changing_research(self):
        runner, passed = self.run_case('moving-stamps', 'commit')
        self.assertTrue(passed, runner.summary.get('error'))
        self.assertEqual(runner.calls.count('client_bridges_commit'), 1)

    def test_actual_project_and_release_differences_still_reject_before_input(self):
        for mode in ('different-project', 'different-release'):
            with self.subTest(mode=mode):
                runner, passed = self.run_case(mode, 'commit')
                self.assertFalse(passed)
                self.assertEqual(runner.calls, [])

    def test_local_original_commit_stamp_must_advance_once(self):
        runner, passed = self.run_case('wrong-local-stamp', 'commit')
        self.assertFalse(passed)
        self.assertIn('own scan stamp exactly once', runner.summary['error']['cause'])
        self.assertEqual(runner.calls.count('client_bridges_commit'), 1)

    def test_commit_preserves_actual_small_staff_census(self):
        runner, passed = self.run_case('small-staff', 'commit')
        self.assertTrue(passed, runner.summary.get('error'))

    def test_current_frame_is_checked_with_vip_and_marketing(self):
        runner, passed = self.run_case('current-frame', 'commit')
        self.assertTrue(passed, runner.summary.get('error'))

    def test_future_or_incomplete_extended_frame_rejects_before_input(self):
        for mode in ('future-frame', 'missing-marketing', 'unknown-marketing'):
            with self.subTest(mode=mode):
                runner, passed = self.run_case(mode, 'commit')
                self.assertFalse(passed)
                self.assertEqual(runner.calls, [])

    def test_equal_peer_staff_drift_still_rejects(self):
        runner, passed = self.run_case('both-staff-drift', 'commit')
        self.assertFalse(passed)
        self.assertIn('physical staff', runner.summary['error']['cause'])

    def test_independent_client_simulation_is_rejected_before_input(self):
        runner, passed = self.run_case('client-simulates')
        self.assertFalse(passed)
        self.assertEqual(runner.calls, [])

    def test_native_bug_delta_allows_original_target_skips_and_worsening(self):
        action = '00000000-0000-0000-0000-000000000002'
        original = [0, 'parent', 0, 'a' * 64, 0, 0, 0, 1, -1, 0, 0, 0.5, False, False, False]
        changed = copy.deepcopy(original)
        changed[9] = 1
        added = [0, 'parent', 1, action, 0, 0, 1, 1, -1, 0, 0, 0.5, False, False, False]
        self.assertEqual(driver.bug_delta([original], [changed, added], action),
                         {'before': 1, 'after': 2, 'added': 1, 'changed': 1, 'retired': 0})
        self.assertEqual(driver.bug_delta([original], [original], action)['added'], 0)

    def test_native_bug_delta_rejects_duplicate_or_foreign_births(self):
        action = '00000000-0000-0000-0000-000000000002'
        row = [0, 'parent', 1, action, 0, 0, 1, 1, -1, 0, 0, 0.5, False, False, False]
        with self.assertRaisesRegex(driver.AcceptanceError, 'duplicated'):
            driver.bug_delta([], [row, row], action)
        with self.assertRaisesRegex(driver.AcceptanceError, 'not bound'):
            driver.bug_delta([], [row], '00000000-0000-0000-0000-000000000003')

    def test_post_action_native_receipt_must_match_actual_effects(self):
        for mode, cause in [('map-receipt-wrong', 'map Bug counts'),
                            ('release-receipt-false', 'members not preserved'),
                            ('receipt-wrong-action', 'identity/confirmation')]:
            with self.subTest(mode=mode):
                runner, passed = self.run_case(mode, 'commit')
                self.assertFalse(passed)
                self.assertIn(cause, runner.summary['error']['cause'])
                self.assertEqual(runner.calls.count('client_bridges_commit'), 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)

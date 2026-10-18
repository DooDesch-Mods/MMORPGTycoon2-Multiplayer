"""Exercise the actual native wizard automation with controlled SDK replies."""
import collections
import copy
import importlib.util
from pathlib import Path
import sys
import unittest
import uuid
from unittest import mock

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    'production_wizard_automation', ROOT / 'tools/lobby_ui/wizard_automation.py')
wizard = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(wizard)


class Clock:
    def __init__(self):
        self.value = 0

    def __call__(self):
        value = self.value
        self.value += 1
        return value


class NativeWizard:
    """A native endpoint double; it records requests and exposes supplied state."""
    def __init__(self):
        self.page = 1
        self.icon = ''
        self.kind = ''
        self.requests = []
        self.overrides = {}
        self.old_page_reads = 0
        self.pending_old_page = None
        self.pending_old_reads = 0
        self.map_pending_reads = 0
        self.map_never_ready = False
        self.state_transform = lambda value: value
        self.disabled = collections.Counter()
        self.always_disabled = set()
        self.reason = 'original native generation still pending'
        self.affinity = {'base': [2, 3, 4, 5], 'allocation': [0, 0, 0, 0],
                         'displayed': [2, 3, 4, 5], 'remaining': 4}
        self.affinity_observations = 0
        self.affinity_observe_transform = lambda value: value
        self.affinity_mutation_transform = lambda value: value

    def reply_once(self, action, observe, value):
        self.overrides.setdefault((action, observe), collections.deque()).append(value)

    def mutations(self):
        return [item for item in self.requests
                if item['action'] != 'state' and item['arguments'].get('observe') is not True]

    def request(self, method, arguments):
        if method != 'desktop_activate' or not arguments['target'].startswith(wizard.TARGET):
            raise AssertionError('unexpected native operation')
        action = arguments['target'][len(wizard.TARGET):]
        observe = arguments.get('observe') is True
        self.requests.append({'action': action, 'page': self.page,
                              'arguments': copy.deepcopy(arguments)})
        replies = self.overrides.get((action, observe))
        if replies:
            value = replies.popleft()
            if isinstance(value, Exception):
                raise value
            return copy.deepcopy(value)
        if action == 'state':
            page = self.page
            if self.pending_old_reads:
                self.pending_old_reads -= 1
                page = self.pending_old_page
            ready = True
            if page == 6 and (self.map_never_ready or self.map_pending_reads):
                ready = False
                if self.map_pending_reads:
                    self.map_pending_reads -= 1
            value = {'status': 'observed', 'confirmed': True, 'native_attempted': False,
                     'page': page, 'page_id': wizard.PAGE_IDS[page - 1],
                     'map_ready': ready, 'cause': self.reason if not ready else ''}
            return self.state_transform(value)
        if action in ('select-icon', 'select-type'):
            field = 'icon_before' if action == 'select-icon' else 'type_before'
            value = self.icon if action == 'select-icon' else self.kind
            ordinal = 'icon_ordinal' if action == 'select-icon' else 'type_ordinal'
            if arguments.get(ordinal) != 0:
                raise AssertionError('selection must address ordinal zero')
            if observe:
                return {'guard_passed': True, 'native_attempted': False,
                        'result': 'observed', field: value}
            if action == 'select-icon':
                self.icon = 'native-icon-zero'
            else:
                self.kind = 'native-type-zero'
            return {'confirmed': True, 'native_attempted': True,
                    'native_returned': True, 'result': 'confirmed'}
        if action == 'allocate-affinity':
            if self.page != 3 or arguments.get('affinity_ordinal') != 0:
                raise AssertionError('affinity must address original page3 ordinal zero')
            before = copy.deepcopy(self.affinity)
            if observe:
                self.affinity_observations += 1
                return self.affinity_observe_transform({
                    'result': 'observed', 'guard_passed': True, 'native_attempted': False,
                    'confirmed': False, 'before': before,
                    'rows': [{'ordinal': i} for i in range(4)],
                    'next_disabled': int(before['remaining'] > 0)})
            token = arguments.get('affinity_token')
            if str(uuid.UUID(token)) != token:
                raise AssertionError('affinity token must be a canonical UUID')
            if arguments.get('expected_remaining') != before['remaining'] or before['remaining'] <= 0 \
                    or arguments.get('expected_values') != before['displayed']:
                raise AssertionError('affinity mutation must use the current positive remaining and displayed values')
            self.affinity['allocation'][0] += 1
            self.affinity['displayed'][0] += 1
            self.affinity['remaining'] -= 1
            return self.affinity_mutation_transform({
                'result': 'confirmed', 'native_attempted': True, 'native_returned': True,
                'confirmed': True, 'before': before, 'after': copy.deepcopy(self.affinity)})
        if action in ('next', 'create'):
            if observe:
                key = (action, self.page)
                disabled = key in self.always_disabled or self.disabled[key] > 0 \
                    or (action == 'next' and self.page == 3 and self.affinity['remaining'] > 0)
                if self.disabled[key] > 0:
                    self.disabled[key] -= 1
                return {'guard_passed': not disabled, 'native_attempted': False,
                        'cause': self.reason if disabled else '',
                        'guards': {'button_found': True, 'lookup_exception': 0,
                                   'owner_matches_window': True, 'disabled_read': True,
                                   'disabled': int(disabled), 'shown_read': True,
                                   'visible': True, 'command_matches': True}}
            if action == 'next':
                if self.page == 3 and self.affinity['remaining']:
                    raise AssertionError('original native Next is disabled until all affinity points are allocated')
                self.pending_old_page = self.page
                self.pending_old_reads = self.old_page_reads
                self.page += 1
            return {'native_attempted': True, 'native_returned': True, 'result': 'accepted'}
        raise AssertionError('unexpected action: ' + action)


class WizardAutomationTests(unittest.TestCase):
    def setUp(self):
        self.ui = NativeWizard()
        self.records = []
        self.captures = []
        self.checkpoints = []

    def complete(self, wait_seconds=20):
        # Any interactive prompt or real sleeping is a regression in this driver.
        with mock.patch('builtins.input', side_effect=AssertionError('stdin is forbidden')), \
                mock.patch.object(wizard.time, 'sleep', side_effect=AssertionError('real sleep is forbidden')):
            return wizard.complete_wizard(
                self.ui, record=lambda name, value: self.records.append((name, copy.deepcopy(value))),
                capture=self.captures.append, checkpoint=self.checkpoints.append,
                wait_seconds=wait_seconds, clock=Clock())

    def actions(self):
        return [item['action'] for item in self.ui.mutations()]

    def test_exact_once_selections_five_next_and_create(self):
        result = self.complete()
        self.assertEqual(result['result'], 'accepted')
        self.assertEqual(self.actions(), ['select-icon', 'next', 'select-type',
                                         'next'] + ['allocate-affinity'] * 4 + ['next', 'next', 'next', 'create'])
        self.assertEqual([item['page'] for item in self.ui.mutations() if item['action'] == 'next'], list(range(1, 6)))
        self.assertEqual(self.captures, ['native-wizard-page-' + str(page) for page in range(1, 4)]
                         + ['native-wizard-affinity-complete']
                         + ['native-wizard-page-' + str(page) for page in range(4, 7)])
        self.assertEqual(len(self.checkpoints), 12)
        for item in self.ui.mutations():
            index = self.ui.requests.index(item)
            self.assertTrue(any(previous['action'] == item['action'] and previous['page'] == item['page']
                                and previous['arguments'].get('observe') is True
                                for previous in self.ui.requests[:index]))

    def test_existing_native_selections_are_preserved(self):
        self.ui.icon = 'user-selected-icon'
        self.ui.kind = 'user-selected-world-type'
        self.complete()
        self.assertEqual(self.actions(), ['next'] * 2 + ['allocate-affinity'] * 4 + ['next'] * 3 + ['create'])
        self.assertEqual((self.ui.icon, self.ui.kind), ('user-selected-icon', 'user-selected-world-type'))
        self.assertIn('native-wizard-select-icon-preserved', [name for name, _ in self.records])
        self.assertIn('native-wizard-select-type-preserved', [name for name, _ in self.records])

    def test_old_page_observations_wait_for_real_next_without_replay(self):
        self.ui.old_page_reads = 2
        self.complete()
        self.assertEqual(self.actions().count('next'), 5)
        confirmed = [(name, value['page']) for name, value in self.records if name.endswith('-confirmed')]
        self.assertEqual([page for _, page in confirmed], list(range(1, 7)))
        self.assertGreater(sum(item['action'] == 'state' for item in self.ui.requests), 20)

    def test_wrong_or_malformed_initial_page_never_mutates(self):
        valid = {'status': 'observed', 'confirmed': True, 'native_attempted': False,
                 'page': 1, 'page_id': wizard.PAGE_IDS[0], 'map_ready': True}
        changes = ({'page': 2, 'page_id': wizard.PAGE_IDS[1]}, {'page': True}, {'page': 0},
                   {'page': 7}, {'page_id': 'foreign-page'}, {'map_ready': 1},
                   {'confirmed': False}, {'native_attempted': True}, {'status': 'unknown'})
        for changed in changes:
            with self.subTest(changed=changed):
                self.setUp()
                self.ui.reply_once('state', False, {**valid, **changed})
                with self.assertRaises(wizard.WizardAutomationError):
                    self.complete()
                self.assertEqual(self.actions(), [])
                self.assertEqual(self.captures, [])

    def test_unexpected_next_page_stops_after_one_next(self):
        def wrong_page(value):
            if value['page'] == 2:
                value.update(page=4, page_id=wizard.PAGE_IDS[3])
            return value
        self.ui.state_transform = wrong_page
        with self.assertRaisesRegex(wizard.WizardAutomationError, 'page changed unexpectedly'):
            self.complete()
        self.assertEqual(self.actions(), ['select-icon', 'next'])

    def test_unknown_mutation_receipts_are_never_replayed(self):
        for action in ('select-icon', 'select-type', 'allocate-affinity', 'next', 'create'):
            with self.subTest(action=action):
                self.setUp()
                self.ui.reply_once(action, False, {'confirmed': False, 'native_attempted': True,
                                                 'native_returned': False, 'result': 'unknown',
                                                 'cause': 'original uncertain native return'})
                with self.assertRaisesRegex(wizard.WizardAutomationError, 'no replay.*original uncertain native return'):
                    self.complete()
                self.assertEqual(self.actions().count(action), 1)
                self.assertEqual(self.actions()[-1], action)
                if action != 'create':
                    self.assertNotIn('original-create-once', self.checkpoints)

    def test_contradictory_confirmed_selection_is_rejected(self):
        for action in ('select-icon', 'select-type'):
            with self.subTest(action=action):
                self.setUp()
                self.ui.reply_once(action, False, {'confirmed': True, 'native_attempted': True,
                                                 'native_returned': True, 'result': 'unknown'})
                with self.assertRaisesRegex(wizard.WizardAutomationError, 'no replay'):
                    self.complete()
                self.assertEqual(self.actions().count(action), 1)
                self.assertEqual(self.actions()[-1], action)

    def test_selection_observation_must_be_readonly_and_observed(self):
        for changed in ({'result': 'unknown'}, {'native_attempted': True}, {'guard_passed': False}, {'icon_before': None}):
            with self.subTest(changed=changed):
                self.setUp()
                self.ui.reply_once('select-icon', True, {'guard_passed': True, 'native_attempted': False,
                                                       'result': 'observed', 'icon_before': '', **changed})
                with self.assertRaises(wizard.WizardAutomationError):
                    self.complete()
                self.assertEqual(self.actions(), [])

    def test_native_request_exception_propagates_without_retry(self):
        self.ui.reply_once('next', False, RuntimeError('native MCP timeout after submission'))
        with self.assertRaisesRegex(RuntimeError, 'native MCP timeout after submission'):
            self.complete()
        self.assertEqual(self.actions(), ['select-icon', 'next'])

    def test_disabled_button_waits_only_by_observation(self):
        self.ui.disabled[('next', 1)] = 2
        self.complete()
        next_mutation = next(i for i, item in enumerate(self.ui.requests)
                             if item['action'] == 'next' and not item['arguments'].get('observe'))
        observations = [item for item in self.ui.requests[:next_mutation] if item['action'] == 'next']
        self.assertEqual(len(observations), 3)
        self.assertTrue(all(item['arguments']['observe'] for item in observations))
        self.assertEqual(self.actions().count('next'), 5)
        self.assertEqual(sum(name == 'native-wizard-next-waiting-page-1' for name, _ in self.records), 1)

    def test_map_generation_waits_before_create_without_more_mutations(self):
        self.ui.map_pending_reads = 3
        self.complete()
        after_last_next = max(i for i, item in enumerate(self.ui.requests)
                              if item['action'] == 'next' and not item['arguments'].get('observe'))
        waiting = self.ui.requests[after_last_next + 1:-1]
        self.assertTrue(all(item['action'] == 'state' or item['arguments'].get('observe') is True for item in waiting))
        self.assertGreater(sum(item['action'] == 'create' for item in waiting), 1)
        self.assertEqual(self.actions()[-1], 'create')
        self.assertEqual(self.actions().count('create'), 1)

    def test_disabled_timeout_retains_original_guard_cause(self):
        self.ui.always_disabled.add(('next', 1))
        with self.assertRaises(wizard.WizardAutomationError) as failure:
            self.complete(wait_seconds=2)
        self.assertIn('button timed out', str(failure.exception))
        self.assertIn(self.ui.reason, str(failure.exception))
        self.assertIn('no mutation attempted', str(failure.exception))
        self.assertEqual(self.actions(), ['select-icon'])

    def test_map_timeout_retains_original_state_cause_without_create(self):
        self.ui.affinity['remaining'] = 0
        self.ui.map_never_ready = True
        with self.assertRaises(wizard.WizardAutomationError) as failure:
            self.complete(wait_seconds=2)
        self.assertIn('action=create page=6', str(failure.exception))
        self.assertIn(self.ui.reason, str(failure.exception))
        self.assertIn('"map_ready": false', str(failure.exception))
        self.assertNotIn('create', self.actions())
        self.assertEqual(self.actions().count('next'), 5)

    def test_old_page_timeout_does_not_repeat_next(self):
        self.ui.old_page_reads = 100
        with self.assertRaisesRegex(wizard.WizardAutomationError, 'Next not replayed'):
            self.complete(wait_seconds=2)
        self.assertEqual(self.actions(), ['select-icon', 'next'])

    def test_foreign_button_guard_is_not_treated_as_disabled_wait(self):
        self.ui.reply_once('next', True, {'native_attempted': False, 'guard_passed': False,
                                        'cause': 'foreign owner', 'guards': {'owner_matches_window': False}})
        with self.assertRaisesRegex(wizard.WizardAutomationError, 'rejected before execution.*foreign owner'):
            self.complete()
        self.assertEqual(self.actions(), ['select-icon'])

    def test_affinity_zero_remaining_preserves_existing_values(self):
        self.ui.affinity = {'base': [2, 3, 4, 5], 'allocation': [1, 2, 3, 4],
                            'displayed': [3, 5, 7, 9], 'remaining': 0}
        before = copy.deepcopy(self.ui.affinity)
        self.complete()
        self.assertEqual(self.ui.affinity, before)
        self.assertNotIn('allocate-affinity', self.actions())
        self.assertEqual(self.ui.affinity_observations, 1)
        self.assertIn('native-wizard-affinity-complete', self.captures)
        completed = next(value for name, value in self.records if name == 'native-wizard-affinity-complete')
        self.assertEqual(completed['allocated_points'], 0)

    def test_affinity_four_points_use_unique_tokens_and_fresh_expected_values(self):
        self.complete()
        increments = [item for item in self.ui.mutations() if item['action'] == 'allocate-affinity']
        self.assertEqual(len(increments), 4)
        tokens = [item['arguments']['affinity_token'] for item in increments]
        self.assertEqual(len(set(tokens)), 4)
        self.assertTrue(all(str(uuid.UUID(token)) == token and uuid.UUID(token).version == 4 for token in tokens))
        self.assertEqual([item['arguments']['expected_remaining'] for item in increments], [4, 3, 2, 1])
        self.assertEqual([item['arguments']['expected_values'] for item in increments],
                         [[2, 3, 4, 5], [3, 3, 4, 5], [4, 3, 4, 5], [5, 3, 4, 5]])
        self.assertEqual(self.ui.affinity, {'base': [2, 3, 4, 5], 'allocation': [4, 0, 0, 0],
                                          'displayed': [6, 3, 4, 5], 'remaining': 0})
        self.assertEqual(self.ui.affinity_observations, 5)
        last_mutation = -1
        for item in increments:
            index = self.ui.requests.index(item)
            intervening = self.ui.requests[last_mutation + 1:index]
            self.assertEqual(sum(request['action'] == 'allocate-affinity'
                                 and request['arguments'].get('observe') is True for request in intervening), 1)
            last_mutation = index

    def test_affinity_preserves_allocated_points_and_only_consumes_remainder(self):
        self.ui.affinity = {'base': [2, 3, 4, 5], 'allocation': [1, 2, 3, 4],
                            'displayed': [3, 5, 7, 9], 'remaining': 2}
        self.complete()
        self.assertEqual(self.actions().count('allocate-affinity'), 2)
        self.assertEqual(self.ui.affinity['allocation'], [3, 2, 3, 4])
        self.assertEqual(self.ui.affinity['displayed'], [5, 5, 7, 9])
        self.assertEqual(self.ui.affinity['base'], [2, 3, 4, 5])

    def test_affinity_unknown_after_one_confirmed_increment_never_retries(self):
        def uncertain(value):
            if value['before']['remaining'] == 3:
                value.update(result='unknown', confirmed=False, native_returned=False,
                             cause='original affinity increment return uncertain')
            return value
        self.ui.affinity_mutation_transform = uncertain
        with self.assertRaisesRegex(wizard.WizardAutomationError, 'no replay.*original affinity increment return uncertain'):
            self.complete()
        self.assertEqual(self.actions().count('allocate-affinity'), 2)
        self.assertEqual(self.actions()[-1], 'allocate-affinity')
        self.assertEqual(self.actions().count('next'), 2)

    def test_affinity_postreadback_must_match_exact_single_increment(self):
        for changed in ('remaining', 'selected', 'other', 'base', 'before', 'malformed'):
            with self.subTest(changed=changed):
                self.setUp()
                def corrupt(value):
                    after = value['after']
                    if changed == 'remaining':
                        after['remaining'] += 1
                    elif changed in ('selected', 'other'):
                        index = 0 if changed == 'selected' else 1
                        after['allocation'][index] += 1
                        after['displayed'][index] += 1
                    elif changed == 'base':
                        after['base'][0] += 1
                        after['displayed'][0] += 1
                    elif changed == 'before':
                        value['before']['remaining'] += 1
                    else:
                        after['remaining'] = True
                    return value
                self.ui.affinity_mutation_transform = corrupt
                with self.assertRaises(wizard.WizardAutomationError):
                    self.complete()
                self.assertEqual(self.actions().count('allocate-affinity'), 1)
                self.assertEqual(self.actions()[-1], 'allocate-affinity')

    def test_affinity_fresh_before_change_stops_before_another_increment(self):
        def concurrent(value):
            if self.ui.affinity_observations == 2:
                value['before']['allocation'][1] += 1
                value['before']['displayed'][1] += 1
            return value
        self.ui.affinity_observe_transform = concurrent
        with self.assertRaisesRegex(wizard.WizardAutomationError, 'changed after confirmed allocation'):
            self.complete()
        self.assertEqual(self.actions().count('allocate-affinity'), 1)
        self.assertEqual(self.actions().count('next'), 2)

    def test_affinity_malformed_observation_never_mutates(self):
        for changed in ('missing', 'short', 'bool', 'remaining', 'sum', 'guard', 'attempted', 'result'):
            with self.subTest(changed=changed):
                self.setUp()
                def malformed(value):
                    if changed == 'missing':
                        value.pop('before')
                    elif changed == 'short':
                        value['before']['base'].pop()
                    elif changed == 'bool':
                        value['before']['allocation'][0] = False
                    elif changed == 'remaining':
                        value['before']['remaining'] = -1
                    elif changed == 'sum':
                        value['before']['displayed'][0] += 1
                    elif changed == 'guard':
                        value['guard_passed'] = False
                    elif changed == 'attempted':
                        value['native_attempted'] = True
                    else:
                        value['result'] = 'unknown'
                    return value
                self.ui.affinity_observe_transform = malformed
                with self.assertRaises(wizard.WizardAutomationError):
                    self.complete()
                self.assertNotIn('allocate-affinity', self.actions())
                self.assertEqual(self.actions().count('next'), 2)

    def test_affinity_raw_extra_fields_do_not_change_confirmed_snapshot(self):
        def extra(value):
            value['before']['diagnostic_serial'] = self.ui.affinity_observations
            return value
        self.ui.affinity_observe_transform = extra
        self.complete()
        self.assertEqual(self.actions().count('allocate-affinity'), 4)

    def test_affinity_page_change_stops_before_native_increment(self):
        def changed(value):
            if value['page'] == 3 and self.ui.affinity_observations == 1:
                value.update(page=2, page_id=wizard.PAGE_IDS[1])
            return value
        self.ui.state_transform = changed
        with self.assertRaisesRegex(wizard.WizardAutomationError, 'allocation left its original page'):
            self.complete()
        self.assertEqual(self.actions().count('allocate-affinity'), 1)
        self.assertEqual(self.actions().count('next'), 2)


if __name__ == '__main__':
    unittest.main(verbosity=2)

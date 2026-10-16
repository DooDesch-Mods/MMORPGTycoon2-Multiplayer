import copy
import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace
import unittest

TREE = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('repair_driver', TREE / 'tools/native_bug_repair_acceptance.py')
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)
ACTUAL = json.loads((Path(__file__).parent / 'fixtures/bug_repair_assigned.json').read_bytes())


class Runner:
    def __init__(self, mode='pass'):
        self.mode, self.complete = mode, False
        self.scales, self.captures = [], []
        self.summary = {'step': 'test', 'passed': False}

    def persist(self):
        pass

    def current(self, peer):
        value = copy.deepcopy(ACTUAL['peers'][peer])
        controller, native = value['engine']['typed_actions'], value['native']
        frame = controller['native_economy']['day']['frame']
        value['frame'] = frame
        if self.mode == 'bad-baseline': native['jobs']['jobs'][0]['developer_reciprocal'] = False
        if self.mode in ('loaded-assigned', 'idle-assigned'):
            state = 1 if self.mode == 'loaded-assigned' else 0
            native['developers'][0]['state'] = state
            frame['developers']['rows'][0][1] = state
        if self.complete:
            if self.mode == 'source-failure': controller['live_state']['cause'] = 'original Subscriber capture changed state'
            if self.mode != 'bound':
                frame['bugs'] = []
                w = frame['developers']['construction']
                w['bug_jobs'] = []
                w['bug_parents'][0][2] = 2
                native['jobs']['queue_count'] = 0
                native['jobs']['jobs'] = []
                for d in native['jobs']['developers']: d.update(job_present=False, reciprocal=False, job_index=-1)
                for d in native['developers']: d.update(state=0, job_link=False, on_map=False)
                value['ui']['known_bugs'] = []
                if self.mode == 'remaining-tracker': value['ui'] = copy.deepcopy(ACTUAL['peers'][peer]['ui'])
                if self.mode == 'ledger' and peer == 'client-1': native['ledger']['cash'] += 1
        return value

    def inspect(self, label, peer):
        if label.startswith('natural-'): self.complete = True
        return self.current(peer)['engine']

    def single(self, label, peer, tool):
        return self.current(peer)['native' if tool == 'game_read_native_developers' else 'ui']

    def clock(self, _, scale, timeout):
        self.scales.append(scale)
        row = {'fields': {'scale': scale}, 'state': 'confirmed'}
        self.summary.setdefault('clock_intents', []).append(row)
        if self.mode == 'start-readback' and scale:
            row['state'] = 'receipt-confirmed'
            raise driver.AcceptanceError('original start readback failure')
        if self.mode == 'pause-failure' and not scale:
            row['state'] = 'attempted'
            raise driver.AcceptanceError('original pause failure')

    def capture(self, peer):
        self.captures.append(peer)


class Tests(unittest.TestCase):
    def run_case(self, mode):
        r = Runner(mode)
        ticks = iter(range(0, 10000, 10))
        result = driver.execute(r, SimpleNamespace(scale=9, wait_seconds=25), clock=r.clock, now=lambda: next(ticks))
        return r, result

    def test_actual_assigned_baseline_and_unchanged_counter_completion(self):
        r, passed = self.run_case('pass')
        self.assertTrue(passed, r.summary.get('error'))
        self.assertEqual(r.scales, [9, 0])
        self.assertEqual(r.captures, ['host', 'client-1'])
        self.assertEqual(r.summary['result']['job_fixed_before'], r.summary['result']['job_fixed_after'])
        self.assertEqual(r.summary['selected']['local_ids'], {'host': 1, 'client-1': 0})

    def test_wrong_reciprocal_baseline_has_no_mutation(self):
        r, passed = self.run_case('bad-baseline')
        self.assertFalse(passed)
        self.assertEqual(r.scales, [])

    def test_loaded_developer_can_resume_going_to_assigned_job(self):
        r, passed = self.run_case('loaded-assigned')
        self.assertTrue(passed, r.summary.get('error'))
        self.assertEqual(r.scales, [9, 0])
        self.assertEqual(r.captures, ['host', 'client-1'])

    def test_idle_developer_cannot_start_assigned_repair_test(self):
        r, passed = self.run_case('idle-assigned')
        self.assertFalse(passed)
        self.assertIn('not assigned to the repair', r.summary['error']['cause'])
        self.assertEqual(r.scales, [])

    def test_actual_source_failure_preserved_and_pause_once(self):
        r, passed = self.run_case('source-failure')
        self.assertFalse(passed)
        self.assertIn('original Subscriber capture changed state', r.summary['error']['cause'])
        self.assertEqual(r.scales, [9, 0])
        self.assertEqual(r.captures, [])

    def test_incomplete_bound_still_fails(self):
        r, passed = self.run_case('bound')
        self.assertFalse(passed)
        self.assertIn('bound reached', r.summary['error']['cause'])
        self.assertEqual(r.scales, [9, 0])

    def test_confirmed_start_readback_failure_does_not_replay_start(self):
        r, passed = self.run_case('start-readback')
        self.assertFalse(passed)
        self.assertEqual(r.scales, [9, 0])
        self.assertEqual(r.summary['error']['cause'], 'original start readback failure')

    def test_attempted_pause_failure_does_not_replay_pause(self):
        r, passed = self.run_case('pause-failure')
        self.assertFalse(passed)
        self.assertEqual(r.scales, [9, 0])
        self.assertEqual(r.summary['error']['cause'], 'original pause failure')

    def test_actual_final_tracker_and_full_ledger_required(self):
        for mode, cause in [('remaining-tracker', 'actual Tracker'), ('ledger', 'ledger differs')]:
            with self.subTest(mode=mode):
                r, passed = self.run_case(mode)
                self.assertFalse(passed)
                self.assertIn(cause, r.summary['error']['cause'])
                self.assertEqual(r.captures, [])


if __name__ == '__main__':
    unittest.main(verbosity=2)

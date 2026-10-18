"""Reject acceptance claims without original selected cycles or real suppression."""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from subscriber_needs_acceptance import cycles_reason, candidate_ids
from local_acceptance import AcceptanceError


class NeedsAcceptanceChecks(unittest.TestCase):
    def setUp(self):
        self.before = {'ticks': [{'entered': 0, 'forwarded': 0, 'guard_rejected': 0} for _ in range(2)],
                       'writes': [{'leaf': str(i), 'entered': 0, 'forwarded': 0, 'suppressed': 0,
                                   'guard_rejected': 0, 'wrong_thread': 0} for i in range(4)]}
        self.after = copy.deepcopy(self.before)
        for tick in self.after['ticks']:
            tick.update(entered=2, forwarded=2)
        self.after['writes'][0].update(entered=9, forwarded=7, suppressed=2)

    def test_two_original_cycles_and_accounted_suppression_pass(self):
        self.assertIsNone(cycles_reason(self.before, self.after))

    def test_entered_without_original_return_fails(self):
        self.after['ticks'][1]['forwarded'] = 1
        with self.assertRaises(AcceptanceError):
            cycles_reason(self.before, self.after)

    def test_no_selected_update_is_an_observation_wait(self):
        self.after['ticks'][1].update(entered=0, forwarded=0)
        self.assertIn('update_needs', cycles_reason(self.before, self.after))

    def test_unaccounted_write_fails(self):
        self.after['writes'][0]['entered'] = 10
        with self.assertRaises(AcceptanceError):
            cycles_reason(self.before, self.after)

    def test_sparse_candidate_ids_remain_exact(self):
        value={'existing_count':100,'scanned_count':17,'eligible_count':2,
               'scan_complete':False,'serialized_ids':[7,2147483647]}
        self.assertEqual(candidate_ids(value),[7,2147483647])

    def test_candidate_receipt_rejects_duplicate_and_false_completeness(self):
        for ids, complete in (([7,7],False),([7,17],True),([True,17],False)):
            with self.assertRaises(AcceptanceError):
                candidate_ids({'existing_count':100,'scanned_count':17,'eligible_count':2,
                               'scan_complete':complete,'serialized_ids':ids})


if __name__ == '__main__':
    unittest.main()

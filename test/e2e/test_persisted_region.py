"""Persistence keeps all selected native content while classifying transient RNG."""
import copy
import unittest

from tools.e2e.persisted_region import persisted_region_state
from local_acceptance import AcceptanceError


def region():
    return {'id': 67, 'preset': 'Custom', 'styles': ['Field'] * 4, 'random': [1, 2, 3, 4],
            'type_random': [1, 2, 3, 4], 'scenery_count': 19, 'scenery_sha256': 'a' * 64,
            'grid_sha256': 'b' * 64, 'name': 'Stormlong Woods', 'type': 1, 'owned': True,
            'status': 1, 'level': 1, 'max_level': 1}


class PersistedRegionTests(unittest.TestCase):
    def test_only_constructor_rng_can_differ(self):
        before, after = region(), region()
        after.update(random=[5, 6, 7, 8], type_random=[5, 6, 7, 8])
        self.assertEqual(persisted_region_state(before), persisted_region_state(after))
        self.assertEqual(before, region())
        self.assertEqual(after['random'], [5, 6, 7, 8])

    def test_every_selected_persisted_field_still_detects_changes(self):
        before = region()
        for key, value in before.items():
            if key in ('random', 'type_random'):
                continue
            after = copy.deepcopy(before)
            after[key] = not value if isinstance(value, bool) else value + 1 if isinstance(value, int) else ['Forest'] * 4 if isinstance(value, list) else value + 'changed'
            self.assertNotEqual(persisted_region_state(before), persisted_region_state(after), key)

    def test_incomplete_native_readback_is_never_accepted(self):
        for key in region():
            incomplete = region()
            del incomplete[key]
            with self.assertRaises(AcceptanceError, msg=key):
                persisted_region_state(incomplete)


if __name__ == '__main__':
    unittest.main()

"""A loaded world does not prove that all native building reads passed."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from large_save_acceptance import building_readbacks
from local_acceptance import AcceptanceError


class BuildingReadbacks(unittest.TestCase):
    def test_complete_native_readbacks_have_a_named_receipt(self):
        value={'state':'confirmed','error':'','building_count':625,'references':625,'rejected_buildings':0,
               'snapshot_id':'native-snapshot','snapshot_sha256':'source-hash'}
        self.assertEqual(building_readbacks({'load_identity':value})['building_count'],625)

    def test_active_world_does_not_override_rejections_or_missing_buildings(self):
        valid={'state':'confirmed','error':'','building_count':625,'references':625,'rejected_buildings':0,
               'snapshot_id':'native-snapshot','snapshot_sha256':'source-hash'}
        for field, value in (('state','failed'),('error','outer_shard_mismatch'),('building_count',596),
                             ('references',4096),('rejected_buildings',29),('rejected_buildings',False)):
            with self.subTest(field=field, value=value), self.assertRaises(AcceptanceError):
                building_readbacks({'in_game':True,'load_identity':dict(valid,**{field:value})})


if __name__=='__main__':
    unittest.main()

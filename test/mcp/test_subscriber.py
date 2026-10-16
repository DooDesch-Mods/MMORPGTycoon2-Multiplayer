"""Check the read-only subscriber contract through SDK stdio and mailbox receipts."""
from unittest.mock import Mock
import unittest
import test_stdio_region as region_tests
from server import dispatch
from runtime import RuntimeErrorDetail
from runtime import MUTATIONS

class SubscriberContract(unittest.TestCase):
    def test_failed_catalog_preserves_native_partial_counts(self):
        helper=region_tests.RegionStdioTests()
        helper.check_stdio("read_building_catalog",{},(),
            {"scan_complete":False,"root":{"typed_valid":227,"rejected":1},"first_error":"native_uid_pointer_mismatch"},
            native_error={"code":"BUILDING_CATALOG_REJECTED","message":"native_uid_pointer_mismatch"})

    def test_readonly_building_catalog_sdk_contract(self):
        helper=region_tests.RegionStdioTests()
        helper.check_stdio("read_building_catalog",{},({"uid":1},{"limit":1000000},{"scope":"all"}),
            {"shared":False,"scope":"root_main_building_arrays","scan_complete":True,
             "root":{"typed_valid":227},"dungeon_template":{"observed":False},"dungeon_instance":{"observed":False}})

    def test_readonly_time_ui_sdk_contract(self):
        helper=region_tests.RegionStdioTests()
        helper.check_stdio("read_time_ui",{},({"scale":9},{"epoch":1},{"script":"x"}),
            {"epoch":18446744073709551615,"capture_enabled":False,"host_state_available":False,
             "items":[{"scale":scale,"native_state":state,"active":state==0}
                      for scale,state in ((0,0),(1,1),(3,1),(9,1))]})

    def test_read_schema_and_matching_acknowledgement(self):
        helper=region_tests.RegionStdioTests()
        invalid=({}, {"serialized_id":True},{"serialized_id":-1},{"serialized_id":2147483648},
                 {"serialized_id":0.0},{"serialized_id":"0"},{"serialized_id":0,"extra":1})
        helper.check_stdio("read_subscriber_needs",{"serialized_id":0},invalid,
            {"serialized_id":0,"subscriber_count":129178,"satisfaction":0,"addiction":2,"logged_in_time":30.5,"needs":[0]*7})

    def test_weapon_read_schema_and_matching_acknowledgement(self):
        self.assertNotIn("read_subscriber_weapon",MUTATIONS)
        helper=region_tests.RegionStdioTests()
        invalid=({}, {"serialized_id":True},{"serialized_id":-1},{"serialized_id":2147483648},
                 {"serialized_id":0.0},{"serialized_id":"0"},{"serialized_id":0,"extra":1})
        helper.check_stdio("read_subscriber_weapon",{"serialized_id":60},invalid,
            {"serialized_id":60,"read_only":True,"attempted":False,"actor_present":True,
             "weapon_ilvl":13,"weapon_category":"swords","weapon_variant_id":1889777055})

    def test_weapon_read_preserves_native_failure(self):
        helper=region_tests.RegionStdioTests()
        helper.check_stdio("read_subscriber_weapon",{"serialized_id":60},(),
            {"serialized_id":60,"read_only":True,"attempted":False,"observed":False},
            native_error={"code":"SUBSCRIBER_WEAPON_READ_REJECTED","message":"actual Toon owner changed"})

    def test_exit_rejects_extra_arguments_before_native_request(self):
        runtime=Mock()
        with self.assertRaises(RuntimeErrorDetail):
            dispatch(runtime,"game_exit_game",{"force":True})
        runtime.request.assert_not_called()

    def test_readonly_network_sdk_contract(self):
        helper=region_tests.RegionStdioTests()
        helper.check_stdio("read_network_state",{},({"node_id":1},{"script":"x"}),
                           {"total_capacity":5600,"serialized_ids_available":False,"nodes":[],"regions":[]})

    def test_readonly_candidates_preserve_sparse_native_ids(self):
        helper=region_tests.RegionStdioTests()
        helper.check_stdio("read_subscriber_candidates",{},({"start_id":0},{"limit":1000000}),
                           {"existing_count":129178,"scanned_count":42,"eligible_count":2,
                            "scan_complete":False,"serialized_ids":[17,2147483647]})

    def test_selected_replica_sdk_contracts(self):
        helper=region_tests.RegionStdioTests()
        epoch=18446744073709551615
        cases=(
            ("arm_subscriber_needs",{"epoch":epoch,"serialized_id":7,"expected_initial":[0]*7}),
            ("apply_subscriber_needs",{"epoch":epoch,"serialized_id":7,"revision":1,"expected_before":[0]*7,"host_needs":[1]*7}),
            ("disarm_subscriber_needs",{"epoch":epoch}),
        )
        for operation,args in cases:
            invalid=[{},dict(args,epoch=True),dict(args,epoch=1.0),dict(args,epoch=0),dict(args,epoch=epoch+1),dict(args,extra=0)]
            if "serialized_id" in args:
                invalid.extend((dict(args,serialized_id=True),dict(args,serialized_id=-1)))
            for key in set(args)&{"expected_initial","expected_before","host_needs"}:
                invalid.extend((dict(args,**{key:[0]*6}),dict(args,**{key:[True]*7}),dict(args,**{key:[101]*7})))
            helper.check_stdio(operation,args,tuple(invalid),{"state":"matching_native_ack","epoch":epoch})

    def test_invalid_replica_values_never_reach_mailbox(self):
        base={"epoch":1,"serialized_id":0,"revision":1,"expected_before":[0]*7,"host_needs":[1]*7}
        for invalid in (dict(base,revision=True),dict(base,revision=1.0),dict(base,host_needs=[float("nan")]*7),
                        dict(base,host_needs=[float("inf")]*7),dict(base,host_needs=[10**1000]*7)):
            runtime=Mock()
            with self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_apply_subscriber_needs",invalid)
            runtime.request.assert_not_called()

if __name__=="__main__": unittest.main()

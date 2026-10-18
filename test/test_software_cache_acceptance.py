import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from software_cache_acceptance import replica_reason


def fixture():
    host={"in_game":True,"session_connected":True,"clock":{"scale":0},"software":{"rating":700,"fun":40}}
    client=copy.deepcopy(host)
    client["software"].update(replica_armed=True,replica_bound=True,replica_error="",applied_revision=1,bound_epoch=123,
        initial_rating=0,initial_fun=0,calculations=[{"entered":3,"forwarded":0,"suppressed":3,"guard_rejected":0} for _ in range(2)],
        ticks=[{}, {"entered":3,"forwarded":3}])
    baseline=copy.deepcopy(client["software"])
    for leaf in baseline["calculations"]:
        leaf["entered"]=1
        leaf["suppressed"]=1
    baseline["ticks"][1]={"entered":1,"forwarded":1}
    return host,client,baseline


class ProofGuards(unittest.TestCase):
    def test_real_mutation_and_forwarded_ticks(self):
        self.assertIsNone(replica_reason(*fixture()))

    def test_idempotent_pair_does_not_prove_mutation(self):
        host,client,baseline=fixture()
        client["software"]["initial_rating"]=700
        self.assertIn("zero pair",replica_reason(host,client,baseline))

    def test_native_ticker_must_continue(self):
        host,client,baseline=fixture()
        client["software"]["ticks"][1]["forwarded"]=2
        self.assertIn("ticker",replica_reason(host,client,baseline))

    def test_native_calculation_must_not_escape(self):
        host,client,baseline=fixture()
        client["software"]["calculations"][0]["forwarded"]=1
        self.assertIn("forwarded",replica_reason(host,client,baseline))

    def test_matching_host_and_live_session_required(self):
        host,client,baseline=fixture()
        client["software"]["fun"]=39
        self.assertIn("differs",replica_reason(host,client,baseline))
        client["software"]["fun"]=40
        client["session_connected"]=False
        self.assertIn("connected",replica_reason(host,client,baseline))

    def test_two_cycles_must_be_observed(self):
        host,client,baseline=fixture()
        client["software"]["calculations"][1]["suppressed"]=2
        self.assertIn("two suppressed",replica_reason(host,client,baseline))


if __name__=="__main__":
    unittest.main()

from pathlib import Path
import importlib.util
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mcp'))
import runtime
spec=importlib.util.spec_from_file_location('feature_events_server',ROOT/'tools/mcp/server.py')
server=importlib.util.module_from_spec(spec);sys.modules[spec.name]=server;spec.loader.exec_module(server)
class Fake:
    def __init__(self):self.calls=[]
    def available(self):return {'read_native_rules_feature_events'}
    def request(self,op,args):self.calls.append((op,args));return {'read_only':True,'event_dispatched':False,'features_ordered_eligible':False}
class FeatureEvents(unittest.TestCase):
    def test_fixed_read_only_dispatch(self):
        fake=Fake();result=server.dispatch(fake,'game_read_native_rules_feature_events',{})
        self.assertEqual(fake.calls,[('read_native_rules_feature_events',{})])
        self.assertTrue(result['read_only']);self.assertFalse(result['event_dispatched']);self.assertFalse(result['features_ordered_eligible'])
        tool=server.tools_for(fake)[0]
        self.assertTrue(tool.annotations.readOnlyHint)
        self.assertFalse(tool.annotations.destructiveHint)
        self.assertNotIn('read_native_rules_feature_events',runtime.MUTATIONS)
    def test_arguments_never_dispatch(self):
        for args in ({'command':'Read'},{'event':59},{'enabled':True},{'address':123}):
            fake=Fake()
            with self.subTest(args=args),self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(fake,'game_read_native_rules_feature_events',args)
            self.assertEqual(fake.calls,[])
    def test_native_failure_remains_visible(self):
        class Failed(Fake):
            def request(self,op,args):raise runtime.RuntimeErrorDetail('native_error','Feature event59 recipient count differs from native list')
        with self.assertRaisesRegex(runtime.RuntimeErrorDetail,'count differs'):server.dispatch(Failed(),'game_read_native_rules_feature_events',{})
if __name__=='__main__':unittest.main()

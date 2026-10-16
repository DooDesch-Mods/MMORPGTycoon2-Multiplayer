"""Exercise fixed policy SDK descriptors and dispatch without any native runtime."""
from pathlib import Path
import importlib.util
import os
import sys
import unittest

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
CANONICAL=Path(os.environ.get("MT2MP_CANONICAL_ROOT",str(ROOT)))
sys.path.insert(0,str(CANONICAL/"tools/mcp"))
sys.path.insert(0,str(ROOT/"tools/mcp"))
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec)
    sys.modules[name]=module
    spec.loader.exec_module(module)
    return module
runtime=load("runtime",ROOT/"tools/mcp/runtime.py")
server=load("bug_policy_test_server",ROOT/"tools/mcp/server.py")
class NoNative:
    def __init__(self):self.calls=[]
    def available(self):return {"read_bug_c_policy","native_bug_c_policy"}
    def request(self,op,args):self.calls.append((op,args));return {"unknown":True,"cause":"retained original cause"}
def args():
    return {"command":"Toggle","scope":"online","token":"11111111-1111-4111-8111-111111111111",
            "native_origin_uuid":"22222222-2222-4222-8222-222222222222","epoch":7,"generation":9,"expected":False}
class SDK(unittest.TestCase):
    def test_only_fixed_original_routes_dispatch_once(self):
        for command in ("Show","Toggle"):
            for scope in ("online","offline"):
                with self.subTest(command=command,scope=scope):
                    fake=NoNative();request={**args(),"command":command,"scope":scope}
                    result=server.dispatch(fake,"game_native_bug_c_policy",request)
                    self.assertEqual(fake.calls,[("native_bug_c_policy",request)])
                    self.assertEqual(result,{"unknown":True,"cause":"retained original cause"})
    def test_read_requires_empty_arguments(self):
        fake=NoNative();server.dispatch(fake,"game_read_bug_c_policy",{})
        self.assertEqual(fake.calls,[("read_bug_c_policy",{})])
        with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(fake,"game_read_bug_c_policy",{"scope":"online"})
        self.assertEqual(len(fake.calls),1)
    def test_bad_fields_and_values_never_reach_native(self):
        bad=[("command","SetKnown"),("command",[]),("scope","all"),("scope",True),
             ("expected",0),("expected","false"),("epoch",True),("epoch",0),("epoch",-1),("epoch",2**64),
             ("generation",1.5),("generation",False),("token","AAAAAAAA-1111-4111-8111-111111111111"),("native_origin_uuid","bad")]
        values=[{**args(),k:v} for k,v in bad]+[{**args(),"extra":1}]+[{k:v for k,v in args().items() if k!=missing} for missing in args()]
        for request in values:
            with self.subTest(request=request):
                fake=NoNative()
                with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(fake,"game_native_bug_c_policy",request)
                self.assertEqual(fake.calls,[])
    def test_uint64_maximum_and_true_expected(self):
        fake=NoNative();request={**args(),"epoch":2**64-1,"generation":2**64-1,"expected":True}
        server.dispatch(fake,"game_native_bug_c_policy",request);self.assertEqual(len(fake.calls),1)
    def test_actual_descriptors_keep_mutation_visible(self):
        tools={tool.name:tool for tool in server.tools_for(NoNative())}
        read=tools["game_read_bug_c_policy"];change=tools["game_native_bug_c_policy"]
        self.assertTrue(read.annotations.readOnlyHint)
        self.assertFalse(change.annotations.readOnlyHint)
        self.assertTrue(change.annotations.destructiveHint)
        self.assertFalse(change.annotations.idempotentHint)
        self.assertEqual(change.inputSchema["properties"]["command"]["enum"],["Show","Toggle"])
        self.assertEqual(change.inputSchema["properties"]["scope"]["enum"],["online","offline"])
    def test_native_error_preserves_original_cause(self):
        class Failed(NoNative):
            def request(self,op,args):raise runtime.RuntimeErrorDetail("native_error","original checkbox Release SEH=5")
        with self.assertRaisesRegex(runtime.RuntimeErrorDetail,"original checkbox Release SEH=5"):
            server.dispatch(Failed(),"game_native_bug_c_policy",args())
if __name__=="__main__":unittest.main(verbosity=2)

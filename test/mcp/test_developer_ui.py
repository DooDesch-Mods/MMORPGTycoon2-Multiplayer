"""Exercise the actual fixed live Developer UI SDK dispatch without native calls."""
from pathlib import Path
import importlib.util
import sys
import unittest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in [HERE,*HERE.parents] if (p/'tools/mcp/runtime.py').is_file())
SDK=HERE/'candidate/tools/mcp'
if not SDK.is_dir():SDK=ROOT/'tools/mcp'
sys.path.insert(0,str(ROOT/'tools/mcp'));sys.path.insert(0,str(SDK))
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);sys.modules[name]=module;spec.loader.exec_module(module);return module
runtime=load('runtime',SDK/'runtime.py')
server=load('developer_live_ui_test_server',ROOT/'tools/mcp/server.py')
class NoNative:
    def __init__(self):self.calls=[]
    def available(self):return {'native_developer_ui'}
    def request(self,op,args):self.calls.append((op,args));return {'admission_only':True,'execution_ack':False,'captured':True,'action_uuid':'33333333-3333-4333-8333-333333333333'}
def args():return {'command':'Hire','token':'11111111-1111-4111-8111-111111111111','native_origin_uuid':'22222222-2222-4222-8222-222222222222','epoch':7,'generation':9,'expected_count':1}
class LiveDeveloperUI(unittest.TestCase):
    def test_exact_original_commands_dispatch_once_without_ack_default(self):
        for command in ('Show','Hire','Fire'):
            with self.subTest(command=command):
                fake=NoNative();request=args();request['command']=command
                result=server.dispatch(fake,'game_native_developer_ui',request)
                self.assertEqual(fake.calls,[('native_developer_ui',request)])
                self.assertTrue(result['admission_only']);self.assertFalse(result['execution_ack'])
                self.assertNotIn('confirmed',result)
    def test_bad_origin_count_command_and_extra_fields_never_dispatch(self):
        variants=[{}, {**args(),'extra':1}]
        for key,value in [('command','Other'),('command',[]),('token','bad'),('native_origin_uuid','A'*36),('epoch',True),('epoch',0),('generation',2**64),('expected_count',True),('expected_count',-1),('expected_count',51)]:variants.append({**args(),key:value})
        for request in variants:
            with self.subTest(request=request):
                fake=NoNative()
                with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(fake,'game_native_developer_ui',request)
                self.assertEqual(fake.calls,[])
    def test_real_descriptor_is_mutation_and_fixed_enum(self):
        tool=next(t for t in server.tools_for(NoNative()) if t.name=='game_native_developer_ui')
        self.assertFalse(tool.annotations.readOnlyHint);self.assertTrue(tool.annotations.destructiveHint);self.assertFalse(tool.annotations.idempotentHint)
        self.assertEqual(tool.inputSchema['properties']['command']['enum'],['Show','Hire','Fire'])
        self.assertIn('native_developer_ui',runtime.MUTATIONS)
    def test_original_unknown_cause_is_never_success_shaped(self):
        class Failed(NoNative):
            def request(self,op,request):raise runtime.RuntimeErrorDetail('native_error','original UI entry cleanup Unknown')
        with self.assertRaisesRegex(runtime.RuntimeErrorDetail,'original UI entry cleanup Unknown'):server.dispatch(Failed(),'game_native_developer_ui',args())
if __name__=='__main__':unittest.main(verbosity=2)
